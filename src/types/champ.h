#pragma once

// Persistent hash tries for DICT and SET values: CHAMP, the "Compressed
// Hash-Array Mapped Prefix-tree" of Steindorfer & Vinju, "Optimizing
// Hash-Array Mapped Tries for Fast and Lean Immutable JVM Collections"
// (OOPSLA 2015).
//
// - Each node consumes 5 bits of the key's 64-bit hash (shift 0, 5, ..., 60;
//   the last level has only 4 bits) and has up to 32 slots. Two bitmaps say
//   which slots hold an inline entry (`datamap`) and which hold a child node
//   (`nodemap`). Inline entries come first in `slots`, ordered by slot
//   number, then the child nodes, also in slot order.
// - Keys whose full 64-bit hashes are equal end up in a collision node below
//   the last level: a flat, unordered list of entries.
// - A dict stores key/value pairs (stride 2); a set is the same trie with
//   keys only (stride 1).
// - Nodes are never modified after construction: every update copies the
//   path from the root to the change and shares everything else, so older
//   versions stay valid and unchanged.
// - Canonical form: a child node always holds at least 2 entries (removal
//   inlines a child that drops to 1). So the shape of a trie depends only on
//   its contents, never on the order of insertions and removals, which makes
//   equality a parallel walk of two tries.
//
// Nodes are GC objects (GC_CHAMP_NODE); gc.c traces them via `header`.
// The empty tries are static and not GC-registered (header == NULL).
//
// Hashing: callers pass each key's hash, computed by champ_hash (which they
// call only after checking the key is hashable; see value_hash.h). The trie
// recomputes the hashes of keys it already holds when it splits an inline
// entry into a child node.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "value.h"
#include "../gc.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)  // flexible array member: standard C99, but MSVC warns
#endif
struct ChampNode {
    GCObject *header;   // the node's GC header (NULL for the static empty tries)
    uint32_t datamap;   // slots holding an inline entry
    uint32_t nodemap;   // slots holding a child node
    size_t size;        // number of entries in this subtree
    uint8_t stride;     // Values per entry: 2 for a dict (key, value), 1 for a set
    bool collision;     // collision node: `size` entries in `slots`, no bitmaps
    void *slots[];      // inline entries (Value *), then child nodes (ChampNode *)
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#define CHAMP_DICT_STRIDE 2
#define CHAMP_SET_STRIDE 1

// The shared empty trie for a stride.
ChampNode *champ_empty(uint8_t stride);

// Number of inline entries / child nodes in `node`.
size_t champ_payload_arity(const ChampNode *node);
size_t champ_node_arity(const ChampNode *node);

// The hash the trie uses for a key: value_hash, unless a test overrode it.
uint64_t champ_hash(Value *key);

// Test hook: make the tries hash keys with `fn` instead of value_hash (NULL
// restores value_hash). `fn` must agree with values_equal. Only switch it
// while no dict or set built under the other function is in use.
void champ_set_hash_function(uint64_t (*fn)(Value *key));

// Lookup: a pointer to the entry's slots (key, then value for a dict), or
// NULL if `key` is absent.
Value **champ_lookup(ChampNode *root, Value *key, uint64_t hash);

// A trie with `key` mapped to `value` (`value` is ignored for a set). Returns
// `root` itself if nothing changed.
ChampNode *champ_insert(GC *gc, ChampNode *root, Value *key, Value *value, uint64_t hash);

// A trie without `key`. Returns `root` itself if `key` wasn't present.
ChampNode *champ_remove(GC *gc, ChampNode *root, Value *key, uint64_t hash);

// Structural, order-independent equality (keys and, for dicts, values by
// values_equal).
bool champ_equal(ChampNode *a, ChampNode *b);

// Call `fn` on every entry, in trie order (unspecified, hash-dependent).
// `value` is NULL for sets.
void champ_foreach(ChampNode *root, void (*fn)(void *ctx, Value *key, Value *value), void *ctx);

// Check the structural invariants (bitmaps, sizes, canonical form, every key
// on its hash path). Returns NULL if they hold, else a description of the
// first violation. For tests.
const char *champ_validate(ChampNode *root);

// Wrap a trie as a DICT or SET value.
Value *make_dict(GC *gc, ChampNode *root);
Value *make_set(GC *gc, ChampNode *root);
