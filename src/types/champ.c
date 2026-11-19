#include "champ.h"
#include "../value_hash.h"
#include <assert.h>
#include <string.h>

// See champ.h for the data structure. Every function here treats existing
// nodes as immutable: changes build new nodes along the path to the root.

#define CHAMP_BITS 5
#define CHAMP_HASH_BITS 64

static ChampNode EMPTY_DICT = {0, 0, 0, CHAMP_DICT_STRIDE, false};
static ChampNode EMPTY_SET = {0, 0, 0, CHAMP_SET_STRIDE, false};

ChampNode *champ_empty(uint8_t stride) {
    return stride == CHAMP_SET_STRIDE ? &EMPTY_SET : &EMPTY_DICT;
}

bool champ_is_static(const ChampNode *node) {
    return node == &EMPTY_DICT || node == &EMPTY_SET;
}

// ---- Bit twiddling ---------------------------------------------------------

static unsigned popcount32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned)__builtin_popcount(x);
#else
    // Portable SWAR popcount (MSVC's __popcnt needs a POPCNT-capable CPU).
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    return (unsigned)((x * 0x01010101u) >> 24);
#endif
}

// The 5-bit slice of `hash` that picks a slot at depth `shift`.
static uint32_t champ_bit(uint64_t hash, unsigned shift) {
    return 1u << ((uint32_t)(hash >> shift) & 31u);
}

// Position of `bit` among the set bits of `map`.
static size_t bit_index(uint32_t map, uint32_t bit) {
    return popcount32(map & (bit - 1));
}

size_t champ_payload_arity(const ChampNode *node) {
    return node->collision ? node->size : popcount32(node->datamap);
}

size_t champ_node_arity(const ChampNode *node) {
    return node->collision ? 0 : popcount32(node->nodemap);
}

static size_t slot_count(const ChampNode *node) {
    return champ_payload_arity(node) * node->stride + champ_node_arity(node);
}

static ChampNode *child_at(const ChampNode *node, size_t child_index) {
    return node->slots[champ_payload_arity(node) * node->stride + child_index];
}

static bool keys_equal(Value *a, Value *b) {
    return a == b || values_equal(a, b);
}

// ---- Hashing ---------------------------------------------------------------

static uint64_t (*hash_override)(Value *key) = NULL;

uint64_t champ_hash(Value *key) {
    return hash_override ? hash_override(key) : value_hash(key);
}

void champ_set_hash_function(uint64_t (*fn)(Value *key)) {
    hash_override = fn;
}

// ---- Node construction -----------------------------------------------------

static ChampNode *new_node(GC *gc, uint8_t stride, bool collision, uint32_t datamap,
                           uint32_t nodemap, size_t size, size_t nslots) {
    ChampNode *node = gc_alloc_champ_node(gc, nslots);
    node->datamap = datamap;
    node->nodemap = nodemap;
    node->size = size;
    node->stride = stride;
    node->collision = collision;
    return node;
}

static void put_entry(ChampNode *node, size_t slot, Value *key, Value *value) {
    node->slots[slot] = key;
    if (node->stride == CHAMP_DICT_STRIDE) node->slots[slot + 1] = value;
}

#define COPY_SLOTS(dst, src, n) memcpy((dst), (src), (n) * sizeof(void *))

// Same node with the value of inline entry `index` replaced (dicts only).
static ChampNode *copy_with_value(GC *gc, ChampNode *n, size_t index, Value *value) {
    size_t total = slot_count(n);
    ChampNode *m = new_node(gc, n->stride, n->collision, n->datamap, n->nodemap, n->size, total);
    COPY_SLOTS(m->slots, n->slots, total);
    m->slots[index * n->stride + 1] = value;
    return m;
}

// Same node with a new inline entry in slot `bit`.
static ChampNode *copy_with_entry(GC *gc, ChampNode *n, uint32_t bit, Value *key, Value *value) {
    size_t s = n->stride, total = slot_count(n);
    size_t at = bit_index(n->datamap, bit) * s;
    ChampNode *m = new_node(gc, n->stride, false, n->datamap | bit, n->nodemap, n->size + 1, total + s);
    COPY_SLOTS(m->slots, n->slots, at);
    put_entry(m, at, key, value);
    COPY_SLOTS(m->slots + at + s, n->slots + at, total - at);
    return m;
}

// Same node without the inline entry in slot `bit`.
static ChampNode *copy_without_entry(GC *gc, ChampNode *n, uint32_t bit) {
    size_t s = n->stride, total = slot_count(n);
    size_t at = bit_index(n->datamap, bit) * s;
    ChampNode *m = new_node(gc, n->stride, false, n->datamap ^ bit, n->nodemap, n->size - 1, total - s);
    COPY_SLOTS(m->slots, n->slots, at);
    COPY_SLOTS(m->slots + at, n->slots + at + s, total - at - s);
    return m;
}

// Same node with the child in slot `bit` replaced by `child`.
static ChampNode *copy_with_child(GC *gc, ChampNode *n, uint32_t bit, ChampNode *child, size_t new_size) {
    size_t total = slot_count(n);
    ChampNode *m = new_node(gc, n->stride, false, n->datamap, n->nodemap, new_size, total);
    COPY_SLOTS(m->slots, n->slots, total);
    m->slots[champ_payload_arity(n) * n->stride + bit_index(n->nodemap, bit)] = child;
    return m;
}

// The inline entry in slot `bit` replaced by `child`, which holds that entry
// plus one more (so the size grows by one).
static ChampNode *copy_entry_to_child(GC *gc, ChampNode *n, uint32_t bit, ChampNode *child) {
    size_t s = n->stride, total = slot_count(n);
    size_t pay = champ_payload_arity(n) * s;
    size_t at = bit_index(n->datamap, bit) * s;
    size_t child_at_old = pay + bit_index(n->nodemap, bit);
    ChampNode *m = new_node(gc, n->stride, false, n->datamap ^ bit, n->nodemap | bit, n->size + 1,
                            total - s + 1);
    COPY_SLOTS(m->slots, n->slots, at);                                    // entries before
    COPY_SLOTS(m->slots + at, n->slots + at + s, child_at_old - at - s);   // entries after, children before
    m->slots[child_at_old - s] = child;
    COPY_SLOTS(m->slots + child_at_old - s + 1, n->slots + child_at_old, total - child_at_old);
    return m;
}

// The child in slot `bit` replaced by its last remaining entry (`child` is
// the size-1 result of a removal from it, so the size shrinks by one).
static ChampNode *copy_child_to_entry(GC *gc, ChampNode *n, uint32_t bit, ChampNode *child) {
    size_t s = n->stride, total = slot_count(n);
    size_t pay = champ_payload_arity(n) * s;
    size_t at = bit_index(n->datamap, bit) * s;  // same index once `bit` joins datamap
    size_t child_at_old = pay + bit_index(n->nodemap, bit);
    ChampNode *m = new_node(gc, n->stride, false, n->datamap | bit, n->nodemap ^ bit, n->size - 1,
                            total - 1 + s);
    COPY_SLOTS(m->slots, n->slots, at);                                    // entries before
    put_entry(m, at, child->slots[0], s == CHAMP_DICT_STRIDE ? child->slots[1] : NULL);
    COPY_SLOTS(m->slots + at + s, n->slots + at, child_at_old - at);       // entries after, children before
    COPY_SLOTS(m->slots + child_at_old + s, n->slots + child_at_old + 1, total - child_at_old - 1);
    return m;
}

// The smallest subtree holding two distinct entries, starting at depth `shift`.
static ChampNode *merge_entries(GC *gc, uint8_t stride, Value *k0, Value *v0, uint64_t h0,
                                Value *k1, Value *v1, uint64_t h1, unsigned shift) {
    if (shift >= CHAMP_HASH_BITS) {
        assert(h0 == h1);
        ChampNode *m = new_node(gc, stride, true, 0, 0, 2, 2 * (size_t)stride);
        put_entry(m, 0, k0, v0);
        put_entry(m, stride, k1, v1);
        return m;
    }

    uint32_t b0 = champ_bit(h0, shift), b1 = champ_bit(h1, shift);
    if (b0 != b1) {
        ChampNode *m = new_node(gc, stride, false, b0 | b1, 0, 2, 2 * (size_t)stride);
        bool first = b0 < b1;
        put_entry(m, 0, first ? k0 : k1, first ? v0 : v1);
        put_entry(m, stride, first ? k1 : k0, first ? v1 : v0);
        return m;
    }

    ChampNode *child = merge_entries(gc, stride, k0, v0, h0, k1, v1, h1, shift + CHAMP_BITS);
    ChampNode *m = new_node(gc, stride, false, 0, b0, 2, 1);
    m->slots[0] = child;
    return m;
}

// ---- Lookup ----------------------------------------------------------------

Value **champ_lookup(ChampNode *root, Value *key, uint64_t hash) {
    ChampNode *n = root;
    unsigned shift = 0;
    for (;;) {
        size_t s = n->stride;
        if (n->collision) {
            for (size_t i = 0; i < n->size; i++) {
                if (keys_equal(n->slots[i * s], key)) return (Value **)&n->slots[i * s];
            }
            return NULL;
        }
        uint32_t bit = champ_bit(hash, shift);
        if (n->datamap & bit) {
            size_t at = bit_index(n->datamap, bit) * s;
            return keys_equal(n->slots[at], key) ? (Value **)&n->slots[at] : NULL;
        }
        if (!(n->nodemap & bit)) return NULL;
        n = child_at(n, bit_index(n->nodemap, bit));
        shift += CHAMP_BITS;
    }
}

// ---- Insertion -------------------------------------------------------------

static ChampNode *node_insert(GC *gc, ChampNode *n, Value *key, Value *value, uint64_t hash,
                              unsigned shift, bool *added) {
    size_t s = n->stride;

    if (n->collision) {
        for (size_t i = 0; i < n->size; i++) {
            if (keys_equal(n->slots[i * s], key)) {
                if (s == CHAMP_SET_STRIDE || n->slots[i * s + 1] == value) return n;
                return copy_with_value(gc, n, i, value);
            }
        }
        size_t total = n->size * s;
        ChampNode *m = new_node(gc, n->stride, true, 0, 0, n->size + 1, total + s);
        COPY_SLOTS(m->slots, n->slots, total);
        put_entry(m, total, key, value);
        *added = true;
        return m;
    }

    uint32_t bit = champ_bit(hash, shift);

    if (n->datamap & bit) {
        size_t index = bit_index(n->datamap, bit);
        Value *existing = n->slots[index * s];
        if (keys_equal(existing, key)) {
            if (s == CHAMP_SET_STRIDE || n->slots[index * s + 1] == value) return n;
            return copy_with_value(gc, n, index, value);
        }
        Value *existing_value = s == CHAMP_DICT_STRIDE ? n->slots[index * s + 1] : NULL;
        ChampNode *child = merge_entries(gc, n->stride, existing, existing_value, champ_hash(existing),
                                         key, value, hash, shift + CHAMP_BITS);
        *added = true;
        return copy_entry_to_child(gc, n, bit, child);
    }

    if (n->nodemap & bit) {
        ChampNode *child = child_at(n, bit_index(n->nodemap, bit));
        bool child_added = false;
        ChampNode *new_child = node_insert(gc, child, key, value, hash, shift + CHAMP_BITS, &child_added);
        if (new_child == child) return n;
        *added = child_added;
        return copy_with_child(gc, n, bit, new_child, n->size + (child_added ? 1 : 0));
    }

    *added = true;
    return copy_with_entry(gc, n, bit, key, value);
}

ChampNode *champ_insert(GC *gc, ChampNode *root, Value *key, Value *value, uint64_t hash) {
    bool added = false;
    return node_insert(gc, root, key, value, hash, 0, &added);
}

// ---- Removal ---------------------------------------------------------------

// Returns `n` if `key` is absent. A non-root result may have size 1; the
// parent then inlines its entry, which keeps the trie canonical.
static ChampNode *node_remove(GC *gc, ChampNode *n, Value *key, uint64_t hash, unsigned shift) {
    size_t s = n->stride;

    if (n->collision) {
        for (size_t i = 0; i < n->size; i++) {
            if (keys_equal(n->slots[i * s], key)) {
                size_t total = n->size * s;
                ChampNode *m = new_node(gc, n->stride, true, 0, 0, n->size - 1, total - s);
                COPY_SLOTS(m->slots, n->slots, i * s);
                COPY_SLOTS(m->slots + i * s, n->slots + (i + 1) * s, total - (i + 1) * s);
                return m;
            }
        }
        return n;
    }

    uint32_t bit = champ_bit(hash, shift);

    if (n->datamap & bit) {
        size_t at = bit_index(n->datamap, bit) * s;
        if (!keys_equal(n->slots[at], key)) return n;
        if (n->size == 1) return champ_empty(n->stride);  // only the root can be this small
        return copy_without_entry(gc, n, bit);
    }

    if (n->nodemap & bit) {
        ChampNode *child = child_at(n, bit_index(n->nodemap, bit));
        ChampNode *new_child = node_remove(gc, child, key, hash, shift + CHAMP_BITS);
        if (new_child == child) return n;
        if (new_child->size == 1) return copy_child_to_entry(gc, n, bit, new_child);
        return copy_with_child(gc, n, bit, new_child, n->size - 1);
    }

    return n;
}

ChampNode *champ_remove(GC *gc, ChampNode *root, Value *key, uint64_t hash) {
    return node_remove(gc, root, key, hash, 0);
}

// ---- Equality --------------------------------------------------------------

static bool entry_equal(ChampNode *a, size_t a_slot, ChampNode *b, size_t b_slot) {
    if (!keys_equal(a->slots[a_slot], b->slots[b_slot])) return false;
    if (a->stride == CHAMP_DICT_STRIDE) {
        Value *va = a->slots[a_slot + 1], *vb = b->slots[b_slot + 1];
        return va == vb || values_equal(va, vb);
    }
    return true;
}

bool champ_equal(ChampNode *a, ChampNode *b) {
    if (a == b) return true;
    if (a->size != b->size || a->stride != b->stride || a->collision != b->collision) return false;
    size_t s = a->stride;

    if (a->collision) {
        // Unordered: every entry of `a` must appear in `b` (sizes match and
        // keys within a node are distinct, so that's a bijection).
        for (size_t i = 0; i < a->size; i++) {
            bool found = false;
            for (size_t j = 0; j < b->size && !found; j++) {
                if (keys_equal(a->slots[i * s], b->slots[j * s])) {
                    if (!entry_equal(a, i * s, b, j * s)) return false;
                    found = true;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    // Canonical form: equal contents means identical shape.
    if (a->datamap != b->datamap || a->nodemap != b->nodemap) return false;
    size_t pay = champ_payload_arity(a);
    for (size_t i = 0; i < pay; i++) {
        if (!entry_equal(a, i * s, b, i * s)) return false;
    }
    size_t nodes = champ_node_arity(a);
    for (size_t i = 0; i < nodes; i++) {
        if (!champ_equal(child_at(a, i), child_at(b, i))) return false;
    }
    return true;
}

// ---- Iteration -------------------------------------------------------------

void champ_foreach(ChampNode *root, void (*fn)(void *ctx, Value *key, Value *value), void *ctx) {
    size_t s = root->stride;
    size_t pay = champ_payload_arity(root);
    for (size_t i = 0; i < pay; i++) {
        fn(ctx, root->slots[i * s], s == CHAMP_DICT_STRIDE ? root->slots[i * s + 1] : NULL);
    }
    size_t nodes = champ_node_arity(root);
    for (size_t i = 0; i < nodes; i++) {
        champ_foreach(child_at(root, i), fn, ctx);
    }
}

// ---- Invariant checking (tests) --------------------------------------------

static const char *validate_node(ChampNode *n, uint8_t stride, unsigned shift, uint64_t prefix,
                                 bool is_root, size_t *count) {
    if (n->stride != stride) return "node stride differs from the root's";
    if (!is_root && n->size < 2) return "non-root node with fewer than 2 entries (not canonical)";
    size_t s = n->stride;
    uint64_t prefix_mask = shift >= CHAMP_HASH_BITS ? ~(uint64_t)0 : (((uint64_t)1 << shift) - 1);

    if (n->collision) {
        if (shift < CHAMP_HASH_BITS) return "collision node above the last trie level";
        if (n->datamap || n->nodemap) return "collision node with bitmaps";
        for (size_t i = 0; i < n->size; i++) {
            if (champ_hash(n->slots[i * s]) != prefix) return "collision node key with a different hash";
            for (size_t j = i + 1; j < n->size; j++) {
                if (keys_equal(n->slots[i * s], n->slots[j * s])) return "duplicate key in collision node";
            }
        }
        *count += n->size;
        return NULL;
    }

    if (shift >= CHAMP_HASH_BITS) return "regular node below the last trie level";
    if (n->datamap & n->nodemap) return "slot is both an entry and a child";

    size_t pay = champ_payload_arity(n);
    size_t total = pay;
    for (uint32_t slot = 0, entry = 0; slot < 32; slot++) {
        uint32_t bit = 1u << slot;
        if (!(n->datamap & bit)) continue;
        uint64_t h = champ_hash(n->slots[entry * s]);
        if ((h & prefix_mask) != prefix) return "entry off its hash path";
        if (champ_bit(h, shift) != bit) return "entry in the wrong slot";
        entry++;
    }
    for (uint32_t slot = 0, child = 0; slot < 32; slot++) {
        uint32_t bit = 1u << slot;
        if (!(n->nodemap & bit)) continue;
        ChampNode *c = child_at(n, child++);
        size_t child_count = 0;
        // Below the last level this is the full hash (collision nodes).
        uint64_t child_prefix = prefix | ((uint64_t)slot << shift);
        const char *why = validate_node(c, stride, shift + CHAMP_BITS, child_prefix, false, &child_count);
        if (why) return why;
        if (child_count != c->size) return "child size field is wrong";
        total += child_count;
    }
    if (total != n->size) return "node size field is wrong";
    *count += total;
    return NULL;
}

const char *champ_validate(ChampNode *root) {
    size_t count = 0;
    return validate_node(root, root->stride, 0, 0, true, &count);
}

// ---- Values ----------------------------------------------------------------

Value *make_dict(GC *gc, ChampNode *root) {
    return gc_alloc_value(gc, DICT, (ValueData){.as_champ = root});
}

Value *make_set(GC *gc, ChampNode *root) {
    return gc_alloc_value(gc, SET, (ValueData){.as_champ = root});
}
