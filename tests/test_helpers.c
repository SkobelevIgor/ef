#include "test_helpers.h"
#include <stdlib.h>
#include <string.h>

/* --- Mock screen vtable -------------------------------------------------- */

int test_mock_width = 80;
int test_mock_height = 24;

static void mock_render(void *s, Pane **p, int n, int a,
                        Mode m, InputState *i, SplitMode sp)
{ (void)s;(void)p;(void)n;(void)a;(void)m;(void)i;(void)sp; }
static int mock_poll(void *s, EditorEvent *e)
{ (void)s;(void)e; return 0; }
static void mock_size(void *s, int *w, int *h)
{ (void)s; *w = test_mock_width; *h = test_mock_height; }
static void mock_sync(void *s)    { (void)s; }
static void mock_close(void *s)   { (void)s; }
static void mock_suspend(void *s) { (void)s; }
static void mock_resume(void *s)  { (void)s; }

ScreenVTable test_mock_screen = {
    mock_render, mock_poll, mock_size, mock_sync,
    mock_close, mock_suspend, mock_resume, NULL
};

/* --- Shared setup -------------------------------------------------------- */

void test_setup_editor(const wchar_t *lines[], int count,
                       Editor **ed_out, Buffer **buf_out) {
    Buffer *buf = buffer_new();
    for (int i = 0; i < count; i++) {
        int len = (int)wcslen(lines[i]);
        wchar_t *l = malloc(sizeof(wchar_t) * (len + 1));
        wmemcpy(l, lines[i], len);
        l[len] = L'\0';
        if (i == 0) buffer_set_line(buf, 0, l, len);
        else buffer_insert_line_after(buf, buf->line_count - 1, l, len);
    }
    Pane *p = pane_new(buf);
    *ed_out = editor_new_with_deps(&test_mock_screen, &buf, &p, 1,
                                   SPLIT_HORIZONTAL);
    *buf_out = buf;
}

void test_send_char(Editor *ed, wchar_t ch) {
    EditorEvent ev = {EV_KEY, (int)ch, ch, true, false};
    editor_handle_key(ed, &ev);
}

void test_send_key(Editor *ed, int key) {
    EditorEvent ev = {EV_KEY, key, 0, false, false};
    editor_handle_key(ed, &ev);
}

void test_send_paste_char(Editor *ed, wchar_t ch) {
    EditorEvent ev = {EV_KEY, (int)ch, ch, true, true};
    editor_handle_key(ed, &ev);
}
