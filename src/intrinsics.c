#include "intrinsics.h"
#include "eval.h"
#include "closure.h"
#include "types/list.h"
#include "types/boolean.h"
#include <stdlib.h>


Value *intrinsic_lambda(Closure *closure, Value *args) {
    // args->car = parameter list (should be a CONS-typed Value)
    // args->cdr = body expressions (implicit progn)

    Derived derived = {
        .closure = closure,          // Capture current environment
        .params = list_head(args),
        .definition = list_tail(args)
    };

    Callable *callable = malloc(sizeof(Callable));
    callable->type = DERIVED_FUNCTION;
    callable->data.as_derived = derived;

    Value *result = malloc(sizeof(Value));
    result->type = FUNCTION;
    result->data.as_function = callable;

    return result;
}

Value *intrinsic_if(Closure *closure, Value *args) {
    // (if condition then-expr else-expr)
    // args->car = condition
    // args->cdr->car = then-expr
    // args->cdr->cdr->car = else-expr (optional)

    Value *condition = evaluate(closure, list_head(args));

    // Determine falsiness: NULL, false boolean, 0, or nil
    int is_false = (condition == NULL) ||
                   (condition->type == BOOLEAN && !condition->data.as_boolean) ||
                   (condition->type == INT && condition->data.as_int == 0) ||
                   (condition->type == CONS && condition->data.as_cons == NULL);

    if (!is_false) {
        // Evaluate then branch
        return evaluate(closure, list_index(args, 1));
    } else if (list_index(args, 2)) {
        // Evaluate else branch if it exists
        return evaluate(closure, list_index(args, 2));
    } else {
        // No else branch, return nil
        return NIL;
    }
}

Value *intrinsic_define(Closure *closure, Value *args) {
    // (define name value)
    // args->car = name (symbol)
    // args->cdr->car = value expression (to be evaluated)

    UT_string *name = list_head(args)->data.as_symbol;
    Value *value = evaluate(closure, list_index(args, 1));

    set_var(closure, name, value);

    // Return the defined value
    return value;
}

Value *intrinsic_add(Closure *closure, Value *args) {
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

    Value *result = malloc(sizeof(Value));
    if (has_float) {
        result->type = FLOAT;
        result->data.as_float = float_result;
    } else {
        result->type = INT;
        result->data.as_int = int_result;
    }
    return result;
}

Value *intrinsic_sub(Closure *closure, Value *args) {
    (void)closure;
    // (- arg1 arg2 ...)
    if (is_nil(args)) return make_value(INT, (ValueData){.as_int = 0});

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

    Value *result = malloc(sizeof(Value));
    if (has_float) {
        result->type = FLOAT;
        result->data.as_float = float_result;
    } else {
        result->type = INT;
        result->data.as_int = int_result;
    }
    return result;
}

Value *intrinsic_mul(Closure *closure, Value *args) {
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

    Value *result = malloc(sizeof(Value));
    if (has_float) {
        result->type = FLOAT;
        result->data.as_float = float_result;
    } else {
        result->type = INT;
        result->data.as_int = int_result;
    }
    return result;
}

Value *intrinsic_div(Closure *closure, Value *args) {
    (void)closure;
    // (/ arg1 arg2 ...)
    if (is_nil(args)) return make_value(INT, (ValueData){.as_int = 1});

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

    Value *result = malloc(sizeof(Value));
    result->type = FLOAT;
    result->data.as_float = float_result;
    return result;
}

Value *intrinsic_divmod(Closure *closure, Value *args) {
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
    Value *quot_val = malloc(sizeof(Value));
    quot_val->type = INT;
    quot_val->data.as_int = quot;

    // Create remainder Value
    Value *rem_val = malloc(sizeof(Value));
    rem_val->type = INT;
    rem_val->data.as_int = rem;

    // Create second cons cell (remainder)
    Cons *second_cons = malloc(sizeof(Cons));
    second_cons->car = rem_val;
    second_cons->cdr = NIL;  // End of list

    Value *cdr_val = malloc(sizeof(Value));
    cdr_val->type = CONS;
    cdr_val->data.as_cons = second_cons;

    // Create first cons cell (quotient)
    Cons *first_cons = malloc(sizeof(Cons));
    first_cons->car = quot_val;
    first_cons->cdr = cdr_val;

    // Wrap in Value
    Value *result = malloc(sizeof(Value));
    result->type = CONS;
    result->data.as_cons = first_cons;

    return result;
}

Value *intrinsic_eq(Closure *closure, Value *args) {
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
