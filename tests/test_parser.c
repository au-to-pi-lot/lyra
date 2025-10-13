#include <check.h>
#include <stdlib.h>
#include "../src/parser.h"
#include "../src/types/value.h"
#include "../src/types/list.h"

START_TEST(test_parse_integer)
{
    Value *result = parse("42");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_parse_negative_integer)
{
    Value *result = parse("-17");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, -17);
}
END_TEST

START_TEST(test_parse_float)
{
    Value *result = parse("3.14");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, FLOAT);
    ck_assert_float_eq(result->data.as_float, 3.14);
}
END_TEST

START_TEST(test_parse_string)
{
    Value *result = parse("\"hello world\"");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, STRING);
    ck_assert_str_eq(utstring_body(result->data.as_string), "hello world");
}
END_TEST

START_TEST(test_parse_string_with_escapes)
{
    Value *result = parse("\"hello\\nworld\\t!\"");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, STRING);
    ck_assert_str_eq(utstring_body(result->data.as_string), "hello\nworld\t!");
}
END_TEST

START_TEST(test_parse_symbol)
{
    Value *result = parse("foo");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "foo");
}
END_TEST

START_TEST(test_parse_operator_symbol)
{
    Value *result = parse("+");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, SYMBOL);
    ck_assert_str_eq(utstring_body(result->data.as_symbol), "+");
}
END_TEST

START_TEST(test_parse_empty_list)
{
    Value *result = parse("()");
    ck_assert_ptr_nonnull(result);
    ck_assert(is_nil(result));
}
END_TEST

START_TEST(test_parse_simple_list)
{
    Value *result = parse("(1 2 3)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, CONS);
    ck_assert_int_eq(list_length(result), 3);

    ck_assert_int_eq(list_index(result, 0)->type, INT);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 1)->data.as_int, 2);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 3);
}
END_TEST

START_TEST(test_parse_nested_list)
{
    Value *result = parse("(1 (2 3) 4)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);

    ck_assert_int_eq(list_index(result, 0)->type, INT);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);

    Value *nested = list_index(result, 1);
    ck_assert_int_eq(nested->type, CONS);
    ck_assert_int_eq(list_length(nested), 2);
    ck_assert_int_eq(list_index(nested, 0)->data.as_int, 2);
    ck_assert_int_eq(list_index(nested, 1)->data.as_int, 3);

    ck_assert_int_eq(list_index(result, 2)->data.as_int, 4);
}
END_TEST

START_TEST(test_parse_function_call)
{
    Value *result = parse("(+ 1 2)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);

    Value *op = list_index(result, 0);
    ck_assert_int_eq(op->type, SYMBOL);
    ck_assert_str_eq(utstring_body(op->data.as_symbol), "+");

    ck_assert_int_eq(list_index(result, 1)->data.as_int, 1);
    ck_assert_int_eq(list_index(result, 2)->data.as_int, 2);
}
END_TEST

START_TEST(test_parse_with_comments)
{
    Value *result = parse("; comment\n42");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(result->type, INT);
    ck_assert_int_eq(result->data.as_int, 42);
}
END_TEST

START_TEST(test_parse_list_with_whitespace)
{
    Value *result = parse("(  1   2   3  )");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 3);
    ck_assert_int_eq(list_index(result, 0)->data.as_int, 1);
}
END_TEST

START_TEST(test_parse_mixed_types)
{
    Value *result = parse("(foo 42 \"bar\" 3.14)");
    ck_assert_ptr_nonnull(result);
    ck_assert_int_eq(list_length(result), 4);

    ck_assert_int_eq(list_index(result, 0)->type, SYMBOL);
    ck_assert_int_eq(list_index(result, 1)->type, INT);
    ck_assert_int_eq(list_index(result, 2)->type, STRING);
    ck_assert_int_eq(list_index(result, 3)->type, FLOAT);
}
END_TEST

Suite *parser_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Parser");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_parse_integer);
    tcase_add_test(tc_core, test_parse_negative_integer);
    tcase_add_test(tc_core, test_parse_float);
    tcase_add_test(tc_core, test_parse_string);
    tcase_add_test(tc_core, test_parse_string_with_escapes);
    tcase_add_test(tc_core, test_parse_symbol);
    tcase_add_test(tc_core, test_parse_operator_symbol);
    tcase_add_test(tc_core, test_parse_empty_list);
    tcase_add_test(tc_core, test_parse_simple_list);
    tcase_add_test(tc_core, test_parse_nested_list);
    tcase_add_test(tc_core, test_parse_function_call);
    tcase_add_test(tc_core, test_parse_with_comments);
    tcase_add_test(tc_core, test_parse_list_with_whitespace);
    tcase_add_test(tc_core, test_parse_mixed_types);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = parser_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
