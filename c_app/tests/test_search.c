#include "unity.h"
#include "search.h"
#include "buffer.h"
#include <stdlib.h>
#include <locale.h>

static Buffer *buf;

static void setup_buffer_lines(const wchar_t *lines[], int count) {
    buf = buffer_new();
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            int len = (int)wcslen(lines[i]);
            wchar_t *l = malloc(sizeof(wchar_t) * (len + 1));
            wmemcpy(l, lines[i], len); l[len] = L'\0';
            buffer_insert_line_after(buf, buf->line_count - 1, l, len);
        } else {
            int len = (int)wcslen(lines[0]);
            wchar_t *l = malloc(sizeof(wchar_t) * (len + 1));
            wmemcpy(l, lines[0], len); l[len] = L'\0';
            buffer_set_line(buf, 0, l, len);
        }
    }
}

void setUp(void) { buf = NULL; }
void tearDown(void) { buffer_free(buf); }

/* --- FindFirstMatchAfterCursor ------------------------------------------- */

void test_find_first_match_after_cursor_empty(void) {
    TEST_ASSERT_EQUAL_INT(-1, find_first_match_after_cursor(NULL, 0, 0, 0));
}

void test_find_first_match_after_cursor_single(void) {
    SearchMatch m = {0, 5, 3};
    TEST_ASSERT_EQUAL_INT(0, find_first_match_after_cursor(&m, 1, 0, 0));
}

void test_find_first_match_after_cursor_wraps(void) {
    SearchMatch ms[] = {{0, 0, 3}, {0, 5, 3}};
    /* Cursor after all matches → wrap to 0 */
    TEST_ASSERT_EQUAL_INT(0, find_first_match_after_cursor(ms, 2, 1, 0));
}

void test_find_first_match_after_cursor_exact(void) {
    SearchMatch ms[] = {{0, 0, 3}, {0, 5, 3}, {1, 2, 3}};
    /* Cursor at row 0, col 5 → should find match at index 1 */
    TEST_ASSERT_EQUAL_INT(1, find_first_match_after_cursor(ms, 3, 0, 5));
}

void test_find_first_match_after_cursor_later_row(void) {
    SearchMatch ms[] = {{0, 0, 3}, {2, 5, 3}};
    TEST_ASSERT_EQUAL_INT(1, find_first_match_after_cursor(ms, 2, 1, 0));
}

/* --- buffer_find_all_matches --------------------------------------------- */

void test_find_all_matches_empty_query(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"", 0, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(m);
}

void test_find_all_matches_no_match(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"xyz", 3, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(m);
}

void test_find_all_matches_single(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"world", 5, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(0, m[0].row);
    TEST_ASSERT_EQUAL_INT(6, m[0].col);
    TEST_ASSERT_EQUAL_INT(5, m[0].length);
    free(m);
}

void test_find_all_matches_multiple(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar"};
    setup_buffer_lines(lines, 2);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"hello", 5, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_INT(0, m[0].row);
    TEST_ASSERT_EQUAL_INT(0, m[0].col);
    TEST_ASSERT_EQUAL_INT(1, m[1].row);
    TEST_ASSERT_EQUAL_INT(4, m[1].col);
    free(m);
}

void test_find_all_matches_case_insensitive(void) {
    const wchar_t *lines[] = {L"Hello HELLO hello"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"hello", 5, &count);
    TEST_ASSERT_EQUAL_INT(3, count);
    free(m);
}

void test_find_all_matches_overlapping(void) {
    const wchar_t *lines[] = {L"aaa"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"aa", 2, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_INT(0, m[0].col);
    TEST_ASSERT_EQUAL_INT(1, m[1].col);
    free(m);
}

void test_find_all_matches_null_buffer(void) {
    int count;
    SearchMatch *m = buffer_find_all_matches(NULL, L"test", 4, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(m);
}

void test_find_all_matches_unicode(void) {
    const wchar_t *lines[] = {L"日本語テスト hello 日本語"};
    setup_buffer_lines(lines, 1);
    int count;
    SearchMatch *m = buffer_find_all_matches(buf, L"日本語", 3, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_INT(0, m[0].col);
    TEST_ASSERT_EQUAL_INT(13, m[1].col);
    free(m);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_find_first_match_after_cursor_empty);
    RUN_TEST(test_find_first_match_after_cursor_single);
    RUN_TEST(test_find_first_match_after_cursor_wraps);
    RUN_TEST(test_find_first_match_after_cursor_exact);
    RUN_TEST(test_find_first_match_after_cursor_later_row);
    RUN_TEST(test_find_all_matches_empty_query);
    RUN_TEST(test_find_all_matches_no_match);
    RUN_TEST(test_find_all_matches_single);
    RUN_TEST(test_find_all_matches_multiple);
    RUN_TEST(test_find_all_matches_case_insensitive);
    RUN_TEST(test_find_all_matches_overlapping);
    RUN_TEST(test_find_all_matches_null_buffer);
    RUN_TEST(test_find_all_matches_unicode);
    return UNITY_END();
}
