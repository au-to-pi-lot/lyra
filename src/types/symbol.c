#include "symbol.h"
#include "../gc.h"
#include <string.h>

/// @brief Allocate a new symbol.
/// @param gc The interpreter's garbage collector.
/// @param name The symbol's identity key (see symbol.h).
/// @return Symbol Value
Value *make_symbol(GC *gc, UT_string *name) {
    Value *result = gc_alloc_value(gc, SYMBOL, (ValueData){.as_string = name});
    return result;
}


Value *symbol_from_char(GC *gc, char *name) {
    UT_string *ut_name = gc_alloc_string(gc);
    utstring_printf(ut_name, "%s", name);
    return make_symbol(gc, ut_name);
}

Value *make_uninterned_symbol(GC *gc, const char *name) {
    UT_string *key = gc_alloc_string(gc);
    utstring_printf(key, "%s", name);
    utstring_bincpy(key, "", 1);  // the NUL separator
    utstring_printf(key, "%zu", gc->uninterned_counter++);
    return make_symbol(gc, key);
}

bool symbol_is_interned(Value *sym) {
    UT_string *key = sym->data.as_symbol;
    return strlen(utstring_body(key)) == utstring_len(key);
}

bool symbol_eq(Value *a, Value *b) {
    UT_string *ka = a->data.as_symbol;
    UT_string *kb = b->data.as_symbol;
    return utstring_len(ka) == utstring_len(kb) &&
           memcmp(utstring_body(ka), utstring_body(kb), utstring_len(ka)) == 0;
}

bool symbol_is(Value *val, const char *name) {
    return val->type == SYMBOL && symbol_is_interned(val) &&
           strcmp(utstring_body(val->data.as_symbol), name) == 0;
}

UT_string *symbol_key_suffix(GC *gc, UT_string *key, size_t skip) {
    UT_string *result = gc_alloc_string(gc);
    if (skip < utstring_len(key)) {
        utstring_bincpy(result, utstring_body(key) + skip, utstring_len(key) - skip);
    }
    return result;
}

Value *make_variadic_marker(GC *gc, UT_string *name) {
    Value *result = gc_alloc_value(gc, VARIADIC_MARKER, (ValueData){.as_symbol = name});
    return result;
}
