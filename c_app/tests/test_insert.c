#include "unity.h"
#include "editor.h"
#include "insert.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static int mock_width = 80, mock_height = 24;
static void mock_render(void *s, Pane **p, int n, int a, Mode m, InputState *i, SplitMode sp) { (void)s;(void)p;(void)n;(void)a;(void)m;(void)i;(void)sp; }
static int mock_poll(void *s, EditorEvent *e) { (void)s;(void)e; return 0; }
static void mock_size(void *s, int *w, int *h) { (void)s; *w = mock_width; *h = mock_height; }
static void mock_sync(void *s) { (void)s; }
static void mock_close(void *s) { (void)s; }
static void mock_suspend(void *s) { (void)s; }
static void mock_resume(void *s) { (void)s; }

static ScreenVTable mock_screen = {
    mock_render, mock_poll, mock_size, mock_sync, mock_close, mock_suspend, mock_resume, NULL
};

static Editor *ed;
static Buffer *buf;

static void setup_editor(const wchar_t *lines[], int count) {
    buf = buffer_new();
    for (int i = 0; i < count; i++) {
        int len = (int)wcslen(lines[i]);
        wchar_t *l = malloc(sizeof(wchar_t) * (len + 1));
        wmemcpy(l, lines[i], len); l[len] = L'\0';
        if (i == 0) buffer_set_line(buf, 0, l, len);
        else buffer_insert_line_after(buf, buf->line_count - 1, l, len);
    }
    Pane *p = pane_new(buf);
    ed = editor_new_with_deps(&mock_screen, &buf, &p, 1, SPLIT_HORIZONTAL);
    /* Enter insert mode */
    editor_enter_insert_mode(ed);
}

static void send_char(wchar_t ch) {
    EditorEvent ev = {EV_KEY, (int)ch, ch, true};
    editor_handle_key(ed, &ev);
}

void setUp(void) { ed = NULL; buf = NULL; }

void tearDown(void) {
    if (ed) { editor_free(ed); ed = NULL; buf = NULL; }
}

void test_insert_char(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    send_char(L'h');
    send_char(L'i');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"hi", 2));
}

void test_escape_returns_to_normal(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 3;

    send_char(27); /* Escape */
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(2, editor_active_pane(ed)->cursor_col); /* back one */
}

void test_backspace_deletes(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 2;

    send_char(127); /* Backspace */
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);
}

void test_enter_inserts_newline(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 2;

    send_char(L'\n');
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]); /* "he" */
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[1]); /* "llo" */
}

void test_insert_after_escape_undoable(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    send_char(L'a');
    send_char(L'b');
    send_char(27); /* Escape */

    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_TRUE(history_can_undo(ed->history));
}

void test_insert_tab_literal(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    buf->config.expand_tab = false;

    send_char(L'\t');
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[0][0]);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_insert_char);
    RUN_TEST(test_escape_returns_to_normal);
    RUN_TEST(test_backspace_deletes);
    RUN_TEST(test_enter_inserts_newline);
    RUN_TEST(test_insert_after_escape_undoable);
    RUN_TEST(test_insert_tab_literal);
    return UNITY_END();
}
