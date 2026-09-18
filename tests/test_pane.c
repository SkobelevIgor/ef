#include "unity.h"
#include "pane.h"
#include "buffer.h"
#include <stdlib.h>
#include <locale.h>

static Buffer *buf;
static Pane *pane;

static void setup_buffer(const wchar_t *lines[], int count) {
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
    pane = pane_new(buf);
}

void setUp(void) {
    buf = NULL;
    pane = NULL;
}

void tearDown(void) {
    pane_free(pane);
    buffer_free(buf);
}

void test_new_pane_at_origin(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_buffer(lines, 1);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_row);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_col);
}

void test_goto_line(void) {
    const wchar_t *lines[] = {L"one", L"two", L"three"};
    setup_buffer(lines, 3);
    pane_goto_line(pane, 2);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_row);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_col);
}

void test_goto_line_clamped(void) {
    const wchar_t *lines[] = {L"one", L"two"};
    setup_buffer(lines, 2);
    pane_goto_line(pane, 100);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_row);
}

void test_move_down(void) {
    const wchar_t *lines[] = {L"one", L"two", L"three"};
    setup_buffer(lines, 3);
    pane_move_down(pane);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_row);
}

void test_move_down_at_last_line_noop(void) {
    const wchar_t *lines[] = {L"only"};
    setup_buffer(lines, 1);
    pane_move_down(pane);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_row);
}

void test_move_up(void) {
    const wchar_t *lines[] = {L"one", L"two"};
    setup_buffer(lines, 2);
    pane->cursor_row = 1;
    pane_move_up(pane);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_row);
}

void test_move_up_at_first_line_noop(void) {
    const wchar_t *lines[] = {L"one"};
    setup_buffer(lines, 1);
    pane_move_up(pane);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_row);
}

void test_move_right(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_buffer(lines, 1);
    pane_move_right(pane);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_col);
}

void test_move_right_wraps_to_next_line(void) {
    const wchar_t *lines[] = {L"ab", L"cd"};
    setup_buffer(lines, 2);
    pane->cursor_col = 2;
    pane_move_right(pane);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_row);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_col);
}

void test_move_left(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_buffer(lines, 1);
    pane->cursor_col = 2;
    pane_move_left(pane);
    TEST_ASSERT_EQUAL_INT(1, pane->cursor_col);
}

void test_move_left_wraps_to_prev_line(void) {
    const wchar_t *lines[] = {L"ab", L"cd"};
    setup_buffer(lines, 2);
    pane->cursor_row = 1;
    pane->cursor_col = 0;
    pane_move_left(pane);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_row);
    TEST_ASSERT_EQUAL_INT(2, pane->cursor_col);
}

void test_move_to_line_start(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_buffer(lines, 1);
    pane->cursor_col = 3;
    pane_move_to_line_start(pane);
    TEST_ASSERT_EQUAL_INT(0, pane->cursor_col);
}

void test_move_to_line_end(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_buffer(lines, 1);
    pane_move_to_line_end(pane);
    TEST_ASSERT_EQUAL_INT(5, pane->cursor_col);
}

void test_page_down(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3", L"4", L"5",
                              L"6", L"7", L"8", L"9", L"10"};
    setup_buffer(lines, 10);
    pane_page_down(pane, 10); /* moves by height/2 = 5 */
    TEST_ASSERT_EQUAL_INT(5, pane->cursor_row);
}

void test_page_up(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3", L"4", L"5",
                              L"6", L"7", L"8", L"9", L"10"};
    setup_buffer(lines, 10);
    pane->cursor_row = 8;
    pane_page_up(pane, 10);
    TEST_ASSERT_EQUAL_INT(3, pane->cursor_row);
}

void test_selection(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer(lines, 1);
    pane->cursor_col = 2;
    pane_start_selection(pane);
    pane->cursor_col = 7;

    int sr, sc, er, ec;
    pane_get_selection(pane, &sr, &sc, &er, &ec);
    TEST_ASSERT_EQUAL_INT(0, sr);
    TEST_ASSERT_EQUAL_INT(2, sc);
    TEST_ASSERT_EQUAL_INT(0, er);
    TEST_ASSERT_EQUAL_INT(7, ec);

    TEST_ASSERT_TRUE(pane_is_in_selection(pane, 0, 3));
    TEST_ASSERT_FALSE(pane_is_in_selection(pane, 0, 1));
}

void test_find_char_forward(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer(lines, 1);
    pane_find_char_forward(pane, L'o');
    TEST_ASSERT_EQUAL_INT(4, pane->cursor_col);
}

void test_find_char_backward(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer(lines, 1);
    pane->cursor_col = 8;
    pane_find_char_backward(pane, L'o');
    TEST_ASSERT_EQUAL_INT(7, pane->cursor_col);
}

void test_move_to_next_word(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer(lines, 1);
    pane_move_to_next_word(pane);
    TEST_ASSERT_EQUAL_INT(6, pane->cursor_col);
}

void test_move_to_prev_word(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_buffer(lines, 1);
    pane->cursor_col = 8;
    pane_move_to_prev_word(pane);
    TEST_ASSERT_EQUAL_INT(6, pane->cursor_col);
}

void test_clamp_cursor_col(void) {
    const wchar_t *lines[] = {L"hi"};
    setup_buffer(lines, 1);
    pane->cursor_col = 100;
    pane_clamp_cursor_col(pane);
    TEST_ASSERT_EQUAL_INT(2, pane->cursor_col);
}

void test_adjust_cursor_for_insert(void) {
    /* Simulate: buffer already had a line inserted at row 1, so it has 4 lines now */
    const wchar_t *lines[] = {L"a", L"new", L"b", L"c"};
    setup_buffer(lines, 4);
    pane->cursor_row = 2; /* was at row 2 before insert */
    pane_adjust_cursor_for_edit(pane, 1, 1);
    TEST_ASSERT_EQUAL_INT(3, pane->cursor_row);
}

void test_adjust_cursor_for_delete(void) {
    const wchar_t *lines[] = {L"a", L"b", L"c", L"d"};
    setup_buffer(lines, 4);
    pane->cursor_row = 3;
    pane_adjust_cursor_for_edit(pane, 1, -1);
    TEST_ASSERT_EQUAL_INT(2, pane->cursor_row);
}

void test_clamp_cursor_clamps_selection_anchor(void) {
    const wchar_t *lines[] = {L"abc", L"de", L"f"};
    setup_buffer(lines, 3);
    pane->selection_active = true;
    pane->selection_start_row = 9;
    pane->selection_start_col = 7;

    pane_clamp_cursor(pane);

    TEST_ASSERT_EQUAL_INT(2, pane->selection_start_row);
    TEST_ASSERT_EQUAL_INT(1, pane->selection_start_col);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_new_pane_at_origin);
    RUN_TEST(test_goto_line);
    RUN_TEST(test_goto_line_clamped);
    RUN_TEST(test_move_down);
    RUN_TEST(test_move_down_at_last_line_noop);
    RUN_TEST(test_move_up);
    RUN_TEST(test_move_up_at_first_line_noop);
    RUN_TEST(test_move_right);
    RUN_TEST(test_move_right_wraps_to_next_line);
    RUN_TEST(test_move_left);
    RUN_TEST(test_move_left_wraps_to_prev_line);
    RUN_TEST(test_move_to_line_start);
    RUN_TEST(test_move_to_line_end);
    RUN_TEST(test_page_down);
    RUN_TEST(test_page_up);
    RUN_TEST(test_selection);
    RUN_TEST(test_find_char_forward);
    RUN_TEST(test_find_char_backward);
    RUN_TEST(test_move_to_next_word);
    RUN_TEST(test_move_to_prev_word);
    RUN_TEST(test_clamp_cursor_col);
    RUN_TEST(test_adjust_cursor_for_insert);
    RUN_TEST(test_adjust_cursor_for_delete);
    RUN_TEST(test_clamp_cursor_clamps_selection_anchor);
    return UNITY_END();
}
