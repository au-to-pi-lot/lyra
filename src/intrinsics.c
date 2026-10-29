#include "intrinsics.h"
#include "eval.h"
#include "closure.h"
#include "call.h"
#include "parser.h"
#include "repr.h"
#include "types/list.h"
#include "types/boolean.h"
#include "types/int.h"
#include "types/float.h"
#include "types/function.h"
#include "types/macro.h"
#include "types/symbol.h"
#include "types/string.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "platform_dir.h"
#include "panic.h"

// Enable debug output with -DDEBUG_LAMBDA
#ifdef DEBUG_LAMBDA
#define DEBUG_LAMBDA_PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_LAMBDA_PRINT(...) ((void)0)
#endif

// Forward declarations for pattern matching helpers
static bool pattern_matches_impl(GC *gc, Value *pattern, Value *value);
static bool values_equal(Value *a, Value *b);
static void pattern_bind_vars(GC *gc, Closure *closure, Value *pattern, Value *value);

// Argument validation helpers (defined below, before the arithmetic intrinsics)
static void expect_arity(const char *name, Value *args, int n);

Value *process_params(GC *gc, Value *params) {
    // Process parameter list to convert *name to VARIADIC_MARKER
    Value *processed_params = NIL;
    bool seen_variadic = false;

    // Handle nil params
    if (is_nil(params)) {
        return NIL;
    }

    for (Value *p = params; !is_nil(p); p = list_tail(p)) {
        // Defensive check: p must be a cons to call list_head
        if (p->type != CONS) {
            lyra_panic("improper parameter list: %s", utstring_body(repr(gc, params)));
        }

        Value *param = list_head(p);

        // Check for null/invalid param
        if (param == NULL) {
            lyra_panic("internal error: null parameter in parameter list");
        }

        if (param->type == CONS && !is_nil(param)) {
            // Destructuring pattern - process and add to params
            if (seen_variadic) {
                lyra_panic("no parameters allowed after variadic parameter");
            }
            // Recursively process the destructuring pattern
            Value *processed_pattern = process_params(gc, param);
            // Note: processed_pattern could be NIL (empty list) which is valid
            list_push(gc, processed_pattern, &processed_params);
        } else if (param->type == SYMBOL) {
            UT_string *sym = param->data.as_symbol;
            const char *name = utstring_body(sym);

            // Check if parameter starts with *
            if (name[0] == '*' && name[1] != '\0') {
                // Variadic parameter must be last (suffix only)
                if (!is_nil(list_tail(p))) {
                    lyra_panic("variadic parameter *%s must be the last parameter", name + 1);
                }

                // Create variadic marker with the name (without *)
                UT_string *rest_name = gc_alloc_string(gc);
                utstring_printf(rest_name, "%s", name + 1);  // Skip the *
                Value *variadic_marker = make_variadic_marker(gc, rest_name);

                list_push(gc, variadic_marker, &processed_params);
                seen_variadic = true;
            } else {
                if (seen_variadic) {
                    lyra_panic("no parameters allowed after variadic parameter");
                }
                list_push(gc, param, &processed_params);
            }
        } else {
            lyra_panic("invalid parameter: %s", utstring_body(repr(gc, param)));
        }
    }

    return list_reverse(gc, processed_params);
}


Value *intrinsic_lambda(GC *gc, Closure *closure, Value *args) {
    // args->car = parameter list (should be a CONS-typed Value)
    // args->cdr = body expressions (implicit progn)

    DEBUG_LAMBDA_PRINT("DEBUG LAMBDA: Called with args\n");
    DEBUG_LAMBDA_PRINT("DEBUG LAMBDA: Args length: %d\n", list_length(args));
#ifdef DEBUG_LAMBDA
    if (list_length(args) > 0) {
        Value *first_arg = list_head(args);
        DEBUG_LAMBDA_PRINT("DEBUG LAMBDA: First arg type: %d\n", first_arg ? first_arg->type : -1);
    }
#endif

    if (is_nil(args)) {
        lyra_panic("lambda expects a parameter list");
    }

    Value *processed_params = process_params(gc, list_head(args));

    Value *result = make_derived_function(
        gc,
        (Derived){
            .closure = closure,          // Capture current environment
            .params = processed_params,
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

    int arg_count = list_length(args);
    if (arg_count != 2 && arg_count != 3) {
        lyra_panic("if expects a condition, a then-branch and an optional else-branch, but got %d arguments",
                   arg_count);
    }

    Value *condition = eval_s_expr(gc, closure, list_head(args));

    // Determine falsiness: NULL, false boolean, 0, or nil
    int is_false = (condition == NULL) ||
                   (condition->type == BOOLEAN && !condition->data.as_boolean) ||
                   (condition->type == INT && condition->data.as_int == 0) ||
                   (condition->type == CONS && condition->data.as_cons == NULL);

    if (!is_false) {
        // Evaluate then branch
        return eval_s_expr(gc, closure, list_index(args, 1));
    } else if (list_index(args, 2)) {
        // Evaluate else branch if it exists
        return eval_s_expr(gc, closure, list_index(args, 2));
    } else {
        // No else branch, return nil
        return NIL;
    }
}

Value *intrinsic_define(GC *gc, Closure *closure, Value *args) {
    // (define name value)
    // args->car = name (symbol)
    // args->cdr->car = value expression (to be evaluated)

    expect_arity("define", args, 2);
    if (list_head(args)->type != SYMBOL) {
        lyra_panic("define expects a symbol as its name, but got %s",
                   utstring_body(repr(gc, list_head(args))));
    }

    UT_string *name = list_head(args)->data.as_symbol;
    Value *value = eval_s_expr(gc, closure, list_index(args, 1));

    set_var(gc, closure, name, value);

    // Return the defined value
    return value;
}

Value *intrinsic_define_macro(GC *gc, Closure *closure, Value *args) {
    expect_arity("define-macro", args, 2);
    if (list_head(args)->type != SYMBOL) {
        lyra_panic("define-macro expects a symbol as its name, but got %s",
                   utstring_body(repr(gc, list_head(args))));
    }

    UT_string *name = list_head(args)->data.as_symbol;
    Value *function = eval_s_expr(gc, closure, list_index(args, 1));

    // Convert function to macro
    Value *macro_value = NULL;
    if (function && function->type == FUNCTION) {
        Callable *func_callable = function->data.as_function;
        if (func_callable->type == DERIVED_FUNCTION) {
            // Create a derived macro from the derived function
            macro_value = make_derived_macro(gc, func_callable->data.as_derived);
        }
    }

    if (!macro_value) {
        lyra_panic("define-macro requires a function");
    }

    set_var(gc, closure, name, macro_value);

    // Return the defined macro
    return macro_value;
}

Value *intrinsic_and(GC *gc, Closure *closure, Value *args) {
    Value *result = TRUE;

    while (!is_nil(args)) {
        Value *arg = list_head(args);
        Value *value = eval_s_expr(gc, closure, arg);
        if (value == FALSE || value == NIL) {
            return FALSE;
        }
        result = value;
        args = list_tail(args);
    }

    return result;
}

Value *intrinsic_or(GC *gc, Closure *closure, Value *args) {
    Value *result = FALSE;

    while (!is_nil(args)) {
        Value *arg = list_head(args);
        Value *value = eval_s_expr(gc, closure, arg);
        if (value != FALSE && value != NIL) {
            return value;
        }
        result = value;
        args = list_tail(args);
    }

    return result;
}

Value *intrinsic_not(GC *gc, Closure *closure, Value *args) {
    int length = list_length(args);
    if (length != 1) {
        lyra_panic("not expects exactly one argument, but got %d", length);
    }
    Value *arg = list_head(args);
    Value *value = eval_s_expr(gc, closure, arg);
    return boolean_not(truthiness(value));
}

// ---- Argument validation helpers --------------------------------------
// Wrong arity and wrong types are programmer errors, so they panic.

static const char *plural_args(int n) {
    return n == 1 ? "argument" : "arguments";
}

// Panic unless `args` has exactly `n` elements.
static void expect_arity(const char *name, Value *args, int n) {
    int got = list_length(args);
    if (got != n) {
        lyra_panic("%s expects exactly %d %s, but got %d", name, n, plural_args(n), got);
    }
}

// Panic unless `v` is an int or a float.
static void expect_number(GC *gc, const char *name, Value *v) {
    if (v->type != INT && v->type != FLOAT) {
        lyra_panic("%s expects numbers, but got %s", name, utstring_body(repr(gc, v)));
    }
}

// Panic unless `v` is a list (a cons cell or nil).
static void expect_list(GC *gc, const char *name, Value *v) {
    if (v->type != CONS) {
        lyra_panic("%s expects a list, but got %s", name, utstring_body(repr(gc, v)));
    }
}

// Shared by + - * : folds `args` left to right. Ints stay ints until the
// first float, after which the accumulator is a float (see ARCHITECTURE.md).
// For - the first argument is the starting value instead of being subtracted.
typedef enum { ARITH_ADD, ARITH_SUB, ARITH_MUL } ArithOp;

static Value *arith_fold(GC *gc, const char *name, ArithOp op, Value *args) {
    int int_result = (op == ARITH_MUL) ? 1 : 0;
    float float_result = 0.0f;
    bool has_float = false;
    bool first = true;

    for (; !is_nil(args); args = list_tail(args)) {
        Value *arg = list_head(args);
        expect_number(gc, name, arg);

        if (first && op == ARITH_SUB) {
            if (arg->type == FLOAT) {
                has_float = true;
                float_result = arg->data.as_float;
            } else {
                int_result = arg->data.as_int;
            }
            first = false;
            continue;
        }
        first = false;

        if (arg->type == FLOAT && !has_float) {
            float_result = (float)int_result;
            has_float = true;
        }

        if (has_float) {
            float operand = (arg->type == FLOAT) ? arg->data.as_float : (float)arg->data.as_int;
            switch (op) {
                case ARITH_ADD: float_result += operand; break;
                case ARITH_SUB: float_result -= operand; break;
                case ARITH_MUL: float_result *= operand; break;
            }
        } else {
            int operand = arg->data.as_int;
            switch (op) {
                case ARITH_ADD: int_result += operand; break;
                case ARITH_SUB: int_result -= operand; break;
                case ARITH_MUL: int_result *= operand; break;
            }
        }
    }

    return has_float ? make_float(gc, float_result) : make_int(gc, int_result);
}

Value *intrinsic_add(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (+ arg1 arg2 ...)
    return arith_fold(gc, "+", ARITH_ADD, args);
}

Value *intrinsic_sub(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (- arg1 arg2 ...)
    return arith_fold(gc, "-", ARITH_SUB, args);
}

Value *intrinsic_mul(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (* arg1 arg2 ...)
    return arith_fold(gc, "*", ARITH_MUL, args);
}

Value *intrinsic_div(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (/ arg1 arg2 ...) -- division always produces a float
    if (is_nil(args)) return make_float(gc, 1.0);

    Value *first = list_head(args);
    expect_number(gc, "/", first);
    float float_result = (first->type == FLOAT) ? first->data.as_float : (float)first->data.as_int;

    for (args = list_tail(args); !is_nil(args); args = list_tail(args)) {
        Value *arg = list_head(args);
        expect_number(gc, "/", arg);
        float divisor = (arg->type == FLOAT) ? arg->data.as_float : (float)arg->data.as_int;
        if (divisor == 0.0f) {
            lyra_panic("division by zero");
        }
        float_result /= divisor;
    }

    return make_float(gc, float_result);
}

Value *intrinsic_divmod(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (divmod dividend divisor) => (quotient remainder)
    expect_arity("divmod", args, 2);

    Value *dividend = list_index(args, 0);
    Value *divisor = list_index(args, 1);

    // Only works with integers
    if (dividend->type != INT || divisor->type != INT) {
        lyra_panic("divmod expects integers, but got %s and %s",
                   utstring_body(repr(gc, dividend)), utstring_body(repr(gc, divisor)));
    }
    if (divisor->data.as_int == 0) {
        lyra_panic("division by zero");
    }
    if (dividend->data.as_int == INT_MIN && divisor->data.as_int == -1) {
        lyra_panic("integer overflow in divmod");
    }

    int quot = dividend->data.as_int / divisor->data.as_int;
    int rem = dividend->data.as_int % divisor->data.as_int;

    Value *quot_val = make_int(gc, quot);
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
    expect_arity("=", args, 2);

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
        case STRING:
            return strcmp(utstring_body(first->data.as_string),
                         utstring_body(second->data.as_string)) == 0 ? TRUE : FALSE;
        case SYMBOL:
            return strcmp(utstring_body(first->data.as_symbol),
                         utstring_body(second->data.as_symbol)) == 0 ? TRUE : FALSE;
        case CONS:
            return first->data.as_cons == second->data.as_cons ? TRUE : FALSE;
        default:
            return FALSE;
    }
}

typedef enum { CMP_LT, CMP_LE, CMP_GT, CMP_GE } CmpOp;

// Shared by < <= > >=. Exactly two numbers of the same type.
static Value *compare_numbers(GC *gc, const char *name, CmpOp op, Value *args) {
    expect_arity(name, args, 2);

    Value *a = list_index(args, 0);
    Value *b = list_index(args, 1);
    expect_number(gc, name, a);
    expect_number(gc, name, b);
    if (a->type != b->type) {
        lyra_panic("%s cannot compare %s and %s: mixed int and float",
                   name, utstring_body(repr(gc, a)), utstring_body(repr(gc, b)));
    }

    bool result;
    if (a->type == INT) {
        int x = a->data.as_int, y = b->data.as_int;
        result = op == CMP_LT ? x < y : op == CMP_LE ? x <= y : op == CMP_GT ? x > y : x >= y;
    } else {
        float x = a->data.as_float, y = b->data.as_float;
        result = op == CMP_LT ? x < y : op == CMP_LE ? x <= y : op == CMP_GT ? x > y : x >= y;
    }
    return result ? TRUE : FALSE;
}

Value *intrinsic_lt(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    return compare_numbers(gc, "<", CMP_LT, args);
}

Value *intrinsic_lt_eq(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    return compare_numbers(gc, "<=", CMP_LE, args);
}

Value *intrinsic_gt(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    return compare_numbers(gc, ">", CMP_GT, args);
}

Value *intrinsic_gt_eq(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    return compare_numbers(gc, ">=", CMP_GE, args);
}

Value *intrinsic_car(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (car list) -- (car nil) is nil
    expect_arity("car", args, 1);
    Value *list = list_index(args, 0);
    expect_list(gc, "car", list);
    return list_head(list);
}

Value *intrinsic_cdr(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (cdr list) -- (cdr nil) is nil
    expect_arity("cdr", args, 1);
    Value *list = list_index(args, 0);
    expect_list(gc, "cdr", list);
    return list_tail(list);
}

Value *intrinsic_cons(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (cons elem list)
    expect_arity("cons", args, 2);
    Value *elem = list_index(args, 0);
    Value *list = list_index(args, 1);
    expect_list(gc, "cons", list);
    return list_append(gc, elem, list);
}

Value *intrinsic_apply(GC *gc, Closure *closure, Value *args) {
    // (apply fn arg-list)
    expect_arity("apply", args, 2);
    Value *fn = list_index(args, 0);
    Value *arg_list = list_index(args, 1);
    expect_list(gc, "apply", arg_list);

    // Call the function with the arg list
    if (fn->type == FUNCTION) {
        return call(gc, closure, fn->data.as_function, arg_list);
    } else if (fn->type == MACRO) {
        return call(gc, closure, fn->data.as_macro, arg_list);
    }

    lyra_panic("apply: not callable: %s", utstring_body(repr(gc, fn)));
}

Value *intrinsic_gensym(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    (void)args;
    // Generate a unique symbol: G__0, G__1, G__2, etc.
    UT_string *name = gc_alloc_string(gc);
    utstring_printf(name, "G__%zu", gc->gensym_counter++);
    return make_symbol(gc, name);
}

// (expand-macro-1 form) / (expand-macro form): expand without evaluating.
// The macro head is resolved in the caller's closure. Non-macro forms,
// including special forms, come back unchanged. Impure: expanding runs the
// macro's body, which may do anything (e.g. gensym). Strictly, purity depends
// on the macro being expanded; we conservatively call it impure.
Value *intrinsic_expand_macro_1(GC *gc, Closure *closure, Value *args) {
    if (list_length(args) != 1) {
        lyra_panic("expand-macro-1 requires exactly one argument (a quoted form)");
    }
    return macroexpand_1(gc, closure, list_head(args), NULL);
}

Value *intrinsic_expand_macro(GC *gc, Closure *closure, Value *args) {
    if (list_length(args) != 1) {
        lyra_panic("expand-macro requires exactly one argument (a quoted form)");
    }
    return macroexpand(gc, closure, list_head(args), NULL);
}

// Type predicates
Value *intrinsic_is_cons(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-cons", args, 1);
    Value *val = list_head(args);
    return val->type == CONS ? TRUE : FALSE;
}

Value *intrinsic_is_int(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-int", args, 1);
    Value *val = list_head(args);
    return val->type == INT ? TRUE : FALSE;
}

Value *intrinsic_is_float(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-float", args, 1);
    Value *val = list_head(args);
    return val->type == FLOAT ? TRUE : FALSE;
}

Value *intrinsic_is_boolean(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-boolean", args, 1);
    Value *val = list_head(args);
    return val->type == BOOLEAN ? TRUE : FALSE;
}

Value *intrinsic_is_string(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-string", args, 1);
    Value *val = list_head(args);
    return val->type == STRING ? TRUE : FALSE;
}

Value *intrinsic_is_symbol(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-symbol", args, 1);
    Value *val = list_head(args);
    return val->type == SYMBOL ? TRUE : FALSE;
}

Value *intrinsic_is_function(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-function", args, 1);
    Value *val = list_head(args);
    return val->type == FUNCTION ? TRUE : FALSE;
}

Value *intrinsic_is_macro(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-macro", args, 1);
    Value *val = list_head(args);
    return val->type == MACRO ? TRUE : FALSE;
}

Value *intrinsic_is_nil(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    expect_arity("is-nil", args, 1);
    Value *val = list_head(args);
    return is_nil(val) ? TRUE : FALSE;
}

Value *intrinsic_load_file(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (load-file "path.lisp")
    if (list_length(args) < 1) {
        lyra_panic("load-file requires a filename argument");
    }

    Value *filename_val = list_index(args, 0);
    if (filename_val->type != STRING) {
        lyra_panic("load-file requires a string filename");
    }

    const char *filename = utstring_body(filename_val->data.as_string);

    // Open the file
    FILE *file = fopen(filename, "r");
    if (!file) {
        // TODO: result
        printf("load-file: could not open file '%s'\n", filename);
        return NIL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file contents
    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        // TODO: result (or panic? out-of-memory is arguably unrecoverable)
        printf("load-file: memory allocation failed\n");
        return NIL;
    }

    size_t read_size = fread(buffer, 1, file_size, file);
    buffer[read_size] = '\0';
    fclose(file);

    // Create a GC-managed string
    UT_string *contents = gc_alloc_string(gc);
    utstring_printf(contents, "%s", buffer);
    free(buffer);

    return make_string(gc, contents);
}

Value *intrinsic_eval_string(GC *gc, Closure *closure, Value *args) {
    // (eval-string "code")
    if (list_length(args) < 1) {
        lyra_panic("eval-string requires a string argument");
    }

    Value *code_val = list_index(args, 0);
    if (code_val->type != STRING) {
        lyra_panic("eval-string requires a string argument");
    }

    const char *code = utstring_body(code_val->data.as_string);

    // Evaluate all expressions in the string
    const char *input = code;
    Value *result = NIL;
    while (*input != '\0') {
        size_t consumed = 0;
        Value *ast = parse_with_pos(gc, input, &consumed);
        if (ast == NULL) {
            // Check for whitespace only
            int only_whitespace = 1;
            for (const char *p = input; *p != '\0'; p++) {
                if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                    only_whitespace = 0;
                    break;
                }
            }
            if (only_whitespace) break;

            // parse_with_pos has returned (and freed its tokens) by now, so
            // panicking here can't leak parser memory.
            lyra_panic("eval-string: parse error");
        }
        result = eval_s_expr(gc, closure, ast);
        if (result == NULL) {
            return NIL;
        }
        input += consumed;
    }

    return result;
}

static FILE *print_stream = NULL;

void intrinsic_set_print_stream(FILE *stream) {
    print_stream = stream;
}

// Shared by print/write: concatenates string args (no separator) to the output stream,
// optionally followed by a newline. All args are validated before anything is written.
// A future file-handle variant of write can peel a handle off the front of args here.
static Value *write_strings(Value *args, const char *name, bool newline) {
    for (Value *p = args; !is_nil(p); p = list_tail(p)) {
        if (list_head(p)->type != STRING) {
            lyra_panic("%s requires string arguments", name);
        }
    }

    FILE *out = print_stream ? print_stream : stdout;
    for (Value *p = args; !is_nil(p); p = list_tail(p)) {
        fputs(utstring_body(list_head(p)->data.as_string), out);
    }
    if (newline) fputc('\n', out);
    fflush(out);
    return NIL;
}

// (print "a" "b" ...) - like write, plus a trailing newline
Value *intrinsic_print(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    return write_strings(args, "print", true);
}

// (write "a" "b" ...) - no trailing newline
Value *intrinsic_write(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    return write_strings(args, "write", false);
}

// (panic "a" "b" ...) - abort evaluation with the concatenated message.
// Same argument rules as print/write: strings only, no implicit conversion.
// Concatenate panic message args, panicking (as `name`) on a non-string.
static UT_string *panic_message_from_args(GC *gc, Value *args, const char *name) {
    UT_string *message = gc_alloc_string(gc);
    for (Value *p = args; !is_nil(p); p = list_tail(p)) {
        Value *arg = list_head(p);
        if (arg->type != STRING) {
            lyra_panic("%s requires string arguments", name);
        }
        utstring_concat(message, arg->data.as_string);
    }
    return message;
}

Value *intrinsic_panic(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    UT_string *message = panic_message_from_args(gc, args, "panic");
    lyra_panic("%s", utstring_body(message));
}

// (panic-with-status status "a" "b" ...) - like panic, but when the panic
// ends the process (file / -c mode) it exits with `status`, an int in
// 1..255. In the REPL it behaves exactly like panic: the status is ignored.
Value *intrinsic_panic_with_status(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    if (is_nil(args)) {
        lyra_panic("panic-with-status requires an exit status");
    }
    Value *status = list_head(args);
    if (status->type != INT) {
        lyra_panic("panic-with-status requires an int exit status, got %s",
                   utstring_body(repr(gc, status)));
    }
    if (status->data.as_int < 1 || status->data.as_int > 255) {
        lyra_panic("panic-with-status exit status must be between 1 and 255, got %d",
                   status->data.as_int);
    }
    UT_string *message = panic_message_from_args(gc, list_tail(args), "panic-with-status");
    lyra_panic_with_status(status->data.as_int, "%s", utstring_body(message));
}

Value *intrinsic_get_var(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    // (get-var 'symbol)
    if (list_length(args) < 1) {
        lyra_panic("get-var requires a symbol argument");
    }

    Value *sym = list_index(args, 0);
    if (sym->type != SYMBOL) {
        lyra_panic("get-var requires a symbol argument");
    }

    Value *value = get_var(closure, sym->data.as_symbol);
    if (value == NULL) {
        return NIL;
    }
    return value;
}

Value *intrinsic_pure_p(GC *gc, Closure *closure, Value *args) {
    (void)gc;
    (void)closure;
    // (pure? fn)
    if (list_length(args) < 1) {
        lyra_panic("is-pure requires a function argument");
    }

    Value *fn = list_index(args, 0);
    if (fn->type != FUNCTION && fn->type != MACRO) {
        lyra_panic("is-pure requires a function or macro argument");
    }

    Callable *callable = (fn->type == FUNCTION) ? fn->data.as_function : fn->data.as_macro;
    return callable->is_pure ? TRUE : FALSE;
}

Value *intrinsic_begin(GC *gc, Closure *closure, Value *args) {
    // (begin expr1 expr2 expr3 ...) - evaluate expressions in sequence, return last
    // Does NOT create a new scope
    Value *result = NIL;
    while (!is_nil(args)) {
        result = eval_s_expr(gc, closure, list_head(args));
        if (result == NULL) {
            return NIL;
        }
        args = list_tail(args);
    }
    return result;
}

Value *intrinsic_string(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // Polymorphic string constructor:
    // (string 'foo) => "foo"   - symbol to string
    // (string 42) => "42"       - int to string
    // (string "hello" " " "world") => "hello world" - string concatenation

    if (list_length(args) == 0) {
        // No args: return empty string
        return make_string(gc, gc_alloc_string(gc));
    }

    if (list_length(args) == 1) {
        // Single arg: convert to string
        Value *arg = list_index(args, 0);
        UT_string *str = gc_alloc_string(gc);

        switch (arg->type) {
            case STRING:
                utstring_printf(str, "%s", utstring_body(arg->data.as_string));
                break;
            case SYMBOL:
                utstring_printf(str, "%s", utstring_body(arg->data.as_symbol));
                break;
            case INT:
                utstring_printf(str, "%d", arg->data.as_int);
                break;
            case FLOAT:
                utstring_printf(str, "%f", arg->data.as_float);
                break;
            default:
                lyra_panic("string: cannot convert %s to a string", utstring_body(repr(gc, arg)));
        }
        return make_string(gc, str);
    }

    // Multiple args: concatenate strings
    UT_string *result = gc_alloc_string(gc);
    while (!is_nil(args)) {
        Value *arg = list_head(args);
        if (arg->type != STRING) {
            lyra_panic("string: multiple arguments must all be strings");
        }
        utstring_printf(result, "%s", utstring_body(arg->data.as_string));
        args = list_tail(args);
    }
    return make_string(gc, result);
}

Value *intrinsic_symbol(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // Polymorphic symbol constructor:
    // (symbol "foo") => 'foo  - string to symbol

    if (list_length(args) < 1) {
        lyra_panic("symbol requires an argument");
    }

    Value *arg = list_index(args, 0);
    UT_string *sym = gc_alloc_string(gc);

    switch (arg->type) {
        case STRING:
            utstring_printf(sym, "%s", utstring_body(arg->data.as_string));
            break;
        case SYMBOL:
            // Symbol to symbol is identity
            return arg;
        default:
            lyra_panic("symbol: cannot convert %s to a symbol", utstring_body(repr(gc, arg)));
    }

    return make_symbol(gc, sym);
}

Value *intrinsic_module(GC *gc, Closure *closure, Value *args) {
    // (module name (export sym1 sym2 ...) body...)
    // Registers module exports and evaluates body

    if (list_length(args) < 3) {
        lyra_panic("module requires: name, export list, and body");
    }

    // Get module name (unevaluated - it's a special form)
    Value *name_val = list_index(args, 0);
    if (name_val->type != SYMBOL) {
        lyra_panic("module: name must be a symbol");
    }

    // Get export list: (export sym1 sym2 ...)
    Value *export_form = list_index(args, 1);
    if (export_form->type != CONS || list_length(export_form) < 1) {
        lyra_panic("module: expected (export ...) form");
    }

    Value *export_keyword = list_head(export_form);
    if (export_keyword->type != SYMBOL ||
        strcmp(utstring_body(export_keyword->data.as_symbol), "export") != 0) {
        lyra_panic("module: expected (export ...) form");
    }

    Value *export_syms = list_tail(export_form);

    // Evaluate body
    Value *body = list_tail(list_tail(args));
    Value *result = NIL;
    while (!is_nil(body)) {
        result = eval_s_expr(gc, closure, list_head(body));
        if (result == NULL) {
            return NIL;
        }
        body = list_tail(body);
    }

    // Register exports in *module-exports* registry
    // Format: ((module-name . ((sym1 . val1) (sym2 . val2) ...)) ...)

    // Get the root (global) closure by walking up parent chain
    Closure *root_closure = closure;
    while (root_closure->parent != NULL) {
        root_closure = root_closure->parent;
    }

    // Get or create the registry from root closure
    UT_string *registry_name = gc_alloc_string(gc);
    utstring_printf(registry_name, "*module-exports*");
    Value *registry = get_var(root_closure, registry_name);
    if (registry == NULL) {
        registry = NIL;
    }

    // Build export alist for this module
    Value *exports_alist = NIL;
    for (Value *syms = export_syms; !is_nil(syms); syms = list_tail(syms)) {
        Value *sym = list_head(syms);
        if (sym->type != SYMBOL) {
            lyra_panic("module: export list must contain only symbols");
        }

        // Get the value of this symbol
        Value *val = get_var(closure, sym->data.as_symbol);
        if (val == NULL) {
            lyra_panic("module: exported symbol '%s' not defined",
                       utstring_body(sym->data.as_symbol));
        }

        // Add (sym . val) to exports_alist
        Value *pair = NIL;
        list_push(gc, val, &pair);
        list_push(gc, sym, &pair);
        list_push(gc, pair, &exports_alist);
    }
    exports_alist = list_reverse(gc, exports_alist);

    // Add or update module entry in registry
    // For multi-file modules, merge exports with existing entry
    Value *existing_exports = NIL;
    Value *new_registry = NIL;

    for (Value *entries = registry; !is_nil(entries); entries = list_tail(entries)) {
        Value *entry = list_head(entries);
        Value *entry_name = list_head(entry);
        if (entry_name->type == SYMBOL &&
            strcmp(utstring_body(entry_name->data.as_symbol),
                   utstring_body(name_val->data.as_symbol)) == 0) {
            // Found existing module entry - save its exports for merging
            // Entry format is (name . exports), so tail gives (exports), we need car of that
            Value *exports_cell = list_tail(entry);
            if (!is_nil(exports_cell)) {
                existing_exports = list_head(exports_cell);
            }
        } else {
            // Keep other module entries
            list_push(gc, entry, &new_registry);
        }
    }
    new_registry = list_reverse(gc, new_registry);

    // Merge new exports with existing exports
    // New exports overwrite existing ones with same name
    Value *merged_exports = exports_alist;
    for (Value *existing = existing_exports; !is_nil(existing); existing = list_tail(existing)) {
        Value *existing_pair = list_head(existing);
        Value *existing_sym = list_head(existing_pair);

        // Check if this symbol is in the new exports
        int found = 0;
        for (Value *new_exp = exports_alist; !is_nil(new_exp); new_exp = list_tail(new_exp)) {
            Value *new_pair = list_head(new_exp);
            Value *new_sym = list_head(new_pair);
            if (new_sym->type == SYMBOL && existing_sym->type == SYMBOL &&
                strcmp(utstring_body(new_sym->data.as_symbol),
                       utstring_body(existing_sym->data.as_symbol)) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            // Add existing export that's not being redefined
            list_push(gc, existing_pair, &merged_exports);
        }
    }

    // Add new entry: (name . merged_exports)
    Value *module_entry = NIL;
    list_push(gc, merged_exports, &module_entry);
    list_push(gc, name_val, &module_entry);
    list_push(gc, module_entry, &new_registry);

    // Store back in root closure registry
    set_var(gc, root_closure, registry_name, new_registry);

    return result;
}

Value *intrinsic_list_directory(GC *gc, Closure *closure, Value *args) {
    (void)closure;
    // (list-directory "path") => list of filenames in directory

    if (list_length(args) < 1) {
        lyra_panic("list-directory requires a path argument");
    }

    Value *path_val = list_index(args, 0);
    if (path_val->type != STRING) {
        lyra_panic("list-directory requires a string path");
    }

    const char *path = utstring_body(path_val->data.as_string);

    char **entries = platform_list_directory(path);
    if (!entries) {
        // TODO: result
        printf("list-directory: could not open directory '%s'\n", path);
        return NIL;
    }

    Value *result = NIL;
    for (char **entry = entries; *entry != NULL; entry++) {
        UT_string *filename = gc_alloc_string(gc);
        utstring_printf(filename, "%s", *entry);
        Value *filename_val = make_string(gc, filename);

        list_push(gc, filename_val, &result);
    }
    platform_free_directory_listing(entries);

    return list_reverse(gc, result);
}

// Pattern matching macro: (match expr (pattern1 body1) (pattern2 body2) ...)
// Evaluates expr once, then tries each pattern in order
// Returns the body of the first matching pattern with variables bound
// Pattern matching rules:
// - Literal quoted values ('foo, 42, etc.) match equal values
// - Symbol '_' matches anything (wildcard, not bound)
// - Symbol starting with '*' matches remaining list elements (variadic)
// - Other symbols match anything (variable binding)
// - Lists match lists of same structure recursively
Value *intrinsic_match(GC *gc, Closure *closure, Value *args) {
    if (list_length(args) < 2) {
        lyra_panic("match requires at least an expression and one clause");
    }

    // First argument is the expression to match (unevaluated)
    Value *expr = list_head(args);

    // Evaluate the expression once in the current closure
    Value *value = eval_s_expr(gc, closure, expr);
    if (value == NULL) {
        lyra_panic("match: failed to evaluate expression");
    }

    // Rest of arguments are match clauses: ((pattern) body)
    Value *clauses = list_tail(args);

    // Try each clause in order
    for (Value *clause_list = clauses; !is_nil(clause_list); clause_list = list_tail(clause_list)) {
        Value *clause = list_head(clause_list);

        // Each clause should be a list: (pattern body...)
        if (clause->type != CONS || is_nil(clause)) {
            lyra_panic("match: invalid clause %s (expected a list)", utstring_body(repr(gc, clause)));
        }

        // Extract pattern - it's the first element of the clause
        Value *pattern = list_head(clause);

        // Check if pattern matches
        if (pattern_matches_impl(gc, pattern, value)) {
            // Pattern matches! Bind variables and evaluate body
            // Create a new closure for the bindings
            Closure *match_closure = make_closure(gc, closure);

            // Bind pattern variables
            pattern_bind_vars(gc, match_closure, pattern, value);

            // Evaluate body (rest of clause after pattern)
            Value *body = list_tail(clause);
            Value *result = NIL;
            while (!is_nil(body)) {
                result = eval_s_expr(gc, match_closure, list_head(body));
                body = list_tail(body);
            }

            return result;
        }
    }

    // No pattern matched
    lyra_panic("match: no pattern matched %s", utstring_body(repr(gc, value)));
}

// Helper function to recursively check pattern matching
static bool pattern_matches_impl(GC *gc, Value *pattern, Value *value) {
    // Check if entire pattern is a STANDALONE quoted literal: (quote value)
    // This is different from a list pattern like ((quote foo) bar baz)
    if (pattern->type == CONS && !is_nil(pattern) && list_length(pattern) == 2) {
        Value *pattern_head = list_head(pattern);
        if (pattern_head->type == SYMBOL &&
            strcmp(utstring_body(pattern_head->data.as_symbol), "quote") == 0) {
            // This is a standalone (quote something) - match the literal value
            Value *literal = list_index(pattern, 1);
            return values_equal(literal, value);
        }
    }

    // Wildcard matches anything
    if (pattern->type == SYMBOL) {
        const char *sym = utstring_body(pattern->data.as_symbol);
        if (strcmp(sym, "_") == 0) {
            return true;  // Wildcard
        }
        if (sym[0] == '*') {
            // Variadic pattern - value must be a list (or nil)
            return value->type == CONS || is_nil(value);
        }
        // Regular symbol - matches anything (variable binding)
        return true;
    }

    // Nil matches nil
    if (is_nil(pattern)) {
        return is_nil(value);
    }

    // List patterns
    if (pattern->type == CONS) {
        // Value must also be a list
        if (value->type != CONS && !is_nil(value)) {
            return false;
        }

        // Check if first element of pattern is a quoted symbol (literal match)
        Value *pattern_head = list_head(pattern);
        if (pattern_head->type == CONS && !is_nil(pattern_head)) {
            Value *pattern_head_first = list_head(pattern_head);
            if (pattern_head_first->type == SYMBOL &&
                strcmp(utstring_body(pattern_head_first->data.as_symbol), "quote") == 0) {
                // This is a quoted value - must match exactly
                Value *pattern_quoted = list_index(pattern_head, 1);
                Value *value_head = is_nil(value) ? NIL : list_head(value);

                if (!values_equal(pattern_quoted, value_head)) {
                    return false;
                }

                // Continue matching the rest
                Value *pattern_tail = list_tail(pattern);
                Value *value_tail = is_nil(value) ? NIL : list_tail(value);
                return pattern_matches_impl(gc, pattern_tail, value_tail);
            }
        }

        // Check for variadic pattern
        if (pattern_head->type == SYMBOL) {
            const char *sym = utstring_body(pattern_head->data.as_symbol);
            if (sym[0] == '*') {
                // Variadic matches rest of list - must be last in pattern
                Value *pattern_tail = list_tail(pattern);
                if (!is_nil(pattern_tail)) {
                    lyra_panic("match: variadic pattern must be last");
                }
                // Value must be a list (already checked above)
                return true;
            }
        }

        // Regular list matching - both must have same structure
        if (is_nil(value)) {
            return false;  // Pattern has elements but value is empty
        }

        // Recursively match head and tail
        if (!pattern_matches_impl(gc, pattern_head, list_head(value))) {
            return false;
        }

        return pattern_matches_impl(gc, list_tail(pattern), list_tail(value));
    }

    // Literal values (int, float, string, boolean) must match exactly
    return values_equal(pattern, value);
}

// Helper to check if two values are equal
static bool values_equal(Value *a, Value *b) {
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case INT:
            return a->data.as_int == b->data.as_int;
        case FLOAT:
            return a->data.as_float == b->data.as_float;
        case BOOLEAN:
            return a->data.as_boolean == b->data.as_boolean;
        case STRING:
            return strcmp(utstring_body(a->data.as_string), utstring_body(b->data.as_string)) == 0;
        case SYMBOL:
            return strcmp(utstring_body(a->data.as_symbol), utstring_body(b->data.as_symbol)) == 0;
        case CONS:
            if (is_nil(a) && is_nil(b)) {
                return true;
            }
            if (is_nil(a) || is_nil(b)) {
                return false;
            }
            return values_equal(list_head(a), list_head(b)) &&
                   values_equal(list_tail(a), list_tail(b));
        default:
            // For functions, macros, etc., use pointer equality
            return a == b;
    }
}

// Helper to bind variables from pattern to values
// This is called after pattern_matches_impl returns true
static void pattern_bind_vars(GC *gc, Closure *closure, Value *pattern, Value *value) {
    // Check if entire pattern is a STANDALONE quoted literal - don't bind anything
    if (pattern->type == CONS && !is_nil(pattern) && list_length(pattern) == 2) {
        Value *pattern_head = list_head(pattern);
        if (pattern_head->type == SYMBOL &&
            strcmp(utstring_body(pattern_head->data.as_symbol), "quote") == 0) {
            // Standalone quoted literal - no binding
            return;
        }
    }

    // Wildcard and literal patterns don't bind
    if (pattern->type == SYMBOL) {
        const char *sym = utstring_body(pattern->data.as_symbol);
        if (strcmp(sym, "_") == 0) {
            return;  // Wildcard - don't bind
        }
        if (sym[0] == '*') {
            // Variadic pattern - bind to remaining list
            if (strcmp(sym, "*_") != 0) {
                // Create a new symbol without the *
                UT_string *var_name = gc_alloc_string(gc);
                utstring_printf(var_name, "%s", sym + 1);
                Value *var_symbol = make_symbol(gc, var_name);
                set_var(gc, closure, var_symbol->data.as_symbol, value);
            }
            // else *_ is a wildcard variadic - don't bind
            return;
        }
        // Regular symbol - bind variable
        set_var(gc, closure, pattern->data.as_symbol, value);
        return;
    }

    // Nil and literals don't bind
    if (is_nil(pattern) || pattern->type == INT || pattern->type == FLOAT ||
        pattern->type == STRING || pattern->type == BOOLEAN) {
        return;
    }

    // List patterns - recursively bind
    if (pattern->type == CONS) {
        Value *pattern_head = list_head(pattern);

        // Handle quoted literals - don't bind
        if (pattern_head->type == CONS && !is_nil(pattern_head)) {
            Value *pattern_head_first = list_head(pattern_head);
            if (pattern_head_first->type == SYMBOL &&
                strcmp(utstring_body(pattern_head_first->data.as_symbol), "quote") == 0) {
                // Skip the literal, continue with rest
                pattern_bind_vars(gc, closure, list_tail(pattern), list_tail(value));
                return;
            }
        }

        // Handle variadic patterns
        if (pattern_head->type == SYMBOL) {
            const char *sym = utstring_body(pattern_head->data.as_symbol);
            if (sym[0] == '*') {
                // Bind variadic to rest of list
                if (strcmp(sym, "*_") != 0) {
                    UT_string *var_name = gc_alloc_string(gc);
                    utstring_printf(var_name, "%s", sym + 1);
                    Value *var_symbol = make_symbol(gc, var_name);
                    set_var(gc, closure, var_symbol->data.as_symbol, value);
                }
                return;  // Variadic is last, no more to bind
            }
        }

        // Regular list - bind head and continue with tail
        if (!is_nil(value)) {
            pattern_bind_vars(gc, closure, pattern_head, list_head(value));
            pattern_bind_vars(gc, closure, list_tail(pattern), list_tail(value));
        }
    }
}

