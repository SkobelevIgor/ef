#include "unity.h"
#include "test_helpers.h"
#include "editor.h"
#include "clipboard.h"
#include "constants.h"
#include "widget.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static Editor *ed;
static Buffer *buf;

static void setup_readonly(const wchar_t *lines[], int count) {
    test_setup_editor(lines, count, &ed, &buf);
    ed->read_only = true;
}

void setUp(void)    { ed = NULL; buf = NULL; }
void tearDown(void) {
    if (ed) { editor_free(ed); ed = NULL; buf = NULL; }
}

/* --- Insert mode blocked ------------------------------------------------- */

void test_readonly_blocks_insert_i(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'i');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_readonly_blocks_insert_a(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'a');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_readonly_blocks_insert_A(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'A');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_readonly_blocks_insert_I(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'I');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_readonly_blocks_insert_o_no_new_line(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    int initial_count = buf->line_count;
    test_send_char(ed, L'o');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(initial_count, buf->line_count);
}

void test_readonly_blocks_insert_O_no_new_line(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    int initial_count = buf->line_count;
    test_send_char(ed, L'O');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(initial_count, buf->line_count);
}

/* --- Visual mode blocked ------------------------------------------------- */

void test_readonly_blocks_visual_v(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'v');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

/* --- Normal mode mutations blocked --------------------------------------- */

void test_readonly_blocks_delete_x(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'x');
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

void test_readonly_blocks_delete_dd(void) {
    const wchar_t *lines[] = {L"hello", L"world"};
    setup_readonly(lines, 2);
    test_send_char(ed, L'd');
    test_send_char(ed, L'd');
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
}

void test_readonly_blocks_delete_dw(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_readonly(lines, 1);
    test_send_char(ed, L'd');
    test_send_char(ed, L'w');
    TEST_ASSERT_EQUAL_INT(11, buf->line_lens[0]);
}

void test_readonly_blocks_paste_p(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    wchar_t *clip = wcsdup(L"x");
    int clip_len = 1;
    clipboard_set(ed->clipboard, &clip, &clip_len, 1, false);
    free(clip);
    test_send_char(ed, L'p');
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

void test_readonly_blocks_paste_P(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    wchar_t *clip = wcsdup(L"x");
    int clip_len = 1;
    clipboard_set(ed->clipboard, &clip, &clip_len, 1, false);
    free(clip);
    test_send_char(ed, L'P');
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

void test_readonly_blocks_undo(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    /* Should stay in normal mode and not crash */
    test_send_char(ed, L'u');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_readonly_blocks_redo(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    EditorEvent ev = {EV_KEY, CTRL_R, (wchar_t)CTRL_R, true, false};
    editor_handle_key(ed, &ev);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

/* --- Find-replace widget cannot mutate in read-only mode ----------------- */

void test_readonly_blocks_widget_replace(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_readonly(lines, 1);
    /* Open find-replace widget, set up a match, then try replace */
    editor_open_find_replace_widget(ed);
    Pane *pane = editor_active_pane(ed);
    WidgetSession *s = widget_current_session(pane->widget);
    /* Simulate a match at col 0, length 5 ("hello") */
    s->matches = malloc(sizeof(SearchMatch));
    s->matches[0].row = 0;
    s->matches[0].col = 0;
    s->matches[0].length = 5;
    s->match_count = 1;
    s->current_index = 0;
    s->replace_text[0] = L'x';
    s->replace_len = 1;
    editor_widget_replace_current(ed);
    /* Buffer must be unchanged */
    TEST_ASSERT_EQUAL_INT(11, buf->line_lens[0]);
}

/* --- Navigation still works ---------------------------------------------- */

void test_readonly_allows_hjkl_navigation(void) {
    const wchar_t *lines[] = {L"hello", L"world"};
    setup_readonly(lines, 2);

    test_send_char(ed, L'j');
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_row);

    test_send_char(ed, L'l');
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);

    test_send_char(ed, L'k');
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_row);

    test_send_char(ed, L'h');
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_col);
}

void test_readonly_allows_word_navigation(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_readonly(lines, 1);

    test_send_char(ed, L'w');
    TEST_ASSERT_EQUAL_INT(6, editor_active_pane(ed)->cursor_col);

    test_send_char(ed, L'b');
    TEST_ASSERT_EQUAL_INT(0, editor_active_pane(ed)->cursor_col);
}

void test_readonly_allows_yank(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_readonly(lines, 1);
    /* yank should work (read-only safe) */
    test_send_char(ed, L'y');
    test_send_char(ed, L'y');
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    /* buffer unchanged */
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_readonly_blocks_insert_i);
    RUN_TEST(test_readonly_blocks_insert_a);
    RUN_TEST(test_readonly_blocks_insert_A);
    RUN_TEST(test_readonly_blocks_insert_I);
    RUN_TEST(test_readonly_blocks_insert_o_no_new_line);
    RUN_TEST(test_readonly_blocks_insert_O_no_new_line);
    RUN_TEST(test_readonly_blocks_visual_v);
    RUN_TEST(test_readonly_blocks_delete_x);
    RUN_TEST(test_readonly_blocks_delete_dd);
    RUN_TEST(test_readonly_blocks_delete_dw);
    RUN_TEST(test_readonly_blocks_paste_p);
    RUN_TEST(test_readonly_blocks_paste_P);
    RUN_TEST(test_readonly_blocks_undo);
    RUN_TEST(test_readonly_blocks_redo);
    RUN_TEST(test_readonly_blocks_widget_replace);
    RUN_TEST(test_readonly_allows_hjkl_navigation);
    RUN_TEST(test_readonly_allows_word_navigation);
    RUN_TEST(test_readonly_allows_yank);
    return UNITY_END();
}
