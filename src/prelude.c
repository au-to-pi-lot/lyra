#include "prelude.h"
#include "closure.h"
#include "intrinsics.h"
#include "types/boolean.h"
#include "types/list.h"

void define_intrinsic_value(Closure *closure, char *key, Value *value) {
    UT_string *ut_key;
    utstring_new(ut_key);
    utstring_printf(ut_key, key);

    set_var(closure, ut_key, value);
}

void define_intrinsic_macro(Closure *closure, char *key, Intrinsic intrinsic_macro) {
    Callable *callable = malloc(sizeof(Callable));
    *callable = (Callable){
        .type = INTRINSIC_MACRO,
        .data.as_intrinsic = intrinsic_macro
    };
    
    Value *value = malloc(sizeof(Value));
    *value = (Value){
        .type = MACRO,
        .data.as_function = callable
    };

    define_intrinsic_value(closure, key, value);
}

void define_intrinsic_function(Closure *closure, char *key, Intrinsic intrinsic_function) {
    Callable *callable = malloc(sizeof(Callable));
    *callable = (Callable){
        .type = INTRINSIC_FUNCTION,
        .data.as_intrinsic = intrinsic_function
    };
    
    Value *value = malloc(sizeof(Value));
    *value = (Value){
        .type = FUNCTION,
        .data.as_function = callable
    };
    
    define_intrinsic_value(closure, key, value);
}

void prelude(Closure *closure) {
    define_intrinsic_value(closure, "true", TRUE);
    define_intrinsic_value(closure, "false", FALSE);
    define_intrinsic_value(closure, "nil", NIL);

    define_intrinsic_macro(closure, "lambda", &intrinsic_lambda);
    define_intrinsic_macro(closure, "if", &intrinsic_if);
    define_intrinsic_macro(closure, "define", &intrinsic_define);

    define_intrinsic_function(closure, "+", &intrinsic_add);
    define_intrinsic_function(closure, "-", &intrinsic_sub);
    define_intrinsic_function(closure, "*", &intrinsic_mul);
    define_intrinsic_function(closure, "/", &intrinsic_div);
    define_intrinsic_function(closure, "divmod", &intrinsic_divmod);
    define_intrinsic_function(closure, "=", &intrinsic_eq);
}
