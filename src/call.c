#include "call.h"
#include "closure.h"
#include "eval.h"
#include "types/list.h"
#include "gc.h"
#include "repr.h"

// Enable debug output with -DDEBUG_MACRO
#ifdef DEBUG_MACRO
#define DEBUG_MACRO_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_MACRO_PRINT(...) ((void)0)
#endif

// Forward declaration
void assign_args(GC *gc, Closure *closure, Value *params, Value *args);

Value *call_derived_function(GC *gc, Derived callable, Value *args) {
    Closure *new_closure = make_closure(gc, callable.closure);

    // Bind parameters using assign_args (handles destructuring, variadics, etc.)
    assign_args(gc, new_closure, callable.params, args);

    // Evaluate body expressions, return last result
    Value *result = NIL;
    Value *body = callable.definition;
    while (!is_nil(body)) {
        result = eval_s_expr(gc, new_closure, list_head(body));
        body = list_tail(body);
    }

    return result;
}

void assign_args(GC *gc, Closure *closure, Value *params, Value *args) {
    // Bind parameters to arguments (unevaluated!)
    Value *args_iter = args;

    DEBUG_MACRO_PRINT("DEBUG MACRO: params length=%d, args length=%d\n",
            list_length(params), list_length(args));

#ifdef DEBUG_MACRO
    int arg_idx = 0;
    for (Value *a = args; !is_nil(a); a = list_tail(a)) {
        DEBUG_MACRO_PRINT("DEBUG MACRO: arg[%d] = %s\n", arg_idx++,
                utstring_body(repr(gc, list_head(a))));
    }
#endif

    while (!is_nil(params) && !is_nil(args_iter)) {
        Value *param = list_head(params);

        if (param->type == CONS) {
            // Destructuring assignment
            assign_args(gc, closure, param, list_head(args_iter));
        } else if (param->type == VARIADIC_MARKER) {
            // Variadic assignment
            set_var(gc, closure, param->data.as_symbol, args_iter);
            break;
        } else {
            // Assumed that param->type == SYMBOL
            // Regular assignment
            set_var(gc, closure, param->data.as_symbol, list_head(args_iter));
        }

        params = list_tail(params);
        args_iter = list_tail(args_iter);
    }
}

Value *expand_derived_macro(GC *gc, Closure *closure, Derived callable, Value *args) {
    // The macro body runs lexically: in a fresh child of the closure captured
    // where the macro was defined, not the caller's. (`closure`, the caller's,
    // is only used by call_derived_macro to evaluate the expansion.)
    (void)closure;
    Closure *new_closure = make_closure(gc, callable.closure);
    assign_args(gc, new_closure, callable.params, args);

    // Evaluate body to get expansion
    Value *expansion = NIL;
    Value *body = callable.definition;
    while (!is_nil(body)) {
        expansion = eval_s_expr(gc, new_closure, list_head(body));
        body = list_tail(body);
    }

#ifdef DEBUG_MACRO
    DEBUG_MACRO_PRINT("DEBUG MACRO: expansion = %s\n", utstring_body(repr(gc, expansion)));
    DEBUG_MACRO_PRINT("DEBUG MACRO: expansion has %d elements\n", list_length(expansion));
    if (list_length(expansion) > 0) {
        Value *first = list_head(expansion);
        DEBUG_MACRO_PRINT("DEBUG MACRO: first element = %s (type=%d)\n",
                utstring_body(repr(gc, first)), first ? first->type : -1);
        if (list_length(expansion) > 1) {
            Value *second = list_index(expansion, 1);
            DEBUG_MACRO_PRINT("DEBUG MACRO: second element = %s (type=%d)\n",
                    utstring_body(repr(gc, second)), second ? second->type : -1);
        }
        if (list_length(expansion) > 2) {
            Value *third = list_index(expansion, 2);
            DEBUG_MACRO_PRINT("DEBUG MACRO: third element = %s (type=%d)\n",
                    utstring_body(repr(gc, third)), third ? third->type : -1);
        }
    }
#endif

    return expansion;
}

Value *call_derived_macro(GC *gc, Closure *closure, Derived callable, Value *args) {
    Value *expansion = expand_derived_macro(gc, closure, callable, args);
    if (expansion == NULL) {
        return NULL;
    }
    // Evaluate expansion in CALLER'S closure
    return eval_s_expr(gc, closure, expansion);
}

Value *macroexpand_1(GC *gc, Closure *closure, Value *form, bool *expanded) {
    if (expanded != NULL) {
        *expanded = false;
    }

    // Only a non-empty list headed by a symbol can be a macro call.
    // (Head expressions that are themselves lists would have to be
    // evaluated to find out, which expansion must not do.)
    if (form == NULL || form->type != CONS || is_nil(form)) {
        return form;
    }
    Value *head = list_head(form);
    if (head->type != SYMBOL) {
        return form;
    }

    // Resolve the head the same way eval_s_expr does: in the caller's closure.
    Value *value = get_var(closure, head->data.as_symbol);
    if (value == NULL || value->type != MACRO) {
        return form;
    }

    // Intrinsic macros (special forms) have no expansion; leave them alone.
    Callable *macro = value->data.as_macro;
    if (macro->type != DERIVED_MACRO) {
        return form;
    }

    Value *expansion = expand_derived_macro(gc, closure, macro->data.as_derived, list_tail(form));
    if (expansion != NULL && expanded != NULL) {
        *expanded = true;
    }
    return expansion;
}

Value *macroexpand(GC *gc, Closure *closure, Value *form, bool *expanded) {
    bool any = false;
    bool step = true;
    while (step && form != NULL) {
        form = macroexpand_1(gc, closure, form, &step);
        any = any || step;
    }
    if (expanded != NULL) {
        *expanded = any;
    }
    return form;
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