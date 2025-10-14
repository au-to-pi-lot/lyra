#include "eval.h"
#include "call.h"
#include "types/list.h"
#include <stdio.h>

Value *eval_args(GC *gc, Closure *closure, Value *args) {
    // If the args are nil, nothing to do
    if (is_nil(args)) {
        return args;
    }

    Value *result = NULL;
    Value *end = NULL;

    // Walk the list
    for (Value *item = list_head(args); !is_nil(args); args = list_tail(args), item = list_head(args)) {
        // Alloc new result item
        Cons *cons = gc_alloc_cons(gc);
        Value *next = gc_alloc_value(gc, CONS, (ValueData){.as_cons = cons});

        // Eval item
        cons->car = evaluate(gc, closure, item);

        // Add cons to end of result list
        if (result == NULL) {
            result = end = next;
        } else {
            end->data.as_cons->cdr = next;
            end = next;
        }
    }

    // Remember to set the cdr of the last cons to nil
    end->data.as_cons->cdr = NIL;

    return result;
}

Value *evaluate(GC *gc, Closure *closure, Value *expr) {
    switch (expr->type) {
        case CONS:
            Value *head = list_head(expr);
            if (head->type == SYMBOL) {
                UT_string *symbol = expr->data.as_cons->car->data.as_symbol;
                
                // Special form: quote
                if (strcmp(utstring_body(symbol), "quote") == 0) {
                    // Just return the first argument unevaluated
                    return expr->data.as_cons->cdr->data.as_cons->car;
                }

                // Normal function/macro lookup
                Value *value = get_var(closure, head->data.as_symbol);
                if (value == NULL) {
                    printf("Undefined variable used as function/macro: `%s`\n", utstring_body(head->data.as_symbol));
                    return NULL;
                } else if (value->type == FUNCTION) {
                    return call(gc, closure, value->data.as_function, eval_args(gc, closure, list_tail(expr)));
                } else if (value->type == MACRO) {
                    return call(gc, closure, value->data.as_macro, list_tail(expr));
                } else {
                    // TODO
                }
            } else if (head->type == CONS) {
                Value *value = evaluate(gc, closure, head);
                if (value == NULL) {
                    printf("Cannot eval subexpression");
                    return NULL;
                } else if (value->type == FUNCTION) {
                    return call(gc, closure, value->data.as_function, eval_args(gc, closure, list_tail(expr)));
                } else if (value->type == MACRO) {
                    return call(gc, closure, value->data.as_macro, list_tail(expr));
                } else {
                    // TODO
                }
            } else {
                printf("Cannot be called as function/macro: `%s`\n", utstring_body(head->data.as_symbol));
                return NULL;
            }
            return NULL;
        case SYMBOL:
            return get_var(closure, expr->data.as_symbol);
        case INT:
        case FLOAT: 
        case STRING:
        case FUNCTION:
        case MACRO:
        default:
            return expr;
    }
}