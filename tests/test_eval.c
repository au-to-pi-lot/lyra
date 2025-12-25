#include <check.h>
#include <stdlib.h>
#include "../src/eval.h"
#include "../src/parser.h"
#include "../src/prelude.h"
#include "../src/closure.h"
#include "../src/types/value.h"
#include "../src/types/list.h"
#include "../src/gc.h"

// Helper to parse and evaluate
static Value* eval_string(GC *gc, Closure *closure, const char *input) {
    Value *parsed = parse(gc, input);
    if (!parsed) return NULL;
    return eval_s_expr(gc, closure, parsed);
}

START_TEST(test_prelude)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_integer)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "42");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_float)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "3.14");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FLOAT);
    ck_assert_float_eq(result->data.as_float, 3.14);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_string)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "\"hello\"");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, STRING);
    ck_assert_str_eq(utstring_body(result->data.as_string), "hello");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_add)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(+ 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_add_multiple)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(+ 1 2 3 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 10);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_sub)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(- 10 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 7);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_mul)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(* 3 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 12);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_div)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(/ 20 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FLOAT);
    ck_assert_float_eq(result->data.as_float, 5.0);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_nested_arithmetic)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(+ (* 2 3) (* 4 5))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 26);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quote)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(quote (1 2 3))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quote_symbol)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Quote should return the symbol itself, not evaluate it
    Value *result = eval_string(&gc, closure, "(quote x)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "x");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quote_number)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Quote of a number should return the number
    Value *result = eval_string(&gc, closure, "(quote 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quote_nested)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Quote should preserve nested structure without evaluation
    Value *result = eval_string(&gc, closure, "(quote (+ 1 2))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);

    // First element should be the + symbol, not evaluated
    Value *first = list_index(result, 0);
    ck_assert_int_eq(first->type, SYMBOL);
    ck_assert_str_eq(utstring_body(first->data.as_symbol), "+");

    // Numbers should be preserved
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 2);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quote_empty_list)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Quote of empty list should return nil
    Value *result = eval_string(&gc, closure, "(quote ())");
    ck_assert_ptr_nonnull(result);
    ck_assert(is_nil(result));

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_define)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define x 42)");
    Value *result = eval_string(&gc, closure, "x");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_if_true)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(if true 10 20)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 10);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_if_false)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(if false 10 20)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 20);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(lambda (x) x)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FUNCTION);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_call)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define identity (lambda (x) x))");
    Value *result = eval_string(&gc, closure, "(identity 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_with_arithmetic)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define double (lambda (x) (* x 2)))");
    Value *result = eval_string(&gc, closure, "(double 21)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_closure)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define x 10)");
    eval_string(&gc, closure, "(define add-x (lambda (y) (+ x y)))");
    Value *result = eval_string(&gc, closure, "(add-x 5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 15);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_z_combinator_factorial)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Define Z combinator (call-by-value Y combinator)
    // The extra lambda delays evaluation
    eval_string(&gc, closure,
        "(define Z (lambda (f) "
        "  ((lambda (x) (f (lambda (v) ((x x) v)))) "
        "   (lambda (x) (f (lambda (v) ((x x) v)))))))");

    // Define factorial using Z
    eval_string(&gc, closure,
        "(define factorial "
        "  (Z (lambda (self) "
        "       (lambda (n) "
        "         (if (= n 0) "
        "             1 "
        "             (* n (self (- n 1))))))))");

    // Test factorial(5) = 120
    Value *result = eval_string(&gc, closure, "(factorial 5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 120);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quasiquote_simple)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Quasiquote without unquote is like quote
    Value *result = eval_string(&gc, closure, "`(1 2 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quasiquote_with_unquote)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define x 42)");

    // Unquote evaluates the expression
    Value *result = eval_string(&gc, closure, "`(1 ,x 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 42);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_quasiquote_nested)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define x 10)");

    // Nested lists with unquote
    Value *result = eval_string(&gc, closure, "`(+ ,x (+ 1 2))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);

    // First element is symbol +
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    // Second element is 10 (unquoted x)
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 10);
    // Third element is unevaluated list (+ 1 2)
    Value *third = list_index(result, 2);
    ck_assert_int_eq(third->type, CONS);
    ck_assert_int_eq(list_length(third), 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_unquote_splicing)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    eval_string(&gc, closure, "(define xs (quote (2 3 4)))");

    // Unquote-splicing splices a list into the template
    Value *result = eval_string(&gc, closure, "`(1 ,@xs 5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 5);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 4);
    ck_assert_int_eq(list_index(result, 4)->data.as_int, 5);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_fn_macro_simple)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Define identity function using fn macro
    eval_string(&gc, closure, "(fn id (x) x)");

    // Test it
    Value *result = eval_string(&gc, closure, "(id 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_fn_macro_factorial)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Define factorial using fn macro (more convenient than Z combinator)
    eval_string(&gc, closure,
        "(fn factorial (n) "
        "  (if (= n 0) "
        "      1 "
        "      (* n (factorial (- n 1)))))");

    // Test factorial(5) = 120
    Value *result = eval_string(&gc, closure, "(factorial 5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 120);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_variadic_args)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Define function that collects all args into a list
    eval_string(&gc, closure, "(define collect (lambda (*args) args))");

    // Test it
    Value *result = eval_string(&gc, closure, "(collect 1 2 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_destructure_simple)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Destructure a pair: (lambda ((a b)) ...)
    eval_string(&gc, closure,
        "(define get-first (lambda ((a b)) a))");
    Value *result = eval_string(&gc, closure, "(get-first '(10 20))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 10);

    eval_string(&gc, closure,
        "(define get-second (lambda ((a b)) b))");
    result = eval_string(&gc, closure, "(get-second '(10 20))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 20);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_destructure_nested)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Nested destructuring: (lambda ((a (b c))) ...)
    eval_string(&gc, closure,
        "(define extract (lambda ((a (b c))) (cons a (cons b (cons c '())))))");
    Value *result = eval_string(&gc, closure, "(extract '(1 (2 3)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_destructure_mixed)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Mix destructured and regular params: (lambda (x (a b) y) ...)
    eval_string(&gc, closure,
        "(define mixed (lambda (x (a b) y) (cons x (cons a (cons b (cons y '()))))))");
    Value *result = eval_string(&gc, closure, "(mixed 1 '(2 3) 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 4);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 4);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_lambda_destructure_variadic)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Destructure with variadic: (lambda ((a *rest)) ...)
    eval_string(&gc, closure,
        "(define get-tail (lambda ((a *rest)) rest))");
    Value *result = eval_string(&gc, closure, "(get-tail '(1 2 3 4 5))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 4);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 3);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 4);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 5);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_eval_apply)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Test apply with list of arguments
    Value *result = eval_string(&gc, closure, "(apply + (quote (1 2 3)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 6);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_foldl)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Test foldl with sum: (foldl f acc lst)
    Value *result = eval_string(&gc, closure, "(foldl + 0 (quote (1 2 3 4 5)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 15);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_foldr)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Test foldr preserves list order when building with cons
    // foldr cons '(1 2 3) '() → (cons 1 (cons 2 (cons 3 '()))) → (1 2 3)
    Value *result = eval_string(&gc, closure,
        "(foldr cons (quote (1 2 3)) (quote ()))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_foldl_vs_foldr)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Compare foldl and foldr with cons
    // foldl needs flipped arguments: (lambda (acc x) (cons x acc))
    // This builds list in reverse order
    Value *foldl_result = eval_string(&gc, closure,
        "(foldl (lambda (acc x) (cons x acc)) (quote ()) (quote (1 2 3)))");
    ck_assert_ptr_nonnull(foldl_result);
    ck_assert_int_eq(list_length(foldl_result), 3);
    ck_assert_int_eq(list_index(foldl_result, 0)->data.as_int, 3);
    ck_assert_int_eq(list_index(foldl_result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(foldl_result, 2)->data.as_int, 1);

    // foldr with cons preserves order
    Value *foldr_result = eval_string(&gc, closure,
        "(foldr cons (quote (1 2 3)) (quote ()))");
    ck_assert_ptr_nonnull(foldr_result);
    ck_assert_int_eq(list_length(foldr_result), 3);
    ck_assert_int_eq(list_index(foldr_result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(foldr_result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(foldr_result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_reverse)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    Value *result = eval_string(&gc, closure, "(reverse (quote (1 2 3 4 5)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 5);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 5);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 4);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 4)->data.as_int, 1);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_map)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Map doubling function over list
    Value *result = eval_string(&gc, closure,
        "(map (lambda (x) (* x 2)) (quote (1 2 3 4 5)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 5);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 4);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 6);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 8);
    ck_assert_int_eq(list_index(result, 4)->data.as_int, 10);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_filter)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Filter even numbers
    eval_string(&gc, closure,
        "(define even? (lambda (n) (= (car (cdr (divmod n 2))) 0)))");
    Value *result = eval_string(&gc, closure,
        "(filter even? (quote (1 2 3 4 5 6 7 8 9 10)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 5);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 4);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 6);
    ck_assert_int_eq(list_index(result, 3)->data.as_int, 8);
    ck_assert_int_eq(list_index(result, 4)->data.as_int, 10);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_let_macro)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Test let macro with multiple bindings
    Value *result = eval_string(&gc, closure,
        "(let ((x 10) (y 20)) (+ x y))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 30);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_gensym)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Test gensym generates unique symbols
    eval_string(&gc, closure, "(define s1 (gensym))");
    eval_string(&gc, closure, "(define s2 (gensym))");
    eval_string(&gc, closure, "(define s3 (gensym))");

    // Verify they're different by checking if they're not equal
    Value *result = eval_string(&gc, closure, "(= s1 s2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert(result->data.as_boolean == false);

    result = eval_string(&gc, closure, "(= s2 s3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert(result->data.as_boolean == false);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_gensym_in_macro)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Define a hygienic macro using gensym
    eval_string(&gc, closure,
        "(define-macro safe-double (lambda (x) "
        "  (define tmp-sym (gensym)) "
        "  `(let ((,tmp-sym ,x)) "
        "     (+ ,tmp-sym ,tmp-sym))))");

    // Test the macro
    Value *result = eval_string(&gc, closure, "(safe-double 21)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_not)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // not true => false
    Value *result = eval_string(&gc, closure, "(not true)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // not false => true
    result = eval_string(&gc, closure, "(not false)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    // not of truthy value (non-false) => false
    result = eval_string(&gc, closure, "(not 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // not of nil
    result = eval_string(&gc, closure, "(not nil)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_and_short_circuit)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // and with no args => true
    Value *result = eval_string(&gc, closure, "(and)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    // and with one arg => that arg
    result = eval_string(&gc, closure, "(and 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    // and with all true => last value
    result = eval_string(&gc, closure, "(and true 1 2 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 3);

    // and with false => false (returns first false)
    result = eval_string(&gc, closure, "(and true false 'unreachable)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Test short-circuiting: gensym should NOT be called if first arg is false
    result = eval_string(&gc, closure, "(gensym)");  // Establish baseline G__0
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__0");

    result = eval_string(&gc, closure, "(and false (gensym))");  // gensym should not execute
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    result = eval_string(&gc, closure, "(gensym)");  // Should still be G__1, not G__2
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__1");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_or_short_circuit)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // or with no args => false
    Value *result = eval_string(&gc, closure, "(or)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // or with one arg => that arg
    result = eval_string(&gc, closure, "(or 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    // or with first true => first truthy value
    result = eval_string(&gc, closure, "(or false false 42 'unreachable)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    // or with all false => false
    result = eval_string(&gc, closure, "(or false false false)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Test short-circuiting: gensym should NOT be called if first arg is truthy
    result = eval_string(&gc, closure, "(gensym)");  // Establish baseline G__0
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__0");

    result = eval_string(&gc, closure, "(or 42 (gensym))");  // gensym should not execute
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    result = eval_string(&gc, closure, "(gensym)");  // Should still be G__1, not G__2
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__1");

    // Test that 'or' doesn't evaluate first arg twice (important for side effects)
    result = eval_string(&gc, closure, "(or (gensym) 'unreachable)");  // Uses G__2
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__2");

    result = eval_string(&gc, closure, "(gensym)");  // Should be G__3, proving gensym in 'or' was only called once
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__3");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_boolean_combinations)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Complex boolean expressions
    Value *result = eval_string(&gc, closure, "(and (or false true) (not false))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(or (and false true) (and true true))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(not (or false false))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_less_than)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Integer comparisons
    Value *result = eval_string(&gc, closure, "(< 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(< 2 1)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    result = eval_string(&gc, closure, "(< 2 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Float comparisons
    result = eval_string(&gc, closure, "(< 1.5 2.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(< 2.5 1.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Negative numbers
    result = eval_string(&gc, closure, "(< -5 -3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(< -3 -5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_less_than_or_equal)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Integer comparisons
    Value *result = eval_string(&gc, closure, "(<= 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(<= 2 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(<= 3 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Float comparisons
    result = eval_string(&gc, closure, "(<= 1.5 2.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(<= 2.5 2.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_greater_than)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Integer comparisons
    Value *result = eval_string(&gc, closure, "(> 2 1)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(> 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    result = eval_string(&gc, closure, "(> 2 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Float comparisons
    result = eval_string(&gc, closure, "(> 2.5 1.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_greater_than_or_equal)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Integer comparisons
    Value *result = eval_string(&gc, closure, "(>= 2 1)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(>= 2 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(>= 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, false);

    // Float comparisons
    result = eval_string(&gc, closure, "(>= 2.5 1.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    result = eval_string(&gc, closure, "(>= 2.5 2.5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, BOOLEAN);
    ck_assert_int_eq(result->data.as_boolean, true);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_literal_int)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match literal integer
    Value *result = eval_string(&gc, closure,
        "(match 42 "
        "  (0 'zero) "
        "  (42 'forty-two) "
        "  (x 'other))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "forty-two");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_literal_symbol)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match literal symbol - use 'symbol to match specific symbol
    Value *result = eval_string(&gc, closure,
        "(match 'lambda "
        "  ('if 'if-form) "
        "  ('lambda 'lambda-form) "
        "  ('define 'define-form) "
        "  (x 'other))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "lambda-form");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_variable_binding)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match with variable binding (catch-all)
    Value *result = eval_string(&gc, closure,
        "(match 99 "
        "  (0 'zero) "
        "  (x x))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 99);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_list_pattern_fixed)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match list with fixed structure: ('lambda params body)
    Value *result = eval_string(&gc, closure,
        "(match '(lambda (x) (+ x 1)) "
        "  (('lambda params body) params) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 1);
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "x");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_list_pattern_variadic)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match list with variadic pattern: ('apply fn *args)
    Value *result = eval_string(&gc, closure,
        "(match '(apply + 1 2 3) "
        "  (('apply fn *args) args) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_nested_patterns)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match nested structure: ('if cond then else)
    Value *result = eval_string(&gc, closure,
        "(match '(if (= x 0) 1 2) "
        "  (('if cond then else) cond) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    // First element should be '='
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "=");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_with_guard)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match with guard condition (using if in the body)
    eval_string(&gc, closure,
        "(define even? (lambda (n) (= (car (cdr (divmod n 2))) 0)))");
    Value *result = eval_string(&gc, closure,
        "(match 4 "
        "  (n (if (even? n) 'even 'odd)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "even");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_empty_list)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match empty list (nil) - () in pattern context is literal nil
    Value *result = eval_string(&gc, closure,
        "(match '() "
        "  (() 'empty) "
        "  (x 'not-empty))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "empty");

    // Also test matching nil token
    result = eval_string(&gc, closure,
        "(match nil "
        "  (() 'empty) "
        "  (x 'not-empty))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "empty");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_nested_empty_list)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Match list containing nil: (())
    // Pattern (()) matches a list with one element that is nil
    Value *result = eval_string(&gc, closure,
        "(match '(()) "
        "  (() 'empty-list) "
        "  ((()) 'list-containing-nil) "
        "  (x 'other))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "list-containing-nil");

    // Match list containing nil and other elements
    result = eval_string(&gc, closure,
        "(match '(() 1 2) "
        "  ((() x y) (cons x (cons y '()))) "
        "  (other 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 2);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_wildcard)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Wildcard _ matches anything but doesn't bind
    // Extract body from lambda, ignoring params
    Value *result = eval_string(&gc, closure,
        "(match '(lambda (x y) (+ x y)) "
        "  (('lambda _ body) body) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "+");

    // Multiple wildcards in same pattern
    result = eval_string(&gc, closure,
        "(match '(1 2 3 4 5) "
        "  ((a _ c _ e) (cons a (cons c (cons e '())))) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 3);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 5);

    // Wildcard in nested pattern
    result = eval_string(&gc, closure,
        "(match '(if (= x 0) 42 99) "
        "  (('if _ then _) then) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);

    // Wildcard with variadic
    result = eval_string(&gc, closure,
        "(match '(define foo (lambda (x) x)) "
        "  (('define name _) name) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "foo");

    // Variadic wildcard *_ - matches rest but doesn't bind
    result = eval_string(&gc, closure,
        "(match '(+ 1 2 3 4 5) "
        "  (('+ x *_) x) "
        "  (other 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 1);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_first_match_wins)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // First matching pattern should win
    Value *result = eval_string(&gc, closure,
        "(match 42 "
        "  (x 'first) "
        "  (42 'second) "
        "  (y 'third))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "first");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_compile_expr)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Realistic example: compile-expr function
    eval_string(&gc, closure,
        "(fn compile-expr (expr) "
        "  (match expr "
        "    (('lambda params body) (cons 'LAMBDA (cons params (cons body '())))) "
        "    (('if cond then else) (cons 'IF (cons cond (cons then (cons else '()))))) "
        "    (('define name val) (cons 'DEFINE (cons name (cons val '())))) "
        "    (other (cons 'LIT (cons other '())))))");

    // Test compiling lambda
    Value *result = eval_string(&gc, closure,
        "(compile-expr '(lambda (x) x))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "LAMBDA");

    // Test compiling if
    result = eval_string(&gc, closure,
        "(compile-expr '(if true 1 2))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "IF");

    // Test compiling literal
    result = eval_string(&gc, closure,
        "(compile-expr 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_str_eq(utstring_body(list_index(result, 0)->data.as_symbol), "LIT");
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 42);

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_list_length_mismatch)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Pattern with 3 elements shouldn't match list with 2 elements
    Value *result = eval_string(&gc, closure,
        "(match '(lambda (x)) "
        "  (('lambda params body) 'matched-three) "
        "  (x 'no-match))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "no-match");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_nested)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Nested match expressions - match inside match body
    eval_string(&gc, closure,
        "(fn classify (expr) "
        "  (match expr "
        "    (('lambda params body) "
        "      (match params "
        "        (('*args) 'variadic-lambda) "
        "        ((x) 'unary-lambda) "
        "        (args 'multi-lambda))) "
        "    (('define name val) "
        "      (match val "
        "        (('lambda p b) 'function-def) "
        "        (v 'variable-def))) "
        "    (other 'unknown)))");

    // Test variadic lambda
    Value *result = eval_string(&gc, closure,
        "(classify '(lambda (*args) args))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "variadic-lambda");

    // Test unary lambda
    result = eval_string(&gc, closure,
        "(classify '(lambda (x) x))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "unary-lambda");

    // Test multi-arg lambda
    result = eval_string(&gc, closure,
        "(classify '(lambda (x y z) x))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "multi-lambda");

    // Test function definition
    result = eval_string(&gc, closure,
        "(classify '(define foo (lambda (x) x)))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "function-def");

    // Test variable definition
    result = eval_string(&gc, closure,
        "(classify '(define x 42))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "variable-def");

    // Test unknown form
    result = eval_string(&gc, closure,
        "(classify '(if true 1 2))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "unknown");

    gc_free_all(&gc);
}
END_TEST

START_TEST(test_match_no_side_effects_in_unmatched)
{
    GC gc;
    gc_init(&gc);
    Closure *closure = make_closure(&gc, NULL);
    prelude(&gc, closure);

    // Side effects (gensym) should NOT occur in unmatched branches
    // gensym increments gc->gensym_counter, which is a real side effect

    // First, establish baseline - gensym should produce G__0
    Value *result = eval_string(&gc, closure, "(gensym)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__0");

    // Match on first pattern - gensym should only be called once (in matched branch)
    result = eval_string(&gc, closure,
        "(match 1 "
        "  (1 (gensym)) "
        "  (2 (gensym)) "
        "  (x (gensym)))");

    // Should be G__1 (only the matched branch executed)
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__1");

    // Next gensym should be G__2, not G__4 (proving unmatched branches didn't execute)
    result = eval_string(&gc, closure, "(gensym)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__2");

    // Match on second pattern
    result = eval_string(&gc, closure,
        "(match 2 "
        "  (1 (gensym)) "
        "  (2 (gensym)) "
        "  (x (gensym)))");

    // Should be G__3 (only second branch executed)
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__3");

    // Next gensym should be G__4
    result = eval_string(&gc, closure, "(gensym)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "G__4");

    gc_free_all(&gc);
}
END_TEST

Suite *eval_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Eval");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_prelude);
    tcase_add_test(tc_core, test_eval_integer);
    tcase_add_test(tc_core, test_eval_float);
    tcase_add_test(tc_core, test_eval_string);
    tcase_add_test(tc_core, test_eval_add);
    tcase_add_test(tc_core, test_eval_add_multiple);
    tcase_add_test(tc_core, test_eval_sub);
    tcase_add_test(tc_core, test_eval_mul);
    tcase_add_test(tc_core, test_eval_div);
    tcase_add_test(tc_core, test_eval_nested_arithmetic);
    tcase_add_test(tc_core, test_eval_quote);
    tcase_add_test(tc_core, test_eval_quote_symbol);
    tcase_add_test(tc_core, test_eval_quote_number);
    tcase_add_test(tc_core, test_eval_quote_nested);
    tcase_add_test(tc_core, test_eval_quote_empty_list);
    tcase_add_test(tc_core, test_eval_define);
    tcase_add_test(tc_core, test_eval_if_true);
    tcase_add_test(tc_core, test_eval_if_false);
    tcase_add_test(tc_core, test_eval_lambda);
    tcase_add_test(tc_core, test_eval_lambda_call);
    tcase_add_test(tc_core, test_eval_lambda_with_arithmetic);
    tcase_add_test(tc_core, test_eval_closure);
    tcase_add_test(tc_core, test_eval_z_combinator_factorial);
    tcase_add_test(tc_core, test_eval_quasiquote_simple);
    tcase_add_test(tc_core, test_eval_quasiquote_with_unquote);
    tcase_add_test(tc_core, test_eval_quasiquote_nested);
    tcase_add_test(tc_core, test_eval_unquote_splicing);
    tcase_add_test(tc_core, test_eval_fn_macro_simple);
    tcase_add_test(tc_core, test_eval_fn_macro_factorial);
    tcase_add_test(tc_core, test_eval_variadic_args);
    tcase_add_test(tc_core, test_eval_lambda_destructure_simple);
    tcase_add_test(tc_core, test_eval_lambda_destructure_nested);
    tcase_add_test(tc_core, test_eval_lambda_destructure_mixed);
    tcase_add_test(tc_core, test_eval_lambda_destructure_variadic);
    tcase_add_test(tc_core, test_eval_apply);
    tcase_add_test(tc_core, test_foldl);
    tcase_add_test(tc_core, test_foldr);
    tcase_add_test(tc_core, test_foldl_vs_foldr);
    tcase_add_test(tc_core, test_reverse);
    tcase_add_test(tc_core, test_map);
    tcase_add_test(tc_core, test_filter);
    tcase_add_test(tc_core, test_let_macro);
    tcase_add_test(tc_core, test_gensym);
    tcase_add_test(tc_core, test_gensym_in_macro);
    tcase_add_test(tc_core, test_not);
    tcase_add_test(tc_core, test_and_short_circuit);
    tcase_add_test(tc_core, test_or_short_circuit);
    tcase_add_test(tc_core, test_boolean_combinations);
    tcase_add_test(tc_core, test_less_than);
    tcase_add_test(tc_core, test_less_than_or_equal);
    tcase_add_test(tc_core, test_greater_than);
    tcase_add_test(tc_core, test_greater_than_or_equal);
    tcase_add_test(tc_core, test_match_literal_int);
    tcase_add_test(tc_core, test_match_literal_symbol);
    tcase_add_test(tc_core, test_match_variable_binding);
    tcase_add_test(tc_core, test_match_list_pattern_fixed);
    tcase_add_test(tc_core, test_match_list_pattern_variadic);
    tcase_add_test(tc_core, test_match_nested_patterns);
    tcase_add_test(tc_core, test_match_with_guard);
    tcase_add_test(tc_core, test_match_empty_list);
    tcase_add_test(tc_core, test_match_nested_empty_list);
    tcase_add_test(tc_core, test_match_wildcard);
    tcase_add_test(tc_core, test_match_first_match_wins);
    tcase_add_test(tc_core, test_match_compile_expr);
    tcase_add_test(tc_core, test_match_list_length_mismatch);
    tcase_add_test(tc_core, test_match_nested);
    tcase_add_test(tc_core, test_match_no_side_effects_in_unmatched);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = eval_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
