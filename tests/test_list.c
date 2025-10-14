#include <check.h>
#include <stdlib.h>
#include "../src/types/list.h"
#include "../src/types/value.h"
#include "../src/gc.h"

START_TEST(test_nil)
{
    Value *nil = NIL;
    ck_assert_ptr_nonnull(nil);
    ck_assert_int_eq(nil->type, CONS);
    ck_assert_ptr_null(nil->data.as_cons);
}
END_TEST

START_TEST(test_is_nil)
{
    Value *nil = NIL;
    ck_assert(is_nil(nil));
}
END_TEST

START_TEST(test_is_not_nil)
{
    GC gc;
    gc_init(&gc);
    Value *not_nil = make_list(&gc, 1);
    ck_assert(!is_nil(not_nil));
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_length_empty)
{
    Value *nil = NIL;
    ck_assert_int_eq(list_length(nil), 0);
}
END_TEST

START_TEST(test_make_list)
{
    GC gc;
    gc_init(&gc);
    Value *list = make_list(&gc, 3);
    ck_assert_ptr_nonnull(list);
    ck_assert_int_eq(list_length(list), 3);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_head_nonempty)
{
    GC gc;
    gc_init(&gc);
    Value *list = make_list(&gc, 1);
    Value *first = gc_alloc_value(&gc, INT, (ValueData){.as_int = 4});
    list->data.as_cons->car = first;

    Value *head = list_head(list);
    ck_assert_ptr_eq(head, first);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_head_empty)
{
    Value *list = NIL;
    Value *head = list_head(list);
    ck_assert(is_nil(head));
}
END_TEST

START_TEST(test_list_copy_nonempty)
{
    GC gc;
    gc_init(&gc);
    Value *original = list_range(&gc, 3);
    Value *copy = list_copy(&gc, original);

    ck_assert_ptr_ne(copy, original);
    ck_assert_ptr_nonnull(copy);
    ck_assert(list_index(copy, 0)->data.as_int == 0);
    ck_assert(list_index(copy, 1)->data.as_int == 1);
    ck_assert(list_index(copy, 2)->data.as_int == 2);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_copy_nil)
{
    GC gc;
    gc_init(&gc);
    Value *nil = NIL;
    Value *copy = list_copy(&gc, nil);
    ck_assert_ptr_eq(copy, nil);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_append)
{
    GC gc;
    gc_init(&gc);
    Value *nil = NIL;
    Value *item = gc_alloc_value(&gc, INT, (ValueData){.as_int = 42});
    Value *list = list_append(&gc, item, nil);

    ck_assert_ptr_nonnull(list);
    ck_assert_int_eq(list_length(list), 1);
    ck_assert_ptr_eq(list_head(list), item);
    ck_assert(is_nil(list_tail(list)));
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_tail_nonempty)
{
    GC gc;
    gc_init(&gc);
    Value *item1 = gc_alloc_value(&gc, INT, (ValueData){.as_int = 1});
    Value *item2 = gc_alloc_value(&gc, INT, (ValueData){.as_int = 2});
    Value *nil = NIL;
    Value *list = list_append(&gc, item1, list_append(&gc, item2, nil));

    Value *tail = list_tail(list);
    ck_assert_ptr_nonnull(tail);
    ck_assert_ptr_eq(list_head(tail), item2);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_tail_empty)
{
    Value *nil = NIL;
    Value *tail = list_tail(nil);
    ck_assert(is_nil(tail));
}
END_TEST

START_TEST(test_list_drop)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 5);
    Value *dropped = list_drop(list, 2);

    ck_assert_int_eq(list_length(dropped), 3);
    ck_assert_int_eq(list_head(dropped)->data.as_int, 2);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_drop_all)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *dropped = list_drop(list, 5);
    ck_assert(is_nil(dropped));
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_drop_zero)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *dropped = list_drop(list, 0);
    ck_assert_ptr_eq(dropped, list);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_keep)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 5);
    Value *kept = list_keep(&gc, list, 3);

    ck_assert_int_eq(list_length(kept), 3);
    ck_assert_int_eq(list_head(kept)->data.as_int, 0);
    ck_assert_int_eq(list_index(kept, 1)->data.as_int, 1);
    ck_assert_int_eq(list_index(kept, 2)->data.as_int, 2);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_keep_zero)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *kept = list_keep(&gc, list, 0);
    ck_assert(is_nil(kept));
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_keep_more_than_length)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *kept = list_keep(&gc, list, 10);
    ck_assert_int_eq(list_length(kept), 3);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_index_valid)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 5);
    ck_assert_int_eq(list_index(list, 0)->data.as_int, 0);
    ck_assert_int_eq(list_index(list, 2)->data.as_int, 2);
    ck_assert_int_eq(list_index(list, 4)->data.as_int, 4);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_index_out_of_bounds)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *result = list_index(list, 10);
    ck_assert(is_nil(result));
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_concat)
{
    GC gc;
    gc_init(&gc);
    Value *left = list_range(&gc, 3);
    Value *right = list_range(&gc, 2);
    Value *concatenated = list_concat(&gc, left, right);

    ck_assert_int_eq(list_length(concatenated), 5);
    ck_assert_int_eq(list_index(concatenated, 0)->data.as_int, 0);
    ck_assert_int_eq(list_index(concatenated, 2)->data.as_int, 2);
    ck_assert_int_eq(list_index(concatenated, 3)->data.as_int, 0);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_concat_nil_left)
{
    GC gc;
    gc_init(&gc);
    Value *nil = NIL;
    Value *right = list_range(&gc, 2);
    Value *concatenated = list_concat(&gc, nil, right);
    ck_assert_ptr_eq(concatenated, right);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_concat_nil_right)
{
    GC gc;
    gc_init(&gc);
    Value *left = list_range(&gc, 2);
    Value *nil = NIL;
    Value *concatenated = list_concat(&gc, left, nil);
    ck_assert_ptr_eq(concatenated, left);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_reverse_nonempty)
{
    GC gc;
    gc_init(&gc);
    Value *list = list_range(&gc, 3);
    Value *reversed = list_reverse(&gc, list);

    ck_assert_ptr_nonnull(reversed);
    ck_assert_int_eq(list_length(reversed), 3);
    ck_assert_int_eq(list_index(reversed, 0)->data.as_int, 2);
    ck_assert_int_eq(list_index(reversed, 1)->data.as_int, 1);
    ck_assert_int_eq(list_index(reversed, 2)->data.as_int, 0);
    gc_free_all(&gc);
}
END_TEST

START_TEST(test_list_reverse_empty)
{
    GC gc;
    gc_init(&gc);
    Value *list = NIL;
    Value *reversed = list_reverse(&gc, list);

    ck_assert_ptr_nonnull(reversed);
    ck_assert_int_eq(list_length(reversed), 0);
    gc_free_all(&gc);
}
END_TEST

Suite *list_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("List");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_nil);
    tcase_add_test(tc_core, test_is_nil);
    tcase_add_test(tc_core, test_is_not_nil);
    tcase_add_test(tc_core, test_list_length_empty);
    tcase_add_test(tc_core, test_make_list);
    tcase_add_test(tc_core, test_list_head_nonempty);
    tcase_add_test(tc_core, test_list_head_empty);
    tcase_add_test(tc_core, test_list_copy_nonempty);
    tcase_add_test(tc_core, test_list_copy_nil);
    tcase_add_test(tc_core, test_list_append);
    tcase_add_test(tc_core, test_list_tail_nonempty);
    tcase_add_test(tc_core, test_list_tail_empty);
    tcase_add_test(tc_core, test_list_drop);
    tcase_add_test(tc_core, test_list_drop_all);
    tcase_add_test(tc_core, test_list_drop_zero);
    tcase_add_test(tc_core, test_list_keep);
    tcase_add_test(tc_core, test_list_keep_zero);
    tcase_add_test(tc_core, test_list_keep_more_than_length);
    tcase_add_test(tc_core, test_list_index_valid);
    tcase_add_test(tc_core, test_list_index_out_of_bounds);
    tcase_add_test(tc_core, test_list_concat);
    tcase_add_test(tc_core, test_list_concat_nil_left);
    tcase_add_test(tc_core, test_list_concat_nil_right);
    tcase_add_test(tc_core, test_list_reverse_nonempty);
    tcase_add_test(tc_core, test_list_reverse_empty);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = list_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
