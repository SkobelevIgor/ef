#include "unity.h"
#include "runes.h"
#include <stdlib.h>
#include <locale.h>

void setUp(void) {}
void tearDown(void) {}

/* --- visual_column tests ------------------------------------------------- */

void test_visual_column_empty_line(void) {
    TEST_ASSERT_EQUAL_INT(0, visual_column(L"", 0, 0, 4));
}

void test_visual_column_no_tabs(void) {
    const wchar_t *line = L"hello";
    TEST_ASSERT_EQUAL_INT(0, visual_column(line, 5, 0, 4));
    TEST_ASSERT_EQUAL_INT(3, visual_column(line, 5, 3, 4));
    TEST_ASSERT_EQUAL_INT(5, visual_column(line, 5, 5, 4));
}

void test_visual_column_with_tab_at_start(void) {
    const wchar_t *line = L"\thello";
    /* Tab at col 0 with tab_stop=4 -> expands to 4 */
    TEST_ASSERT_EQUAL_INT(4, visual_column(line, 6, 1, 4));
    TEST_ASSERT_EQUAL_INT(5, visual_column(line, 6, 2, 4));
}

void test_visual_column_tab_in_middle(void) {
    const wchar_t *line = L"ab\tcd";
    /* 'a'=1, 'b'=2, tab at visual col 2 -> next tab stop at 4 */
    TEST_ASSERT_EQUAL_INT(4, visual_column(line, 5, 3, 4));
    TEST_ASSERT_EQUAL_INT(5, visual_column(line, 5, 4, 4));
}

void test_visual_column_default_tab_stop(void) {
    const wchar_t *line = L"\tx";
    TEST_ASSERT_EQUAL_INT(4, visual_column(line, 2, 1, 0));
}

void test_visual_column_wide_chars(void) {
    const wchar_t *line = L"\x4E2D\x6587" L"abc"; /* 中文abc */
    TEST_ASSERT_EQUAL_INT(4, visual_column(line, 5, 2, 4));
    TEST_ASSERT_EQUAL_INT(7, visual_column(line, 5, 5, 4));
}

void test_visual_column_combining_mark(void) {
    const wchar_t *line = L"e\x0301" L"x"; /* e + combining acute, x */
    TEST_ASSERT_EQUAL_INT(1, visual_column(line, 3, 2, 4));
    TEST_ASSERT_EQUAL_INT(2, visual_column(line, 3, 3, 4));
}

/* --- visual_line_width tests --------------------------------------------- */

void test_visual_line_width_empty(void) {
    TEST_ASSERT_EQUAL_INT(0, visual_line_width(L"", 0, 4));
}

void test_visual_line_width_no_tabs(void) {
    TEST_ASSERT_EQUAL_INT(5, visual_line_width(L"hello", 5, 4));
}

void test_visual_line_width_with_tab(void) {
    TEST_ASSERT_EQUAL_INT(5, visual_line_width(L"\tx", 2, 4));
}

void test_visual_line_width_wide_chars(void) {
    TEST_ASSERT_EQUAL_INT(5, visual_line_width(L"\x4E2D\x6587" L"a", 3, 4));
}

/* --- normalize_range tests ----------------------------------------------- */

void test_normalize_range_already_ordered(void) {
    int sr = 0, sc = 0, er = 2, ec = 5;
    normalize_range(&sr, &sc, &er, &ec);
    TEST_ASSERT_EQUAL_INT(0, sr);
    TEST_ASSERT_EQUAL_INT(0, sc);
    TEST_ASSERT_EQUAL_INT(2, er);
    TEST_ASSERT_EQUAL_INT(5, ec);
}

void test_normalize_range_reversed(void) {
    int sr = 3, sc = 5, er = 1, ec = 2;
    normalize_range(&sr, &sc, &er, &ec);
    TEST_ASSERT_EQUAL_INT(1, sr);
    TEST_ASSERT_EQUAL_INT(2, sc);
    TEST_ASSERT_EQUAL_INT(3, er);
    TEST_ASSERT_EQUAL_INT(5, ec);
}

void test_normalize_range_same_row_reversed(void) {
    int sr = 1, sc = 10, er = 1, ec = 3;
    normalize_range(&sr, &sc, &er, &ec);
    TEST_ASSERT_EQUAL_INT(1, sr);
    TEST_ASSERT_EQUAL_INT(3, sc);
    TEST_ASSERT_EQUAL_INT(1, er);
    TEST_ASSERT_EQUAL_INT(10, ec);
}

/* --- is_whitespace / is_word_char tests ---------------------------------- */

void test_is_whitespace(void) {
    TEST_ASSERT_TRUE(is_whitespace(L' '));
    TEST_ASSERT_TRUE(is_whitespace(L'\t'));
    TEST_ASSERT_FALSE(is_whitespace(L'a'));
    TEST_ASSERT_FALSE(is_whitespace(L'1'));
}

void test_is_word_char(void) {
    TEST_ASSERT_TRUE(is_word_char(L'a'));
    TEST_ASSERT_TRUE(is_word_char(L'Z'));
    TEST_ASSERT_TRUE(is_word_char(L'0'));
    TEST_ASSERT_TRUE(is_word_char(L'_'));
    TEST_ASSERT_FALSE(is_word_char(L' '));
    TEST_ASSERT_FALSE(is_word_char(L'.'));
    TEST_ASSERT_FALSE(is_word_char(L'-'));
}

/* --- insert_runes tests -------------------------------------------------- */

void test_insert_runes_at_start(void) {
    int out_len;
    wchar_t *result = insert_runes(L"world", 5, 0, L"hello ", 6, &out_len);
    TEST_ASSERT_EQUAL_INT(11, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"hello world", 11));
    free(result);
}

void test_insert_runes_at_end(void) {
    int out_len;
    wchar_t *result = insert_runes(L"hello", 5, 5, L" world", 6, &out_len);
    TEST_ASSERT_EQUAL_INT(11, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"hello world", 11));
    free(result);
}

void test_insert_runes_in_middle(void) {
    int out_len;
    wchar_t *result = insert_runes(L"helo", 4, 2, L"ll", 2, &out_len);
    TEST_ASSERT_EQUAL_INT(6, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"helllo", 6));
    free(result);
}

void test_insert_runes_into_empty(void) {
    int out_len;
    wchar_t *result = insert_runes(L"", 0, 0, L"abc", 3, &out_len);
    TEST_ASSERT_EQUAL_INT(3, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"abc", 3));
    free(result);
}

/* --- remove_runes tests -------------------------------------------------- */

void test_remove_runes_from_start(void) {
    int out_len;
    wchar_t *result = remove_runes(L"hello", 5, 0, 2, &out_len);
    TEST_ASSERT_EQUAL_INT(3, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"llo", 3));
    free(result);
}

void test_remove_runes_from_end(void) {
    int out_len;
    wchar_t *result = remove_runes(L"hello", 5, 3, 5, &out_len);
    TEST_ASSERT_EQUAL_INT(3, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"hel", 3));
    free(result);
}

void test_remove_runes_from_middle(void) {
    int out_len;
    wchar_t *result = remove_runes(L"hello", 5, 1, 4, &out_len);
    TEST_ASSERT_EQUAL_INT(2, out_len);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(result, L"ho", 2));
    free(result);
}

void test_remove_all_runes(void) {
    int out_len;
    wchar_t *result = remove_runes(L"abc", 3, 0, 3, &out_len);
    TEST_ASSERT_EQUAL_INT(0, out_len);
    free(result);
}

/* --- Unicode tests ------------------------------------------------------- */

void test_is_word_char_unicode(void) {
    /* Unicode letters should be word chars */
    TEST_ASSERT_TRUE(is_word_char(L'\u00e9')); /* e-acute */
    /* Emoji should not be word chars */
    TEST_ASSERT_FALSE(is_word_char(L'\u2603')); /* snowman */
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_visual_column_empty_line);
    RUN_TEST(test_visual_column_no_tabs);
    RUN_TEST(test_visual_column_with_tab_at_start);
    RUN_TEST(test_visual_column_tab_in_middle);
    RUN_TEST(test_visual_column_default_tab_stop);
    RUN_TEST(test_visual_column_wide_chars);
    RUN_TEST(test_visual_column_combining_mark);
    RUN_TEST(test_visual_line_width_empty);
    RUN_TEST(test_visual_line_width_no_tabs);
    RUN_TEST(test_visual_line_width_with_tab);
    RUN_TEST(test_visual_line_width_wide_chars);
    RUN_TEST(test_normalize_range_already_ordered);
    RUN_TEST(test_normalize_range_reversed);
    RUN_TEST(test_normalize_range_same_row_reversed);
    RUN_TEST(test_is_whitespace);
    RUN_TEST(test_is_word_char);
    RUN_TEST(test_insert_runes_at_start);
    RUN_TEST(test_insert_runes_at_end);
    RUN_TEST(test_insert_runes_in_middle);
    RUN_TEST(test_insert_runes_into_empty);
    RUN_TEST(test_remove_runes_from_start);
    RUN_TEST(test_remove_runes_from_end);
    RUN_TEST(test_remove_runes_from_middle);
    RUN_TEST(test_remove_all_runes);
    RUN_TEST(test_is_word_char_unicode);
    return UNITY_END();
}
