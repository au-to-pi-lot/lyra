#include "eval.h"
#include "call.h"
#include "types/list.h"
#include <stdio.h>
#include "parser.h"

// Enable debug output with -DDEBUG_QUASIQUOTE
#ifdef DEBUG_QUASIQUOTE
#define DEBUG_QQ_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_QQ_PRINT(...) ((void)0)
#endif

// Forward declaration
Value *eval_quasiquote(GC *gc, Closure *closure, Value *expr);

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
        cons->car = eval_s_expr(gc, closure, item);

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

// Evaluate a quasiquote template
// Recursively processes the template, evaluating unquote and unquote-splicing forms
Value *eval_quasiquote(GC *gc, Closure *closure, Value *expr) {
    DEBUG_QQ_PRINT("DEBUG QQ: eval_quasiquote called\n");
    // Base case: atoms are returned as-is
    if (expr->type != CONS) {
        DEBUG_QQ_PRINT("DEBUG QQ: Returning atom\n");
        return expr;
    }

    // Empty list
    if (is_nil(expr)) {
        return NIL;
    }

    Value *head = list_head(expr);

    // Check if this is an unquote form: (unquote x)
    if (head->type == SYMBOL && strcmp(utstring_body(head->data.as_symbol), "unquote") == 0) {
        DEBUG_QQ_PRINT("DEBUG QQ: Found unquote\n");
        // Evaluate the unquoted expression
        Value *unquoted_expr = list_index(expr, 1);
        if (unquoted_expr == NULL || is_nil(unquoted_expr)) {
            return NULL;
        }
        Value *result = eval_s_expr(gc, closure, unquoted_expr);
        DEBUG_QQ_PRINT("DEBUG QQ: Unquote evaluated to %p\n", (void*)result);
        return result;
    }

    // Check if this is an unquote-splicing form: (unquote-splicing xs)
    // This is handled by the parent, so we shouldn't see it here at the top level
    if (head->type == SYMBOL && strcmp(utstring_body(head->data.as_symbol), "unquote-splicing") == 0) {
        printf("unquote-splicing not in list context\n");
        return NULL;
    }

    // Otherwise, we need to process this list recursively
    // Walk through the list and build a new list
    Value *result = NIL;
    DEBUG_QQ_PRINT("DEBUG QQ: Processing list recursively\n");

    for (Value *curr = expr; !is_nil(curr); curr = list_tail(curr)) {
        Value *elem = list_head(curr);
        DEBUG_QQ_PRINT("DEBUG QQ: Processing element\n");

        // Check if this element is (unquote-splicing xs)
        if (elem->type == CONS && !is_nil(elem)) {
            Value *elem_head = list_head(elem);
            if (elem_head->type == SYMBOL &&
                strcmp(utstring_body(elem_head->data.as_symbol), "unquote-splicing") == 0) {
                // Evaluate the expression and splice it in
                Value *spliced_expr = list_index(elem, 1);
                if (spliced_expr == NULL || is_nil(spliced_expr)) {
                    return NULL;
                }
                Value *spliced_list = eval_s_expr(gc, closure, spliced_expr);
                if (spliced_list == NULL) {
                    return NULL;
                }
                // Append all elements from spliced_list to result
                for (Value *splice_curr = spliced_list; !is_nil(splice_curr); splice_curr = list_tail(splice_curr)) {
                    list_push(gc, list_head(splice_curr), &result);
                }
                continue;
            }
        }

        // Not unquote-splicing, recursively process this element
        Value *processed = eval_quasiquote(gc, closure, elem);
        if (processed == NULL) {
            return NULL;
        }
        list_push(gc, processed, &result);
    }

    return list_reverse(gc, result);
}

Value *eval_s_expr(GC *gc, Closure *closure, Value *expr) {
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

                // Special form: quasiquote
                if (strcmp(utstring_body(symbol), "quasiquote") == 0) {
                    Value *template = expr->data.as_cons->cdr->data.as_cons->car;
                    return eval_quasiquote(gc, closure, template);
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
                Value *value = eval_s_expr(gc, closure, head);
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

Value *eval(GC *gc, Closure *closure, char *lyra) {
    Value *s_expr = parse(gc, lyra);
    Value *value = eval_s_expr(gc, closure, s_expr);
    return value;
}
