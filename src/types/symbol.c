#include "symbol.h"
#include "../gc.h"

Value *make_symbol(GC *gc, UT_string *value) {
    Value *result = gc_alloc_value(gc, SYMBOL, (ValueData){.as_string = value});
    return result;
}
