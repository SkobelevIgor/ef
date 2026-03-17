#include "unity.h"
#include "clipboard.h"
#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

static Clipboard *cb;

void setUp(void) {
    cb = clipboard_new();
}

void tearDown(void) {
    clipboard_free(cb);
    cb = NULL;
}

/* --- Lifecycle tests ----------------------------------------------------- */

void test_clipboard_new_initializes_empty(void) {
    TEST_ASSERT_NOT_NULL(cb);
    TEST_ASSERT_NULL(cb->lines);
    TEST_ASSERT_NULL(cb->line_lens);
    TEST_ASSERT_EQUAL_INT(0, cb->line_count);
    TEST_ASSERT_FALSE(cb->is_line_mode);
}

void test_clipboard_free_null_safe(void) {
    clipboard_free(NULL);
    /* No crash = pass */
}

/* --- clipboard_set tests ------------------------------------------------- */

void test_clipboard_set_single_line_char_mode(void) {
    wchar_t *line = L"hello";
    int len = 5;
    clipboard_set(cb, &line, &len, 1, false);

    TEST_ASSERT_EQUAL_INT(1, cb->line_count);
    TEST_ASSERT_FALSE(cb->is_line_mode);
    TEST_ASSERT_EQUAL_INT(5, cb->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(cb->lines[0], L"hello", 5));
}

void test_clipboard_set_multiple_lines_line_mode(void) {
    wchar_t *lines[] = {L"line1", L"line2", L"line3"};
    int lens[] = {5, 5, 5};
    clipboard_set(cb, lines, lens, 3, true);

    TEST_ASSERT_EQUAL_INT(3, cb->line_count);
    TEST_ASSERT_TRUE(cb->is_line_mode);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(cb->lines[0], L"line1", 5));
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(cb->lines[1], L"line2", 5));
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(cb->lines[2], L"line3", 5));
}

void test_clipboard_set_creates_deep_copy(void) {
    wchar_t *src = malloc(sizeof(wchar_t) * 4);
    wmemcpy(src, L"abc", 3);
    src[3] = L'\0';
    int len = 3;
    clipboard_set(cb, &src, &len, 1, false);

    /* Modifying source should not affect clipboard */
    src[0] = L'X';
    TEST_ASSERT_EQUAL_INT(L'a', cb->lines[0][0]);
    free(src);
}

/* --- clipboard_clear tests ----------------------------------------------- */

void test_clipboard_clear_resets_all_fields(void) {
    wchar_t *line = L"test";
    int len = 4;
    clipboard_set(cb, &line, &len, 1, true);
    TEST_ASSERT_EQUAL_INT(1, cb->line_count);

    clipboard_clear(cb);
    TEST_ASSERT_NULL(cb->lines);
    TEST_ASSERT_NULL(cb->line_lens);
    TEST_ASSERT_EQUAL_INT(0, cb->line_count);
    TEST_ASSERT_FALSE(cb->is_line_mode);
}

/* --- Overwrite tests ----------------------------------------------------- */

void test_clipboard_set_twice_overwrites(void) {
    wchar_t *line1 = L"first";
    int len1 = 5;
    clipboard_set(cb, &line1, &len1, 1, false);

    wchar_t *line2 = L"second";
    int len2 = 6;
    clipboard_set(cb, &line2, &len2, 1, true);

    TEST_ASSERT_EQUAL_INT(1, cb->line_count);
    TEST_ASSERT_TRUE(cb->is_line_mode);
    TEST_ASSERT_EQUAL_INT(6, cb->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(cb->lines[0], L"second", 6));
}

/* --- Edge cases ---------------------------------------------------------- */

void test_clipboard_empty_after_clear(void) {
    clipboard_clear(cb);
    TEST_ASSERT_EQUAL_INT(0, cb->line_count);
    TEST_ASSERT_NULL(cb->lines);
}

void test_clipboard_set_zero_length_lines(void) {
    wchar_t *lines[] = {L"", L""};
    int lens[] = {0, 0};
    clipboard_set(cb, lines, lens, 2, false);

    TEST_ASSERT_EQUAL_INT(2, cb->line_count);
    TEST_ASSERT_EQUAL_INT(0, cb->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, cb->line_lens[1]);
}

void test_clipboard_set_unicode(void) {
    wchar_t *line = L"\u00e9\u00e8\u00ea";  /* e-acute, e-grave, e-circumflex */
    int len = 3;
    clipboard_set(cb, &line, &len, 1, false);

    TEST_ASSERT_EQUAL_INT(1, cb->line_count);
    TEST_ASSERT_EQUAL_INT(3, cb->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\u00e9', cb->lines[0][0]);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();

    /* Lifecycle */
    RUN_TEST(test_clipboard_new_initializes_empty);
    RUN_TEST(test_clipboard_free_null_safe);

    /* Set */
    RUN_TEST(test_clipboard_set_single_line_char_mode);
    RUN_TEST(test_clipboard_set_multiple_lines_line_mode);
    RUN_TEST(test_clipboard_set_creates_deep_copy);

    /* Clear */
    RUN_TEST(test_clipboard_clear_resets_all_fields);

    /* Overwrite */
    RUN_TEST(test_clipboard_set_twice_overwrites);

    /* Edge cases */
    RUN_TEST(test_clipboard_empty_after_clear);
    RUN_TEST(test_clipboard_set_zero_length_lines);
    RUN_TEST(test_clipboard_set_unicode);

    return UNITY_END();
}
