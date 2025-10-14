#include "intrinsics.h"
#include "eval.h"
#include "closure.h"
#include "types/list.h"
#include "types/boolean.h"
#include "types/int.h"
#include "types/float.h"
#include "types/function.h"
#include <stdlib.h>


Value *intrinsic_lambda(GC *gc, Closure *closure, Value *args) {
    // args->car = parameter list (should be a CONS-typed Value)
    // args->cdr = body expressions (implicit progn)

    Value *result = make_derived_function(
        gc,
        (Derived){
            .closure = closure,          // Capture current environment
            .params = list_head(args),
            .definition = list_tail(args)
        }
    );
    
    return result;
}

Value *intrinsic_if(GC *gc, Closure *closure, Value *args) {
    // (if condition then-expr else-expr)
    // args->car = condition
    // args->cdr->car = then-expr
    // args->cdr->cdr->car = else-expr (optional)

    Value *condition = evaluate(gc, closure, list_head(args));

    // Determine falsiness: NULL, false boolean, 0, or nil
    int is_false = (condition == NULL) ||
                   (condition->type == BOOLEAN && !condition->data.as_boolean) ||
                   (condition->type == INT && condition->data.as_int == 0) ||
                   (condition->type == CONS && condition->data.as_cons == NULL);

    if (!is_false) {
        // Evaluate then branch
        return evaluate(gc, closure, list_index(args, 1));
    } else if (list_index(args, 2)) {
        // Evaluate else branch if it exists
        return evaluate(gc, closure, list_index(args, 2));
    } else {
        // No else branch, return nil
        return NIL;
    }
}

Value *intrinsic_define(GC *gc, Closure *closure, Value *args) {
    // (define name value)
    // args->car = name (symbol)
    // args->cdr->car = value expression (to be evaluated)

    UT_string *name = list_head(args)->data.as_symbol;
    Value *value = evaluate(gc, closure, list_index(args, 1));

    set_var(gc, closure, name, value);

    // Return the defined value
    return value;
}

Value *intrinsic_add(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (+ arg1 arg2 ...)
    int int_result = 0;
    float float_result = 0.0f;
    int has_float = 0;

    for (Value *arg = list_head(args); !is_nil(args); args = list_tail(args), arg = list_head(args)) {
        if (arg->type == INT) {
            if (has_float) {
                float_result += arg->data.as_int;
            } else {
                int_result += arg->data.as_int;
            }
        } else if (arg->type == FLOAT) {
            if (!has_float) {
                float_result = (float)int_result;
                has_float = 1;
            }
            float_result += arg->data.as_float;
        }
    }

    Value *result;
    if (has_float) {
        result = make_float(gc, float_result);
    } else {
        result = make_int(gc, int_result);
    }
    return result;
}

Value *intrinsic_sub(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (- arg1 arg2 ...)
    if (is_nil(args)) return gc_alloc_value(gc, INT, (ValueData){.as_int = 0});

    Value *first = list_head(args);
    int has_float = (first->type == FLOAT);
    int int_result = has_float ? 0 : first->data.as_int;
    float float_result = has_float ? first->data.as_float : 0.0f;

    args = list_tail(args);

    for (Value *arg = list_head(args); !is_nil(args); args = list_tail(args), arg = list_head(args)) {
        if (arg->type == INT) {
            if (has_float) {
                float_result -= arg->data.as_int;
            } else {
                int_result -= arg->data.as_int;
            }
        } else if (arg->type == FLOAT) {
            if (!has_float) {
                float_result = (float)int_result;
                has_float = 1;
            }
            float_result -= arg->data.as_float;
        }
    }

    Value *result;
    if (has_float) {
        result = make_float(gc, float_result);
    } else {
        result = make_int(gc, int_result);
    }
    return result;
}

Value *intrinsic_mul(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (* arg1 arg2 ...)
    int int_result = 1;
    float float_result = 1.0f;
    int has_float = 0;

    for (Value *arg = list_head(args); !is_nil(args); args = list_tail(args), arg = list_head(args)) {
        if (arg->type == INT) {
            if (has_float) {
                float_result *= arg->data.as_int;
            } else {
                int_result *= arg->data.as_int;
            }
        } else if (arg->type == FLOAT) {
            if (!has_float) {
                float_result = (float)int_result;
                has_float = 1;
            }
            float_result *= arg->data.as_float;
        }
    }

    Value *result;
    if (has_float) {
        result = make_float(gc, float_result);
    } else {
        result = make_int(gc, int_result);
    }
    return result;
}

Value *intrinsic_div(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (/ arg1 arg2 ...)
    if (is_nil(args)) return make_float(gc, 1.0);

    Value *first = list_head(args);
    int has_float = (first->type == FLOAT);
    float float_result = has_float ? first->data.as_float : (float)first->data.as_int;

    // Division always produces float
    has_float = 1;

    args = list_tail(args);

    for (Value *arg = list_head(args); !is_nil(args); args = list_tail(args), arg = list_head(args)) {
        if (arg->type == INT) {
            float_result /= arg->data.as_int;
        } else if (arg->type == FLOAT) {
            float_result /= arg->data.as_float;
        }
    }

    Value *result = make_float(gc, float_result);
    return result;
}

Value *intrinsic_divmod(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (divmod dividend divisor) => (quotient remainder)
    if (list_length(args) < 2) return NIL;

    Value *dividend = list_index(args, 0);
    Value *divisor = list_index(args, 1);

    // Only works with integers
    if (dividend->type != INT || divisor->type != INT) return NIL;

    int quot = dividend->data.as_int / divisor->data.as_int;
    int rem = dividend->data.as_int % divisor->data.as_int;

    // Create quotient Value
    Value *quot_val = make_int(gc, quot);

    // Create remainder Value
    Value *rem_val = make_int(gc, rem);

    Value *result = NIL;
    result = list_append(gc, rem_val, result);
    result = list_append(gc, quot_val, result);

    return result;
}

Value *intrinsic_eq(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    // (= arg1 arg2)
    if (list_length(args) < 2) return FALSE;

    Value *first = list_index(args, 0);
    Value *second = list_index(args, 1);

    // Type must match
    if (first->type != second->type) return FALSE;

    // Compare based on type
    switch (first->type) {
        case INT:
            return first->data.as_int == second->data.as_int ? TRUE : FALSE;
        case FLOAT:
            return first->data.as_float == second->data.as_float ? TRUE : FALSE;
        case BOOLEAN:
            return first->data.as_boolean == second->data.as_boolean ? TRUE : FALSE;
        case CONS:
            return first->data.as_cons == second->data.as_cons ? TRUE : FALSE;
        default:
            return FALSE;
    }
}
