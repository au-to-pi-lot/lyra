#include "macro.h"

Value *make_intrinsic_macro(GC *gc, Intrinsic intrinsic_macro) {
    Callable *callable = gc_alloc_callable(gc);
    *callable = (Callable){
        .type = INTRINSIC_MACRO,
        .data.as_intrinsic = intrinsic_macro
    };
    
    Value *value = gc_alloc_value(gc, MACRO, (ValueData){.as_macro = callable});

    return value;
}