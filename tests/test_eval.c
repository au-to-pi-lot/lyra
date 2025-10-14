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

Suite *eval_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Eval");
    tc_core = tcase_create("Core");

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
