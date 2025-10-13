#include <check.h>
#include <stdlib.h>
#include "../src/eval.h"
#include "../src/parser.h"
#include "../src/prelude.h"
#include "../src/closure.h"
#include "../src/types/value.h"
#include "../src/types/list.h"

// Helper to parse and evaluate
static Value* eval_string(Closure *closure, const char *input) {
    Value *parsed = parse(input);
    if (!parsed) return NULL;
    return evaluate(closure, parsed);
}

START_TEST(test_eval_integer)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "42");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_eval_float)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "3.14");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FLOAT);
    ck_assert_float_eq(result->data.as_float, 3.14);
}
END_TEST

START_TEST(test_eval_string)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "\"hello\"");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, STRING);
    ck_assert_str_eq(utstring_body(result->data.as_string), "hello");
}
END_TEST

START_TEST(test_eval_add)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(+ 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 3);
}
END_TEST

START_TEST(test_eval_add_multiple)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(+ 1 2 3 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 10);
}
END_TEST

START_TEST(test_eval_sub)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(- 10 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 7);
}
END_TEST

START_TEST(test_eval_mul)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(* 3 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 12);
}
END_TEST

START_TEST(test_eval_div)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(/ 20 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FLOAT);
    ck_assert_float_eq(result->data.as_float, 5.0);
}
END_TEST

START_TEST(test_eval_nested_arithmetic)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(+ (* 2 3) (* 4 5))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 26);
}
END_TEST

START_TEST(test_eval_quote)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(quote (1 2 3))");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);
}
END_TEST

START_TEST(test_eval_define)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    eval_string(closure, "(define x 42)");
    Value *result = eval_string(closure, "x");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_eval_if_true)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(if true 10 20)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 10);
}
END_TEST

START_TEST(test_eval_if_false)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(if false 10 20)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 20);
}
END_TEST

START_TEST(test_eval_lambda)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    Value *result = eval_string(closure, "(lambda (x) x)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FUNCTION);
}
END_TEST

START_TEST(test_eval_lambda_call)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    eval_string(closure, "(define identity (lambda (x) x))");
    Value *result = eval_string(closure, "(identity 42)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_eval_lambda_with_arithmetic)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    eval_string(closure, "(define double (lambda (x) (* x 2)))");
    Value *result = eval_string(closure, "(double 21)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_eval_closure)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    eval_string(closure, "(define x 10)");
    eval_string(closure, "(define add-x (lambda (y) (+ x y)))");
    Value *result = eval_string(closure, "(add-x 5)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 15);
}
END_TEST

START_TEST(test_eval_z_combinator_factorial)
{
    Closure *closure = make_closure(NULL);
    prelude(closure);

    // Define Z combinator (call-by-value Y combinator)
    // The extra lambda delays evaluation
    eval_string(closure,
        "(define Z (lambda (f) "
        "  ((lambda (x) (f (lambda (v) ((x x) v)))) "
        "   (lambda (x) (f (lambda (v) ((x x) v)))))))");

    // Define factorial using Z
    eval_string(closure,
        "(define factorial "
        "  (Z (lambda (self) "
        "       (lambda (n) "
        "         (if (= n 0) "
        "             1 "
        "             (* n (self (- n 1))))))))");

    // Test factorial(1) = 1 (simpler test)
    Value *result = eval_string(closure, "(factorial 1)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 1);
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
