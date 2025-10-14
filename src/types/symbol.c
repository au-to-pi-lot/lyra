#include "symbol.h"
#include "../gc.h"

/// @brief Allocate a new symbol.
/// @param gc The interpreter's garbage collector.
/// @param value The symbol's name.
/// @return Symbol Value
Value *make_symbol(GC *gc, UT_string *value) {
    Value *result = gc_alloc_value(gc, SYMBOL, (ValueData){.as_string = value});
    return result;
}
