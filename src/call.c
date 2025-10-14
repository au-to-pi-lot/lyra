#include "call.h"
#include "closure.h"
#include "eval.h"
#include "types/list.h"
#include "gc.h"


Value *call_derived_function(GC *gc, Derived callable, Value *args) {
    Closure *new_closure = make_closure(gc, callable.closure);
    
    // Bind parameters
    Value *params = callable.params;
    Value *args_iter = args;
    while (!is_nil(params) && !is_nil(args_iter)) {
        set_var(gc, new_closure, list_head(params)->data.as_symbol, list_head(args_iter));
        params = list_tail(params);
        args_iter = list_tail(args_iter);
    }
    
    // Evaluate body expressions, return last result
    Value *result = NIL;
    Value *body = callable.definition;
    while (!is_nil(body)) {
        result = evaluate(gc, new_closure, list_head(body));
        body = list_tail(body);
    }

    return result;
}
Value *call_derived_macro(GC *gc, Closure *closure, Derived callable, Value *args) {
    // Create new closure from macro's captured environment
    Closure *new_closure = make_closure(gc, callable.closure);
    
    // Bind parameters to arguments (unevaluated!)
    Value *params = callable.params;
    Value *args_iter = args;
    while (!is_nil(params) && !is_nil(args_iter)) {
        set_var(gc, new_closure, list_head(params)->data.as_symbol, list_head(args));
        params = list_tail(params);
        args_iter = list_tail(args_iter);
    }
    
    // Evaluate body to get expansion
    Value *expansion = NIL;
    Value *body = callable.definition;
    while (!is_nil(body)) {
        expansion = evaluate(gc, new_closure, list_head(body));
        body = list_tail(body);
    }
    
    // Evaluate expansion in CALLER'S closure
    return evaluate(gc, closure, expansion);
}

Value *call_intrinsic(GC *gc, Closure *closure, Intrinsic callable, Value *args) {
    return callable(gc, closure, args);
}

Value *call(GC *gc, Closure *closure, Callable *callable, Value *args) {
    switch (callable->type) {
        case DERIVED_FUNCTION:
            return call_derived_function(gc, callable->data.as_derived, args);
        case DERIVED_MACRO:
            return call_derived_macro(gc, closure, callable->data.as_derived, args);
        case INTRINSIC_FUNCTION:
        case INTRINSIC_MACRO:
            return call_intrinsic(gc, closure, callable->data.as_intrinsic, args);
        default:
            printf("Unknown callable type: %d", callable->type);
            return NIL;
    }
}