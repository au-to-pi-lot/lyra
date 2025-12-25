#include "macro.h"

Value *make_intrinsic_macro(GC *gc, Intrinsic intrinsic_macro) {
    Callable *callable = gc_alloc_callable(gc);
    *callable = (Callable){
        .type = INTRINSIC_MACRO,
        .is_pure = true,  // Macros are pure (they just transform code)
        .data.as_intrinsic = intrinsic_macro
    };

    Value *value = gc_alloc_value(gc, MACRO, (ValueData){.as_macro = callable});

    return value;
}

Value *make_derived_macro(GC *gc, Derived derived) {
    Callable *callable = gc_alloc_callable(gc);
    *callable = (Callable){
        .type = DERIVED_MACRO,
        .is_pure = true,  // Macros are pure (they just transform code)
        .data.as_derived = derived
    };

    Value *value = gc_alloc_value(gc, MACRO, (ValueData){.as_macro = callable});

    return value;
}