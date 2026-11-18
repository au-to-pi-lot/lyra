// Intrinsics for the persistent DICT and SET types (types/champ.h).
// Every operation returns a new dict or set and leaves its arguments as
// they were.

#include "intrinsics.h"
#include "eval.h"
#include "panic.h"
#include "repr.h"
#include "value_hash.h"
#include "types/champ.h"
#include "types/list.h"
#include "types/int.h"
#include "types/boolean.h"

// ---- missing ---------------------------------------------------------------

// `missing` is one shared uninterned symbol, so it's static like NIL and TRUE
// rather than GC-allocated. Its identity key has the uninterned layout
// `name '\0' suffix` (symbol.h) with a non-numeric suffix, so it can't equal
// any symbol made by `unique` or `gensym`, whose suffixes are serials.
static char missing_key_bytes[] = "missing\0builtin";
static UT_string MISSING_KEY = {.d = missing_key_bytes, .n = sizeof missing_key_bytes,
                                .i = sizeof missing_key_bytes - 1};
static Value MISSING_VALUE = {.type = SYMBOL, .data = {.as_symbol = &MISSING_KEY}};
Value *MISSING = &MISSING_VALUE;

// ---- Argument helpers ------------------------------------------------------

static const char *repr_of(GC *gc, Value *v) {
    return utstring_body(repr(gc, v));
}

static void expect_args(const char *name, Value *args, int n) {
    int got = list_length(args);
    if (got != n) {
        lyra_panic("%s expects exactly %d %s, but got %d", name, n, n == 1 ? "argument" : "arguments", got);
    }
}

static Value *expect_dict(GC *gc, const char *name, Value *v) {
    if (v->type != DICT) lyra_panic("%s expects a dict, but got %s", name, repr_of(gc, v));
    return v;
}

static Value *expect_set(GC *gc, const char *name, Value *v) {
    if (v->type != SET) lyra_panic("%s expects a set, but got %s", name, repr_of(gc, v));
    return v;
}

static Value *expect_dict_or_set(GC *gc, const char *name, Value *v) {
    if (v->type != DICT && v->type != SET) {
        lyra_panic("%s expects a dict or a set, but got %s", name, repr_of(gc, v));
    }
    return v;
}

static const char *kind_name(Value *v) {
    switch (v->type) {
        case FLOAT: return "a float";
        case FUNCTION: return "a function";
        case MACRO: return "a macro";
        default: return "a value without total equality";
    }
}

// The trie hash of `key`, after checking it can be a key at all. `role` is
// "dict key" or "set member".
static uint64_t checked_hash(GC *gc, const char *name, const char *role, Value *key) {
    Value *bad = value_unhashable_part(key);
    if (bad == key) {
        lyra_panic("%s: a %s must have total equality, but %s is %s", name, role,
                   repr_of(gc, key), kind_name(key));
    } else if (bad) {
        lyra_panic("%s: a %s must have total equality, but %s contains %s, %s", name, role,
                   repr_of(gc, key), kind_name(bad), repr_of(gc, bad));
    }
    return champ_hash(key);
}

static uint64_t key_hash_for(GC *gc, const char *name, Value *coll, Value *key) {
    return checked_hash(gc, name, coll->type == DICT ? "dict key" : "set member", key);
}

static Value *eval_checked(GC *gc, Closure *closure, const char *name, Value *expr) {
    Value *v = eval_s_expr(gc, closure, expr);
    if (v == NULL) lyra_panic("%s: failed to evaluate %s", name, repr_of(gc, expr));
    return v;
}

// ---- Construction ----------------------------------------------------------

Value *intrinsic_dict(GC *gc, Closure *closure, Value *args) {
    // (dict (key-expr value-expr) ...): a special form, since its arguments
    // are pairs to evaluate, not calls. A repeated key keeps its last value.
    ChampNode *root = champ_empty(CHAMP_DICT_STRIDE);
    for (; !is_nil(args); args = list_tail(args)) {
        Value *pair = list_head(args);
        if (pair->type != CONS || list_length(pair) != 2) {
            lyra_panic("dict expects (key value) pairs, but got %s", repr_of(gc, pair));
        }
        Value *key = eval_checked(gc, closure, "dict", list_index(pair, 0));
        Value *value = eval_checked(gc, closure, "dict", list_index(pair, 1));
        root = champ_insert(gc, root, key, value, checked_hash(gc, "dict", "dict key", key));
    }
    return make_dict(gc, root);
}

Value *intrinsic_set(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (set member ...)
    ChampNode *root = champ_empty(CHAMP_SET_STRIDE);
    for (; !is_nil(args); args = list_tail(args)) {
        Value *member = list_head(args);
        root = champ_insert(gc, root, member, NULL, checked_hash(gc, "set", "set member", member));
    }
    return make_set(gc, root);
}

// ---- Dict operations -------------------------------------------------------

Value *intrinsic_get(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (get dict key) or (get dict key default); without a default, a missing
    // key gives `missing`.
    int argc = list_length(args);
    if (argc != 2 && argc != 3) {
        lyra_panic("get expects a dict, a key and an optional default, but got %d arguments", argc);
    }
    Value *d = expect_dict(gc, "get", list_index(args, 0));
    Value *key = list_index(args, 1);
    Value **entry = champ_lookup(d->data.as_champ, key, checked_hash(gc, "get", "dict key", key));
    if (entry) return entry[1];
    return argc == 3 ? list_index(args, 2) : MISSING;
}

Value *intrinsic_assoc(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (assoc dict key value)
    expect_args("assoc", args, 3);
    Value *d = expect_dict(gc, "assoc", list_index(args, 0));
    Value *key = list_index(args, 1);
    Value *value = list_index(args, 2);
    ChampNode *root = d->data.as_champ;
    ChampNode *new_root = champ_insert(gc, root, key, value, checked_hash(gc, "assoc", "dict key", key));
    return new_root == root ? d : make_dict(gc, new_root);
}

Value *intrinsic_dissoc(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (dissoc dict key)
    expect_args("dissoc", args, 2);
    Value *d = expect_dict(gc, "dissoc", list_index(args, 0));
    Value *key = list_index(args, 1);
    ChampNode *root = d->data.as_champ;
    ChampNode *new_root = champ_remove(gc, root, key, checked_hash(gc, "dissoc", "dict key", key));
    return new_root == root ? d : make_dict(gc, new_root);
}

// ---- Shared by dicts and sets ----------------------------------------------

Value *intrinsic_contains(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (contains? dict-or-set key)
    expect_args("contains?", args, 2);
    Value *coll = expect_dict_or_set(gc, "contains?", list_index(args, 0));
    Value *key = list_index(args, 1);
    uint64_t hash = key_hash_for(gc, "contains?", coll, key);
    return champ_lookup(coll->data.as_champ, key, hash) ? TRUE : FALSE;
}

Value *intrinsic_count(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (count dict-or-set)
    expect_args("count", args, 1);
    Value *coll = expect_dict_or_set(gc, "count", list_index(args, 0));
    return make_int(gc, (int)coll->data.as_champ->size);
}

// ---- Set operations --------------------------------------------------------

Value *intrinsic_set_add(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (set-add set member)
    expect_args("set-add", args, 2);
    Value *s = expect_set(gc, "set-add", list_index(args, 0));
    Value *member = list_index(args, 1);
    ChampNode *root = s->data.as_champ;
    ChampNode *new_root = champ_insert(gc, root, member, NULL, checked_hash(gc, "set-add", "set member", member));
    return new_root == root ? s : make_set(gc, new_root);
}

Value *intrinsic_set_remove(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (set-remove set member)
    expect_args("set-remove", args, 2);
    Value *s = expect_set(gc, "set-remove", list_index(args, 0));
    Value *member = list_index(args, 1);
    ChampNode *root = s->data.as_champ;
    ChampNode *new_root = champ_remove(gc, root, member, checked_hash(gc, "set-remove", "set member", member));
    return new_root == root ? s : make_set(gc, new_root);
}

// Every argument must be a set, and there must be at least one.
static void expect_sets(GC *gc, const char *name, Value *args) {
    if (is_nil(args)) lyra_panic("%s expects at least 1 argument, but got 0", name);
    for (Value *rest = args; !is_nil(rest); rest = list_tail(rest)) {
        expect_set(gc, name, list_head(rest));
    }
}

typedef struct SetBuild {
    GC *gc;
    ChampNode *root;
    ChampNode *other;  // intersection: the set to test against
} SetBuild;

static void add_member(void *ctx, Value *member, Value *value) {
    (void)value;
    SetBuild *b = ctx;
    b->root = champ_insert(b->gc, b->root, member, NULL, champ_hash(member));
}

static void remove_member(void *ctx, Value *member, Value *value) {
    (void)value;
    SetBuild *b = ctx;
    b->root = champ_remove(b->gc, b->root, member, champ_hash(member));
}

static void remove_unless_in_other(void *ctx, Value *member, Value *value) {
    (void)value;
    SetBuild *b = ctx;
    uint64_t hash = champ_hash(member);
    if (!champ_lookup(b->other, member, hash)) {
        b->root = champ_remove(b->gc, b->root, member, hash);
    }
}

Value *intrinsic_union(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (union set ...)
    expect_sets(gc, "union", args);
    Value *first = list_head(args);
    SetBuild b = {gc, first->data.as_champ, NULL};
    for (Value *rest = list_tail(args); !is_nil(rest); rest = list_tail(rest)) {
        ChampNode *next = list_head(rest)->data.as_champ;
        // Add the smaller set's members to the larger one
        if (next->size > b.root->size) {
            ChampNode *smaller = b.root;
            b.root = next;
            champ_foreach(smaller, add_member, &b);
        } else {
            champ_foreach(next, add_member, &b);
        }
    }
    return b.root == first->data.as_champ ? first : make_set(gc, b.root);
}

Value *intrinsic_intersection(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (intersection set ...)
    expect_sets(gc, "intersection", args);
    Value *first = list_head(args);
    SetBuild b = {gc, first->data.as_champ, NULL};
    for (Value *rest = list_tail(args); !is_nil(rest); rest = list_tail(rest)) {
        b.other = list_head(rest)->data.as_champ;
        champ_foreach(b.root, remove_unless_in_other, &b);
    }
    return b.root == first->data.as_champ ? first : make_set(gc, b.root);
}

Value *intrinsic_difference(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (difference set ...): members of the first set that are in none of the others
    expect_sets(gc, "difference", args);
    Value *first = list_head(args);
    SetBuild b = {gc, first->data.as_champ, NULL};
    for (Value *rest = list_tail(args); !is_nil(rest); rest = list_tail(rest)) {
        champ_foreach(list_head(rest)->data.as_champ, remove_member, &b);
    }
    return b.root == first->data.as_champ ? first : make_set(gc, b.root);
}

// ---- Listing contents ------------------------------------------------------
// The iterator protocol doesn't exist yet, so these return lists, in an
// unspecified order.

typedef enum { LIST_KEYS, LIST_VALUES, LIST_ENTRIES } ListWhat;

typedef struct ListBuild {
    GC *gc;
    ListWhat what;
    Value *list;
} ListBuild;

static void push_entry(void *ctx, Value *key, Value *value) {
    ListBuild *b = ctx;
    Value *item;
    switch (b->what) {
        case LIST_KEYS: item = key; break;
        case LIST_VALUES: item = value; break;
        default: item = list_append(b->gc, key, list_append(b->gc, value, NIL)); break;
    }
    b->list = list_append(b->gc, item, b->list);
}

static Value *list_contents(GC *gc, Value *coll, ListWhat what) {
    ListBuild b = {gc, what, NIL};
    champ_foreach(coll->data.as_champ, push_entry, &b);
    return b.list;
}

Value *intrinsic_keys(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    expect_args("keys", args, 1);
    return list_contents(gc, expect_dict(gc, "keys", list_head(args)), LIST_KEYS);
}

Value *intrinsic_values(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    expect_args("values", args, 1);
    return list_contents(gc, expect_dict(gc, "values", list_head(args)), LIST_VALUES);
}

Value *intrinsic_entries(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    expect_args("entries", args, 1);
    return list_contents(gc, expect_dict(gc, "entries", list_head(args)), LIST_ENTRIES);
}

Value *intrinsic_members(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    expect_args("members", args, 1);
    return list_contents(gc, expect_set(gc, "members", list_head(args)), LIST_KEYS);
}

// ---- Type predicates -------------------------------------------------------

Value *intrinsic_is_dict(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_args("is-dict", args, 1);
    return list_head(args)->type == DICT ? TRUE : FALSE;
}

Value *intrinsic_is_set(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_args("is-set", args, 1);
    return list_head(args)->type == SET ? TRUE : FALSE;
}
