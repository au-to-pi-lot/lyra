#include "value.h"
#include "../gc.h"
#include "list.h"
#include "symbol.h"
#include "champ.h"
#include <string.h>

// The one equality: used by `=`, literal patterns and ,expr value patterns
// in `match`, and by dict/set key lookup, so they can't drift apart.
// Different types are never equal.
// Atoms: numbers by value, strings by contents, interned symbols by name,
// uninterned symbols by identity (symbol_eq). Lists compare element by
// element, recursively. Dicts and sets compare by contents, regardless of
// insertion order. Functions and macros are equal only to themselves.
//
// value_hash (value_hash.c) must agree: a == b implies hash(a) == hash(b).
bool values_equal(Value *a, Value *b) {
    // Walk list spines iteratively so long lists don't grow the C stack
    while (a->type == CONS && b->type == CONS) {
        if (is_nil(a) || is_nil(b)) {
            return is_nil(a) && is_nil(b);
        }
        if (!values_equal(list_head(a), list_head(b))) {
            return false;
        }
        a = list_tail(a);
        b = list_tail(b);
    }

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case INT:
            return a->data.as_int == b->data.as_int;
        case FLOAT:
            return a->data.as_float == b->data.as_float;
        case BOOLEAN:
            return a->data.as_boolean == b->data.as_boolean;
        case STRING:
            // Length-tracked: strings can hold NUL bytes
            return utstring_len(a->data.as_string) == utstring_len(b->data.as_string) &&
                   memcmp(utstring_body(a->data.as_string), utstring_body(b->data.as_string),
                          utstring_len(a->data.as_string)) == 0;
        case SYMBOL:
            return symbol_eq(a, b);
        case DICT:
        case SET:
            return champ_equal(a->data.as_champ, b->data.as_champ);
        default:
            // Functions, macros: identity
            return a == b;
    }
}
