#include "prelude.h"
#include "closure.h"
#include "intrinsics.h"
#include "types/boolean.h"
#include "types/list.h"
#include "types/macro.h"
#include "types/function.h"

void define_intrinsic_value(GC *gc, Closure *closure, char *key, Value *value) {
    UT_string *ut_key = gc_alloc_string(gc);
    utstring_printf(ut_key, "%s", key);

    set_var(gc, closure, ut_key, value);
}

void define_intrinsic_macro(GC *gc, Closure *closure, char *key, Intrinsic intrinsic_macro) {
    Value *value = make_intrinsic_macro(gc, intrinsic_macro);
    define_intrinsic_value(gc, closure, key, value);
}

void define_intrinsic_function(GC *gc, Closure *closure, char *key, Intrinsic intrinsic_function) {
    Value *value = make_intrinsic_function(gc, intrinsic_function);
    define_intrinsic_value(gc, closure, key, value);
}

void prelude(GC *gc, Closure *closure) {
    define_intrinsic_value(gc, closure, "true", TRUE);
    define_intrinsic_value(gc, closure, "false", FALSE);
    define_intrinsic_value(gc, closure, "nil", NIL);

    define_intrinsic_macro(gc, closure, "lambda", &intrinsic_lambda);
    define_intrinsic_macro(gc, closure, "if", &intrinsic_if);
    define_intrinsic_macro(gc, closure, "define", &intrinsic_define);

    define_intrinsic_function(gc, closure, "+", &intrinsic_add);
    define_intrinsic_function(gc, closure, "-", &intrinsic_sub);
    define_intrinsic_function(gc, closure, "*", &intrinsic_mul);
    define_intrinsic_function(gc, closure, "/", &intrinsic_div);
    define_intrinsic_function(gc, closure, "divmod", &intrinsic_divmod);
    define_intrinsic_function(gc, closure, "=", &intrinsic_eq);
}
