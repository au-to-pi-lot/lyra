#pragma once

// The "what" half of value hashing (hash.h is the "how"): each type feeds a
// type tag and its contents to a Hasher, so that a == b (values_equal)
// implies value_hash(a) == value_hash(b), and values of different types
// (1 vs "1") don't collide by construction.
//
//   int      tag, the value as a 64-bit word
//   boolean  tag, one byte
//   string   tag, the bytes up to the first NUL (values_equal compares with
//            strcmp), then a 0xFF terminator, so ("ab" "c") and ("a" "bc")
//            differ as list keys
//   symbol   tag, the length and bytes of its identity key (symbol.h): the
//            name for an interned symbol, name + NUL + serial for an
//            uninterned one, so uninterned symbols hash by identity, never
//            by pointer
//   list     tag, the length, then each element in order
//   dict/set tag, the count, then the wrapping sum of a separate hash per
//            entry (key and value, or member): order-independent, since equal
//            dicts can hold their entries in different orders
//
// Floats, functions and macros have no total equality and no hash. Neither
// does a list or dict that contains one anywhere (a dict through its values;
// keys and set members were checked on the way in).

#include <stdint.h>
#include "types/value.h"

// The hash of `v`, which must be hashable (value_unhashable_part(v) == NULL).
uint64_t value_hash(Value *v);

// The first part of `v` (possibly `v` itself) that has no total equality,
// or NULL if `v` can be a dict key or set member.
Value *value_unhashable_part(Value *v);
