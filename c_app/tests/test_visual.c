#include "unity.h"
#include "editor.h"
#include "visual.h"
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
    /* Enter visual mode */
    ed->mode = MODE_VISUAL;
    pane_start_selection(p);
}

static void send_char(wchar_t ch) {
    EditorEvent ev = {EV_KEY, (int)ch, ch, true};
    editor_handle_key(ed, &ev);
}

void setUp(void) { ed = NULL; buf = NULL; }

void tearDown(void) {
    if (ed) { editor_free(ed); ed = NULL; buf = NULL; }
}

void test_escape_exits_visual(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(27); /* Escape */
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_FALSE(editor_active_pane(ed)->selection_active);
}

void test_v_toggles_visual(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    send_char(L'v');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
}

void test_visual_yank(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 0;
    pane_start_selection(editor_active_pane(ed));
    editor_active_pane(ed)->cursor_col = 3;

    send_char(L'y');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(1, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
}

void test_visual_delete(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    editor_active_pane(ed)->cursor_col = 0;
    pane_start_selection(editor_active_pane(ed));
    editor_active_pane(ed)->cursor_col = 2;

    send_char(L'd');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    /* "hel" deleted (inclusive), "lo" remains */
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
}

void test_visual_indent(void) {
    const wchar_t *lines[] = {L"abc", L"def"};
    setup_editor(lines, 2);
    buf->config.expand_tab = false;
    editor_active_pane(ed)->cursor_col = 0;
    pane_start_selection(editor_active_pane(ed));
    editor_active_pane(ed)->cursor_row = 1;

    send_char(L'>');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[1][0]);
}

void test_visual_unindent(void) {
    const wchar_t *lines[] = {L"\tabc", L"\tdef"};
    setup_editor(lines, 2);
    editor_active_pane(ed)->cursor_col = 0;
    pane_start_selection(editor_active_pane(ed));
    editor_active_pane(ed)->cursor_row = 1;

    send_char(L'<');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[0]); /* "abc" */
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[1]); /* "def" */
}

void test_visual_yank_multiline(void) {
    const wchar_t *lines[] = {L"hello", L"world", L"test"};
    setup_editor(lines, 3);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 0;
    pane_start_selection(p);

    /* Select down two rows */
    send_char(L'j');
    send_char(L'j');

    send_char(L'y');
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, ed->mode);
    TEST_ASSERT_EQUAL_INT(3, ed->clipboard->line_count);
    TEST_ASSERT_FALSE(ed->clipboard->is_line_mode);
    /* First line: "hello" (from col 0 to end) */
    TEST_ASSERT_EQUAL_INT(5, ed->clipboard->line_lens[0]);
    /* Middle line: "world" (full) */
    TEST_ASSERT_EQUAL_INT(5, ed->clipboard->line_lens[1]);
}

void test_visual_yank_multiline_paste(void) {
    const wchar_t *lines[] = {L"aaa", L"bbb", L"ccc"};
    setup_editor(lines, 3);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 0;
    pane_start_selection(p);

    /* Select first two rows */
    send_char(L'j');
    send_char(L'y');
    TEST_ASSERT_EQUAL_INT(2, ed->clipboard->line_count);

    /* Move to row 2, col 1 and paste after */
    p->cursor_row = 2;
    p->cursor_col = 1;
    send_char(L'p');

    /* Original "ccc" at col 1 → insert_pos=2
     * clipboard[0]="aaa", clipboard[1]="b" (first char of "bbb")
     * first line: "cc" + "aaa" = "ccaaa"
     * last line:  "b" + "c" = "bc"
     */
    TEST_ASSERT_EQUAL_INT(4, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[2], L"ccaaa", 5));
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[3], L"bc", 2));
}

void test_visual_yank_multiline_paste_before(void) {
    const wchar_t *lines[] = {L"abc"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);

    /* Manually set clipboard with 2-line content */
    wchar_t *cb_lines[] = {L"XX", L"YY"};
    int cb_lens[] = {2, 2};
    clipboard_set(ed->clipboard, cb_lines, cb_lens, 2, false);

    /* Paste before at col 1 */
    ed->mode = MODE_NORMAL;
    p->cursor_col = 1;
    send_char(L'P');

    /* insert_pos=1: "a" + "XX" = "aXX", "YY" + "bc" = "YYbc" */
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"aXX", 3));
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"YYbc", 4));
}

void test_visual_navigation(void) {
    const wchar_t *lines[] = {L"hello", L"world"};
    setup_editor(lines, 2);

    send_char(L'j');
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_row);
    TEST_ASSERT_EQUAL_INT(MODE_VISUAL, ed->mode); /* Still visual */
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_escape_exits_visual);
    RUN_TEST(test_v_toggles_visual);
    RUN_TEST(test_visual_yank);
    RUN_TEST(test_visual_delete);
    RUN_TEST(test_visual_indent);
    RUN_TEST(test_visual_unindent);
    RUN_TEST(test_visual_yank_multiline);
    RUN_TEST(test_visual_yank_multiline_paste);
    RUN_TEST(test_visual_yank_multiline_paste_before);
    RUN_TEST(test_visual_navigation);
    return UNITY_END();
}
