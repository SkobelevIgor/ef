#include "unity.h"
#include "theme.h"
#include <stdbool.h>

void setUp(void) {}
void tearDown(void) {}

/* Active pane current line: should return mode-specific pair */
void test_gutter_current_line_pair_active_normal(void) {
    TEST_ASSERT_EQUAL(PAIR_NORMAL_LINENUM,
                      theme_gutter_current_line_pair(true, MODE_NORMAL));
}

void test_gutter_current_line_pair_active_insert(void) {
    TEST_ASSERT_EQUAL(PAIR_INSERT_LINENUM,
                      theme_gutter_current_line_pair(true, MODE_INSERT));
}

void test_gutter_current_line_pair_active_visual(void) {
    TEST_ASSERT_EQUAL(PAIR_VISUAL_LINENUM,
                      theme_gutter_current_line_pair(true, MODE_VISUAL));
}

/* Inactive pane current line: should return plain PAIR_LINE_NUM */
void test_gutter_current_line_pair_inactive_normal(void) {
    TEST_ASSERT_EQUAL(PAIR_LINE_NUM,
                      theme_gutter_current_line_pair(false, MODE_NORMAL));
}

void test_gutter_current_line_pair_inactive_insert(void) {
    TEST_ASSERT_EQUAL(PAIR_LINE_NUM,
                      theme_gutter_current_line_pair(false, MODE_INSERT));
}

void test_gutter_current_line_pair_inactive_visual(void) {
    TEST_ASSERT_EQUAL(PAIR_LINE_NUM,
                      theme_gutter_current_line_pair(false, MODE_VISUAL));
}

/* PAIR_READONLY_LINENUM must be distinct from all other mode pairs */
void test_pair_readonly_linenum_is_distinct(void) {
    TEST_ASSERT_NOT_EQUAL(PAIR_READONLY_LINENUM, PAIR_NORMAL_LINENUM);
    TEST_ASSERT_NOT_EQUAL(PAIR_READONLY_LINENUM, PAIR_INSERT_LINENUM);
    TEST_ASSERT_NOT_EQUAL(PAIR_READONLY_LINENUM, PAIR_VISUAL_LINENUM);
    TEST_ASSERT_NOT_EQUAL(PAIR_READONLY_LINENUM, PAIR_LINE_NUM);
}

/* Without initscr(), init_pair fails; the fallback must be pair 0 */
void test_syntax_pair_falls_back_when_init_pair_fails(void) {
    TEST_ASSERT_EQUAL(0, theme_syntax_pair(1));
    TEST_ASSERT_EQUAL(0, theme_syntax_pair(1));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gutter_current_line_pair_active_normal);
    RUN_TEST(test_gutter_current_line_pair_active_insert);
    RUN_TEST(test_gutter_current_line_pair_active_visual);
    RUN_TEST(test_gutter_current_line_pair_inactive_normal);
    RUN_TEST(test_gutter_current_line_pair_inactive_insert);
    RUN_TEST(test_gutter_current_line_pair_inactive_visual);
    RUN_TEST(test_pair_readonly_linenum_is_distinct);
    RUN_TEST(test_syntax_pair_falls_back_when_init_pair_fails);
    return UNITY_END();
}
