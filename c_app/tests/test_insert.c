#include "unity.h"
#include "editor.h"
#include "insert.h"
#include "autocomplete.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>

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

static void send_key(int key) {
    EditorEvent ev = {EV_KEY, key, 0, false};
    editor_handle_key(ed, &ev);
}

void test_typing_triggers_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    /* Prefix "he" (len 2) should trigger autocomplete */
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_TRUE(ed->input_state->autocomplete->active);
}

void test_tab_accepts_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    /* Tab accepts the suggestion */
    send_char(L'\t');
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
    /* Word should be completed */
    TEST_ASSERT_TRUE(buf->line_lens[1] > 2);
}

void test_escape_dismisses_autocomplete_stays_insert(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    send_char(27); /* Escape */
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode); /* stays in insert */
    TEST_ASSERT_EQUAL_INT(2, p->cursor_col);      /* cursor unchanged */
}

void test_arrow_dismisses_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    send_key(KEY_LEFT);
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
}

void test_down_navigates_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);
    TEST_ASSERT_EQUAL_INT(0, ac->selected_idx);

    send_key(KEY_DOWN);
    /* Should navigate, not move cursor */
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_EQUAL_INT(1, ed->input_state->autocomplete->selected_idx);
    TEST_ASSERT_EQUAL_INT(1, p->cursor_row); /* cursor didn't move */
}

void test_backspace_retriggers_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L"hel"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 3;

    /* Trigger with "hel" prefix */
    editor_trigger_autocomplete(ed);
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    /* Backspace should retrigger (now "he") */
    send_char(127);
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);
}

void test_enter_accepts_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    send_char(L'\n');
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
    /* Enter should accept suggestion — word completed, no newline inserted */
    TEST_ASSERT_TRUE(buf->line_lens[1] > 2);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count); /* no newline inserted */
}

void test_ctrl_n_navigates_autocomplete_down(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);
    TEST_ASSERT_EQUAL_INT(0, ac->selected_idx);

    send_char(14); /* Ctrl+N */
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_EQUAL_INT(1, ed->input_state->autocomplete->selected_idx);
    TEST_ASSERT_EQUAL_INT(1, p->cursor_row); /* cursor didn't move */
}

void test_ctrl_p_navigates_autocomplete_up(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    send_char(L'h');
    send_char(L'e');
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);
    int count = ac->suggestion_count;
    TEST_ASSERT_TRUE(count > 1);

    send_char(16); /* Ctrl+P */
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);
    /* Wraps around to last suggestion */
    TEST_ASSERT_EQUAL_INT(count - 1, ed->input_state->autocomplete->selected_idx);
    TEST_ASSERT_EQUAL_INT(1, p->cursor_row); /* cursor didn't move */
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
    RUN_TEST(test_typing_triggers_autocomplete);
    RUN_TEST(test_tab_accepts_autocomplete);
    RUN_TEST(test_escape_dismisses_autocomplete_stays_insert);
    RUN_TEST(test_arrow_dismisses_autocomplete);
    RUN_TEST(test_down_navigates_autocomplete);
    RUN_TEST(test_backspace_retriggers_autocomplete);
    RUN_TEST(test_enter_accepts_autocomplete);
    RUN_TEST(test_ctrl_n_navigates_autocomplete_down);
    RUN_TEST(test_ctrl_p_navigates_autocomplete_up);
    return UNITY_END();
}
