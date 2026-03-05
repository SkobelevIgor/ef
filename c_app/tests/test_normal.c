#include "unity.h"
#include "editor.h"
#include "normal.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>

/* Mock screen vtable */
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
}

static void send_char(wchar_t ch) {
    EditorEvent ev = {EV_KEY, (int)ch, ch, true};
    editor_handle_key(ed, &ev);
}

void setUp(void) { ed = NULL; buf = NULL; }

void tearDown(void) {
    if (ed) {
        /* Detach buffers/panes since editor_free frees them */
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
    TEST_ASSERT_TRUE(ed->clipboard->is_line_mode);
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
    RUN_TEST(test_yy_yanks_line);
    RUN_TEST(test_x_deletes_char);
    RUN_TEST(test_undo);
    RUN_TEST(test_goto_line_colon);
    RUN_TEST(test_G_goes_to_last_line);
    RUN_TEST(test_g_goes_to_first_line);
    RUN_TEST(test_find_char_f);
    RUN_TEST(test_word_navigation_w);
    RUN_TEST(test_word_navigation_b);
    return UNITY_END();
}
