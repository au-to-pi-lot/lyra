// TEMPORARY: deliberately failing test to prove CI turns red. Will be reverted.
#include <check.h>
#include <stdlib.h>

START_TEST(test_ci_should_fail) {
    ck_assert_int_eq(1, 2);
}
END_TEST

int main(void) {
    Suite *s = suite_create("ci_canary");
    TCase *tc = tcase_create("core");
    tcase_add_test(tc, test_ci_should_fail);
    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
