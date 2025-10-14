#include "function.h"
#include "value.h"
#include "../gc.h"

Value *make_function(GC *gc, Callable *function) {
    Value *result = gc_alloc_value(gc, FUNCTION, (ValueData){.as_function = function});
    return result;
}

Value *make_derived_function(GC *gc, Derived derived) {
    Callable *callable = gc_alloc_callable(gc);
    callable->type = DERIVED_FUNCTION;
    callable->data.as_derived = derived;
    Value *function = make_function(gc, callable);
    return function;
}

Value *make_intrinsic_function(GC *gc, Intrinsic intrinsic) {
    Callable *callable = gc_alloc_callable(gc);
    callable->type = INTRINSIC_FUNCTION;
    callable->data.as_intrinsic = intrinsic;
    Value *function = make_function(gc, callable);
    return function;
}
    
