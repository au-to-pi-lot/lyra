#include "value_hash.h"
#include "hash.h"
#include "types/champ.h"
#include "types/list.h"
#include <assert.h>
#include <string.h>

// Type tags. Fixed numbers rather than ValueType values, so reordering the
// enum doesn't change hashes.
enum {
    HASH_TAG_LIST = 1,
    HASH_TAG_INT = 2,
    HASH_TAG_BOOLEAN = 3,
    HASH_TAG_STRING = 4,
    HASH_TAG_SYMBOL = 5,
    HASH_TAG_DICT = 6,
    HASH_TAG_SET = 7,
};

static void hash_into(Hasher *h, Value *v);

typedef struct EntrySum {
    uint64_t sum;
} EntrySum;

static void add_entry_hash(void *ctx, Value *key, Value *value) {
    Hasher h;
    hasher_init_process(&h);
    hash_into(&h, key);
    if (value) hash_into(&h, value);
    ((EntrySum *)ctx)->sum += hasher_finish(&h);
}

static void hash_into(Hasher *h, Value *v) {
    switch (v->type) {
        case CONS: {
            hasher_write_u8(h, HASH_TAG_LIST);
            hasher_write_u64(h, (uint64_t)list_length(v));
            for (Value *rest = v; !is_nil(rest); rest = list_tail(rest)) {
                hash_into(h, list_head(rest));
            }
            break;
        }
        case INT:
            hasher_write_u8(h, HASH_TAG_INT);
            hasher_write_u64(h, (uint64_t)(int64_t)v->data.as_int);
            break;
        case BOOLEAN:
            hasher_write_u8(h, HASH_TAG_BOOLEAN);
            hasher_write_u8(h, v->data.as_boolean ? 1 : 0);
            break;
        case STRING: {
            const char *body = utstring_body(v->data.as_string);
            hasher_write_u8(h, HASH_TAG_STRING);
            hasher_write(h, body, strlen(body));
            hasher_write_u8(h, 0xff);
            break;
        }
        case SYMBOL: {
            UT_string *key = v->data.as_symbol;
            hasher_write_u8(h, HASH_TAG_SYMBOL);
            hasher_write_u64(h, (uint64_t)utstring_len(key));
            hasher_write(h, utstring_body(key), utstring_len(key));
            break;
        }
        case DICT:
        case SET: {
            EntrySum acc = {0};
            champ_foreach(v->data.as_champ, add_entry_hash, &acc);
            hasher_write_u8(h, v->type == DICT ? HASH_TAG_DICT : HASH_TAG_SET);
            hasher_write_u64(h, (uint64_t)v->data.as_champ->size);
            hasher_write_u64(h, acc.sum);
            break;
        }
        default:
            assert(!"value_hash of an unhashable value");
            break;
    }
}

uint64_t value_hash(Value *v) {
    Hasher h;
    hasher_init_process(&h);
    hash_into(&h, v);
    return hasher_finish(&h);
}

typedef struct FirstUnhashable {
    Value *found;
} FirstUnhashable;

static void check_dict_value(void *ctx, Value *key, Value *value) {
    (void)key;
    FirstUnhashable *acc = ctx;
    if (!acc->found) acc->found = value_unhashable_part(value);
}

Value *value_unhashable_part(Value *v) {
    switch (v->type) {
        case INT:
        case BOOLEAN:
        case STRING:
        case SYMBOL:
        case SET:  // members were checked when they were added
            return NULL;
        case CONS:
            for (Value *rest = v; !is_nil(rest); rest = list_tail(rest)) {
                Value *bad = value_unhashable_part(list_head(rest));
                if (bad) return bad;
            }
            return NULL;
        case DICT: {
            // Keys were checked when they were added; values weren't.
            FirstUnhashable acc = {NULL};
            champ_foreach(v->data.as_champ, check_dict_value, &acc);
            return acc.found;
        }
        default:  // FLOAT, FUNCTION, MACRO, VARIADIC_MARKER
            return v;
    }
}
