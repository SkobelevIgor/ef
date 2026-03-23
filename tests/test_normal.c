#include "unity.h"
#include "test_helpers.h"
#include "normal.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static Editor *ed;
static Buffer *buf;

static void setup_editor(const wchar_t *lines[], int count) {
    test_setup_editor(lines, count, &ed, &buf);
}

static void send_char(wchar_t ch) {
    test_send_char(ed, ch);
}

void setUp(void) { ed = NULL; buf = NULL; }

void tearDown(void) {
    if (ed) {
        editor_free(ed);
        ed = NULL; buf = NULL;
    }
}

void test_hjkl_navigation(void) {
    const wchar_t *lines[] = {L"hello", L"world"};
    setup_editor(lines, 2);

    send_char(L'l'); /* right */
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);

    send_char(L'j'); /* down */
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_row);

    send_char(L'h'); /* left */
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_col);

    send_char(L'k'); /* up */
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_row);
}

void test_numeric_prefix(void) {
    const wchar_t *lines[] = {L"a", L"b", L"c", L"d", L"e"};
    setup_editor(lines, 5);

    send_char(L'3');
    send_char(L'j');
    TEST_ASSERT_EQUAL_INT(3, editor_active_pane(ed)->cursor_row);
}

void test_goto_line_start_end(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'$');
    TEST_ASSERT_EQUAL_INT(5, editor_active_pane(ed)->cursor_col);

    send_char(L'0');
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_col);
}

void test_enter_insert_mode_i(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'i');
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
}

void test_enter_insert_mode_a(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 2;

    send_char(L'a');
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(3, editor_active_pane(ed)->cursor_col);
}

void test_enter_visual_mode(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'v');
    TEST_ASSERT_EQUAL_INT(MODE_VISUAL, ed->mode);
    TEST_ASSERT_TRUE(editor_active_pane(ed)->selection_active);
}

void test_dd_deletes_line(void) {
    const wchar_t *lines[] = {L"one", L"two", L"three"};
    setup_editor(lines, 3);
    editor_active_pane(ed)->cursor_row = 1;

    send_char(L'd');
    send_char(L'd');
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    /* dd must cut the deleted line into clipboard (line-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_TRUE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(3, ed->clipboard->line_lens[0]); /* "two" */
}

void test_dd_overwrites_clipboard(void) {
    const wchar_t *lines[] = {L"aaa", L"bbb", L"ccc"};
    setup_editor(lines, 3);

    /* Yank first line */
    send_char(L'y');
    send_char(L'y');
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_EQUAL_INT(3, ed->clipboard->line_lens[0]);

    /* Delete second line — clipboard must be overwritten with "bbb" */
    editor_active_pane(ed)->cursor_row = 1;
    send_char(L'd');
    send_char(L'd');
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_TRUE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"bbb", 3));
}

void test_x_cuts_to_clipboard(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'x');
    TEST_ASSERT_EQUAL_INT(4, buf->line_lens[0]);
    /* x must cut deleted char into clipboard (char-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"h", 1));
}

void test_dw_cuts_to_clipboard(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    send_char(L'd');
    send_char(L'w');
    /* dw must cut deleted word into clipboard (char-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    /* "hello " deleted (cursor was at 0, next word at 6) */
    TEST_ASSERT_EQUAL_INT(6, ed->clipboard->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"hello ", 6));
}

void test_db_cuts_to_clipboard(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 8;

    send_char(L'd');
    send_char(L'b');
    /* db must cut deleted text into clipboard (char-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    /* "wo" deleted (from col 6 to col 8) */
    TEST_ASSERT_EQUAL_INT(2, ed->clipboard->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"wo", 2));
}

void test_d_dollar_cuts_to_clipboard(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 2;

    send_char(L'd');
    send_char(L'$');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]); /* "he" remains */
    /* d$ must cut "llo" into clipboard (char-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(3, ed->clipboard->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"llo", 3));
}

void test_d_zero_cuts_to_clipboard(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 3;

    send_char(L'd');
    send_char(L'0');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]); /* "lo" remains */
    /* d0 must cut "hel" into clipboard (char-mode) */
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(3, ed->clipboard->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(ed->clipboard->lines[0], L"hel", 3));
}

void test_yy_yanks_line(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'y');
    send_char(L'y');
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_TRUE(ed->clipboard->is_line_mode);
    TEST_ASSERT_EQUAL_INT(5, ed->clipboard->line_lens[0]);
}

void test_x_deletes_char(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_editor(lines, 1);

    send_char(L'x');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'b', buf->lines[0][0]);
}

void test_undo(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_editor(lines, 1);

    send_char(L'x'); /* delete 'a' */
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);

    send_char(L'u'); /* undo */
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[0]);
}

void test_goto_line_colon(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3", L"4", L"5"};
    setup_editor(lines, 5);

    send_char(L':');
    send_char(L'3');
    /* Enter to confirm */
    EditorEvent ev = {EV_KEY, (int)L'\n', L'\n', true};
    editor_handle_key(ed, &ev);
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_row);
}

void test_goto_line_colon_cr(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3", L"4", L"5"};
    setup_editor(lines, 5);

    send_char(L':');
    send_char(L'1');
    send_char(L'0');
    /* Real terminal sends \r (carriage return) with nonl() */
    EditorEvent ev = {EV_KEY, (int)L'\r', L'\r', true};
    editor_handle_key(ed, &ev);
    TEST_ASSERT_EQUAL_INT(4, editor_active_pane(ed)->cursor_row);
}

void test_G_goes_to_last_line(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3"};
    setup_editor(lines, 3);

    send_char(L'G');
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_row);
}

void test_g_goes_to_first_line(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3"};
    setup_editor(lines, 3);
    editor_active_pane(ed)->cursor_row = 2;

    send_char(L'g');
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_row);
}

void test_find_char_f(void) {
    const wchar_t *lines[] = {L"abcdef"};
    setup_editor(lines, 1);

    send_char(L'f');
    send_char(L'd');
    TEST_ASSERT_EQUAL_INT(3, editor_active_pane(ed)->cursor_col);
}

void test_find_char_n_repeat(void) {
    const wchar_t *lines[] = {L"abcdbcf"};
    setup_editor(lines, 1);

    send_char(L'f');
    send_char(L'c');
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_col);

    send_char(L'n');
    TEST_ASSERT_EQUAL_INT(5, editor_active_pane(ed)->cursor_col);
}

void test_find_char_N_reverse(void) {
    const wchar_t *lines[] = {L"abcdbcf"};
    setup_editor(lines, 1);

    send_char(L'f');
    send_char(L'c');
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_col);

    send_char(L'n');
    TEST_ASSERT_EQUAL_INT(5, editor_active_pane(ed)->cursor_col);

    send_char(L'N');
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_col);
}

void test_set_mark_and_jump(void) {
    const wchar_t *lines[] = {L"hello", L"world", L"test"};
    setup_editor(lines, 3);
    Pane *p = editor_active_pane(ed);

    /* Move to (1,3) and set mark 'a' */
    p->cursor_row = 1;
    p->cursor_col = 3;
    send_char(L'm');
    send_char(L'a');

    /* Move away */
    p->cursor_row = 0;
    p->cursor_col = 0;

    /* Jump to mark 'a' */
    send_char(L'`');
    send_char(L'a');
    TEST_ASSERT_EQUAL_INT(1, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(3, p->cursor_col);
}

void test_jump_to_unset_mark(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 0;
    p->cursor_col = 2;

    send_char(L'`');
    send_char(L'z');
    /* Cursor should not move */
    TEST_ASSERT_EQUAL_INT(0, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(2, p->cursor_col);
}

void test_mark_invalid_id_ignored(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 2;

    send_char(L'm');
    send_char(L'!'); /* invalid */
    /* Should not crash, pending_mark should be cleared */
    TEST_ASSERT_FALSE(ed->input_state->pending_mark);
}

void test_mark_clamps_after_lines_deleted(void) {
    const wchar_t *lines[] = {L"aaa", L"bbb", L"ccc"};
    setup_editor(lines, 3);
    Pane *p = editor_active_pane(ed);

    /* Set mark at row 2, col 1 */
    p->cursor_row = 2;
    p->cursor_col = 1;
    send_char(L'm');
    send_char(L'a');

    /* Delete row 2 (dd at row 2) */
    p->cursor_row = 2;
    send_char(L'd');
    send_char(L'd');
    /* Now only 2 rows left (0,1) */

    /* Jump to mark -- row should clamp to last row */
    p->cursor_row = 0;
    p->cursor_col = 0;
    send_char(L'`');
    send_char(L'a');
    TEST_ASSERT_EQUAL_INT(1, p->cursor_row);
}

void test_mark_digits_and_uppercase(void) {
    const wchar_t *lines[] = {L"abcdef"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);

    /* Set mark '0' at col 3 */
    p->cursor_col = 3;
    send_char(L'm');
    send_char(L'0');

    /* Set mark 'Z' at col 5 */
    p->cursor_col = 5;
    send_char(L'm');
    send_char(L'Z');

    /* Jump to '0' */
    p->cursor_col = 0;
    send_char(L'`');
    send_char(L'0');
    TEST_ASSERT_EQUAL_INT(3, p->cursor_col);

    /* Jump to 'Z' */
    send_char(L'`');
    send_char(L'Z');
    TEST_ASSERT_EQUAL_INT(5, p->cursor_col);
}

void test_mark_escape_cancels(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'm');
    TEST_ASSERT_TRUE(ed->input_state->pending_mark);

    /* Escape cancels */
    EditorEvent esc = {EV_KEY, 27, 27, true};
    editor_handle_key(ed, &esc);
    TEST_ASSERT_FALSE(ed->input_state->pending_mark);
}

void test_word_navigation_w(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    send_char(L'w');
    TEST_ASSERT_EQUAL_INT(6, editor_active_pane(ed)->cursor_col);
}

void test_word_navigation_b(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 8;

    send_char(L'b');
    TEST_ASSERT_EQUAL_INT(6, editor_active_pane(ed)->cursor_col);
}

void test_goto_line_rejects_letters(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3", L"4", L"5"};
    setup_editor(lines, 5);

    send_char(L':');
    TEST_ASSERT_TRUE(ed->input_state->pending_goto_line);

    /* Letter should be ignored */
    send_char(L'a');
    TEST_ASSERT_EQUAL_INT(0, ed->input_state->goto_line_buf_len);
    TEST_ASSERT_TRUE(ed->input_state->pending_goto_line);

    /* Digit should still work */
    send_char(L'4');
    TEST_ASSERT_EQUAL_INT(1, ed->input_state->goto_line_buf_len);

    /* Enter to confirm */
    EditorEvent ev = {EV_KEY, (int)L'\n', L'\n', true};
    editor_handle_key(ed, &ev);
    TEST_ASSERT_EQUAL_INT(3, editor_active_pane(ed)->cursor_row);
}

void test_goto_line_auto_cancel_on_empty(void) {
    const wchar_t *lines[] = {L"1", L"2", L"3"};
    setup_editor(lines, 3);

    send_char(L':');
    TEST_ASSERT_TRUE(ed->input_state->pending_goto_line);

    send_char(L'5');
    TEST_ASSERT_EQUAL_INT(1, ed->input_state->goto_line_buf_len);

    /* Backspace removes digit and auto-cancels */
    EditorEvent bs = {EV_KEY, 127, 127, true};
    editor_handle_key(ed, &bs);
    TEST_ASSERT_FALSE(ed->input_state->pending_goto_line);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_goto_line_large_number(void) {
    const wchar_t *lines[] = {L"a", L"b", L"c"};
    setup_editor(lines, 3);

    send_char(L':');
    /* Type 100000 */
    send_char(L'1');
    send_char(L'0');
    send_char(L'0');
    send_char(L'0');
    send_char(L'0');
    send_char(L'0');
    TEST_ASSERT_EQUAL_INT(6, ed->input_state->goto_line_buf_len);
    TEST_ASSERT_EQUAL_STRING("100000", ed->input_state->goto_line_buffer);

    /* Enter - goes to last line (clamped) */
    EditorEvent ev = {EV_KEY, (int)L'\n', L'\n', true};
    editor_handle_key(ed, &ev);
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_row);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_hjkl_navigation);
    RUN_TEST(test_numeric_prefix);
    RUN_TEST(test_goto_line_start_end);
    RUN_TEST(test_enter_insert_mode_i);
    RUN_TEST(test_enter_insert_mode_a);
    RUN_TEST(test_enter_visual_mode);
    RUN_TEST(test_dd_deletes_line);
    RUN_TEST(test_dd_overwrites_clipboard);
    RUN_TEST(test_yy_yanks_line);
    RUN_TEST(test_x_deletes_char);
    RUN_TEST(test_x_cuts_to_clipboard);
    RUN_TEST(test_dw_cuts_to_clipboard);
    RUN_TEST(test_db_cuts_to_clipboard);
    RUN_TEST(test_d_dollar_cuts_to_clipboard);
    RUN_TEST(test_d_zero_cuts_to_clipboard);
    RUN_TEST(test_undo);
    RUN_TEST(test_goto_line_colon);
    RUN_TEST(test_goto_line_colon_cr);
    RUN_TEST(test_G_goes_to_last_line);
    RUN_TEST(test_g_goes_to_first_line);
    RUN_TEST(test_find_char_f);
    RUN_TEST(test_find_char_n_repeat);
    RUN_TEST(test_find_char_N_reverse);
    RUN_TEST(test_set_mark_and_jump);
    RUN_TEST(test_jump_to_unset_mark);
    RUN_TEST(test_mark_invalid_id_ignored);
    RUN_TEST(test_mark_clamps_after_lines_deleted);
    RUN_TEST(test_mark_digits_and_uppercase);
    RUN_TEST(test_mark_escape_cancels);
    RUN_TEST(test_word_navigation_w);
    RUN_TEST(test_word_navigation_b);
    RUN_TEST(test_goto_line_rejects_letters);
    RUN_TEST(test_goto_line_auto_cancel_on_empty);
    RUN_TEST(test_goto_line_large_number);
    return UNITY_END();
}
