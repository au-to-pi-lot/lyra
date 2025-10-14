#include "float.h"
#include "../gc.h"

Value *make_float(GC *gc, float value) {
    Value *result = gc_alloc_value(gc, FLOAT, (ValueData){.as_float = value});
    return result;
}