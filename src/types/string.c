#include "string.h"

Value *make_string(GC *gc, UT_string *value) {
    Value *result = gc_alloc_value(gc, STRING, (ValueData){.as_string = value});
    return result;
}
