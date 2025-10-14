#include "int.h"
#include "../gc.h"

Value *make_int(GC *gc, int value) {
    Value *result = gc_alloc_value(gc, INT, (ValueData){.as_int = value});
    return result;
}