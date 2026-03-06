#include "unity.h"
#include "input.h"
#include <stdlib.h>
#include <locale.h>

static InputState *is;

void setUp(void) { is = input_state_new(); }
void tearDown(void) { input_state_free(is); }

void test_new_input_state(void) {
    TEST_ASSERT_EQUAL_INT(0, is->count);
    TEST_ASSERT_FALSE(is->has_count);
    TEST_ASSERT_FALSE(input_state_has_pending(is));
}

void test_get_count_default(void) {
    TEST_ASSERT_EQUAL_INT(1, input_state_get_count(is));
}

void test_add_digits(void) {
    input_state_add_digit(is, 3);
    TEST_ASSERT_EQUAL_INT(3, input_state_get_count(is));
    input_state_add_digit(is, 5);
    TEST_ASSERT_EQUAL_INT(35, input_state_get_count(is));
}

void test_reset_clears_count(void) {
    input_state_add_digit(is, 5);
    input_state_reset(is);
    TEST_ASSERT_FALSE(is->has_count);
    TEST_ASSERT_EQUAL_INT(1, input_state_get_count(is));
}

void test_reset_preserves_last_find(void) {
    input_state_save_last_find(is, L'x', true);
    input_state_reset(is);
    TEST_ASSERT_TRUE(is->has_last_find);
    TEST_ASSERT_EQUAL_INT(L'x', is->last_find_char);
    TEST_ASSERT_TRUE(is->last_find_forward);
}

void test_has_pending_find_forward(void) {
    is->pending_find_forward = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_operator(void) {
    is->pending_operator = L'd';
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_goto_line(void) {
    is->pending_goto_line = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_mark(void) {
    is->pending_mark = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

void test_has_pending_jump(void) {
    is->pending_jump_to_mark = true;
    TEST_ASSERT_TRUE(input_state_has_pending(is));
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_new_input_state);
    RUN_TEST(test_get_count_default);
    RUN_TEST(test_add_digits);
    RUN_TEST(test_reset_clears_count);
    RUN_TEST(test_reset_preserves_last_find);
    RUN_TEST(test_has_pending_find_forward);
    RUN_TEST(test_has_pending_operator);
    RUN_TEST(test_has_pending_goto_line);
    RUN_TEST(test_has_pending_mark);
    RUN_TEST(test_has_pending_jump);
    return UNITY_END();
}
