#include "symbol.h"
#include "../gc.h"

/// @brief Allocate a new symbol.
/// @param gc The interpreter's garbage collector.
/// @param name The symbol's name.
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

Value *make_variadic_marker(GC *gc, UT_string *name) {
    Value *result = gc_alloc_value(gc, VARIADIC_MARKER, (ValueData){.as_symbol = name});
    return result;
}
