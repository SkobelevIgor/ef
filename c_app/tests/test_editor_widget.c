#include "unity.h"
#include "editor.h"
#include "widget.h"
#include "search.h"
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>

/* --- Mock screen --------------------------------------------------------- */
static void mock_render(void *s, Pane **p, int n, int a, Mode m, InputState *i, SplitMode sp) {
    (void)s;(void)p;(void)n;(void)a;(void)m;(void)i;(void)sp;
}
static int mock_poll(void *s, EditorEvent *ev) { (void)s;(void)ev; return 0; }
static void mock_size(void *s, int *w, int *h) { (void)s; *w = 80; *h = 24; }
static void mock_noop(void *s) { (void)s; }

static ScreenVTable *make_mock_screen(void) {
    ScreenVTable *vt = calloc(1, sizeof(ScreenVTable));
    vt->render = mock_render;
    vt->poll_event = mock_poll;
    vt->get_size = mock_size;
    vt->sync = mock_noop;
    vt->close = mock_noop;
    vt->suspend = mock_noop;
    vt->resume = mock_noop;
    return vt;
}

/* --- Test helpers -------------------------------------------------------- */
static Editor *ed;
static ScreenVTable *scr;
static Buffer *buf;

static void setup_editor(const wchar_t *lines[], int count) {
    scr = make_mock_screen();
    buf = buffer_new();
    for (int i = 0; i < count; i++) {
        int len = (int)wcslen(lines[i]);
        wchar_t *l = malloc(sizeof(wchar_t) * (len + 1));
        wmemcpy(l, lines[i], len); l[len] = L'\0';
        if (i == 0) {
            buffer_set_line(buf, 0, l, len);
        } else {
            buffer_insert_line_after(buf, buf->line_count - 1, l, len);
        }
    }
    Pane *pane = pane_new(buf);
    ed = editor_new_with_deps(scr, &buf, &pane, 1, SPLIT_HORIZONTAL);
}

static EditorEvent make_char_event(wchar_t ch) {
    EditorEvent ev = {0};
    ev.type = EV_KEY;
    ev.key = (int)ch;
    ev.ch = ch;
    ev.is_char = true;
    return ev;
}

static EditorEvent make_key_event(int key) {
    EditorEvent ev = {0};
    ev.type = EV_KEY;
    ev.key = key;
    ev.is_char = false;
    return ev;
}

void setUp(void) { ed = NULL; scr = NULL; buf = NULL; }
void tearDown(void) {
    if (ed) editor_free(ed);
    free(scr);
}

/* --- openSearchWidget (F4) ----------------------------------------------- */

void test_f4_opens_search_widget(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    EditorEvent ev = make_key_event(KEY_F(4));
    editor_handle_key(ed, &ev);

    Pane *p = editor_active_pane(ed);
    TEST_ASSERT_TRUE(pane_has_active_widget(p));
    TEST_ASSERT_EQUAL_INT(WIDGET_SEARCH, p->widget->kind);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, p->widget->focus);
}

void test_f4_cycle_focus(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, editor_active_pane(ed)->widget->focus);

    editor_open_search_widget(ed);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, editor_active_pane(ed)->widget->focus);

    editor_open_search_widget(ed);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, editor_active_pane(ed)->widget->focus);
}

void test_f4_switches_from_find_replace(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, editor_active_pane(ed)->widget->kind);

    editor_open_search_widget(ed);
    TEST_ASSERT_EQUAL_INT(WIDGET_SEARCH, editor_active_pane(ed)->widget->kind);
    TEST_ASSERT_NOT_NULL(editor_active_pane(ed)->widget->find_replace_session);
}

/* --- openFindReplaceWidget (F3) ------------------------------------------ */

void test_f3_opens_find_replace(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    EditorEvent ev = make_key_event(KEY_F(3));
    editor_handle_key(ed, &ev);

    Pane *p = editor_active_pane(ed);
    TEST_ASSERT_TRUE(pane_has_active_widget(p));
    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, p->widget->kind);
}

void test_f3_when_active_is_noop(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, editor_active_pane(ed)->widget->focus);

    editor_open_find_replace_widget(ed); /* should be no-op */
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, editor_active_pane(ed)->widget->focus);
}

void test_f3_switches_from_search(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    widget_session_append_query(editor_active_pane(ed)->widget->search_session, L'h');

    editor_open_find_replace_widget(ed);
    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, editor_active_pane(ed)->widget->kind);
    TEST_ASSERT_NOT_NULL(editor_active_pane(ed)->widget->search_session);
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->widget->search_session->query_len);
}

/* --- closeWidget (Escape) ------------------------------------------------ */

void test_escape_closes_widget(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 0; p->cursor_col = 5;

    editor_open_search_widget(ed);
    p->cursor_row = 0; p->cursor_col = 0; /* simulate navigation */

    editor_close_widget(ed);
    TEST_ASSERT_NULL(p->widget);
    TEST_ASSERT_EQUAL_INT(5, p->cursor_col); /* restored to anchor */
}

void test_escape_nil_widget_noop(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    editor_close_widget(ed); /* no crash */
}

void test_close_confirmed_keeps_cursor(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 6;

    editor_open_search_widget(ed);

    /* Type "hello" and confirm */
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);

    int match_row = p->cursor_row;
    int match_col = p->cursor_col;

    /* Escape after confirm → keeps cursor */
    EditorEvent esc = make_key_event(27);
    editor_handle_widget_mode(ed, &esc);

    TEST_ASSERT_EQUAL_INT(match_row, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(match_col, p->cursor_col);
}

/* --- FindBar typing ------------------------------------------------------ */

void test_find_bar_type_builds_query(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    EditorEvent ev_h = make_char_event(L'h');
    EditorEvent ev_e = make_char_event(L'e');
    editor_handle_widget_mode(ed, &ev_h);
    editor_handle_widget_mode(ed, &ev_e);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    TEST_ASSERT_EQUAL_INT(2, s->query_len);
    TEST_ASSERT_TRUE(s->query[0] == L'h' && s->query[1] == L'e');
}

void test_find_bar_auto_search_at_2_chars(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar"};
    setup_editor(lines, 2);

    editor_open_search_widget(ed);
    EditorEvent ev_h = make_char_event(L'h');
    editor_handle_widget_mode(ed, &ev_h);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    TEST_ASSERT_TRUE(s->match_count > 0);

    EditorEvent ev_e = make_char_event(L'e');
    editor_handle_widget_mode(ed, &ev_e);

    TEST_ASSERT_TRUE(s->current_index >= 0);
}

void test_find_bar_backspace(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    EditorEvent ev_h = make_char_event(L'h');
    EditorEvent ev_e = make_char_event(L'e');
    editor_handle_widget_mode(ed, &ev_h);
    editor_handle_widget_mode(ed, &ev_e);

    EditorEvent bs = make_key_event(127);
    editor_handle_widget_mode(ed, &bs);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    TEST_ASSERT_EQUAL_INT(1, s->query_len);

    editor_handle_widget_mode(ed, &bs);
    TEST_ASSERT_EQUAL_INT(0, s->query_len);

    editor_handle_widget_mode(ed, &bs); /* no crash on empty */
    TEST_ASSERT_EQUAL_INT(0, s->query_len);
}

void test_find_bar_no_matches(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"zzz"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    TEST_ASSERT_TRUE(s->no_matches);
    TEST_ASSERT_EQUAL_INT(0, s->match_count);
}

/* --- Enter confirms search ----------------------------------------------- */

void test_search_enter_confirms(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar"};
    setup_editor(lines, 2);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);

    WidgetState *w = editor_active_pane(ed)->widget;
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, w->focus);
    TEST_ASSERT_TRUE(w->confirmed);
}

/* --- n/N navigation ------------------------------------------------------ */

void test_editor_focus_next_prev(void) {
    const wchar_t *lines[] = {L"hello world hello again hello"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    int first_idx = s->current_index;

    EditorEvent ev_n = make_char_event(L'n');
    editor_handle_widget_mode(ed, &ev_n);
    TEST_ASSERT_TRUE(s->current_index != first_idx);

    EditorEvent ev_N = make_char_event(L'N');
    editor_handle_widget_mode(ed, &ev_N);
    TEST_ASSERT_EQUAL_INT(first_idx, s->current_index);
}

void test_editor_focus_wrap_around(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar", L"hello again"};
    setup_editor(lines, 3);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    int total = s->match_count;
    TEST_ASSERT_TRUE(total >= 3);

    s->current_index = total - 1;
    editor_widget_next_match(ed);
    TEST_ASSERT_EQUAL_INT(0, s->current_index);

    editor_widget_prev_match(ed);
    TEST_ASSERT_EQUAL_INT(total - 1, s->current_index);
}

/* --- Find-Replace Enter replaces ----------------------------------------- */

void test_find_replace_enter_replaces(void) {
    const wchar_t *lines[] = {L"hello world hello"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    WidgetState *w = editor_active_pane(ed)->widget;
    widget_session_append_replace(w->find_replace_session, L'h');
    widget_session_append_replace(w->find_replace_session, L'i');
    w->focus = FOCUS_EDITOR;

    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);

    /* Verify "hello" was replaced with "hi" */
    wchar_t *line = buf->lines[0];
    /* "hi world hello" */
    TEST_ASSERT_TRUE(line[0] == L'h' && line[1] == L'i' && line[2] == L' ');
}

void test_find_replace_repeated_enter(void) {
    const wchar_t *lines[] = {L"hello world hello"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    WidgetState *w = editor_active_pane(ed)->widget;
    widget_session_append_replace(w->find_replace_session, L'h');
    widget_session_append_replace(w->find_replace_session, L'i');
    w->focus = FOCUS_EDITOR;

    EditorEvent enter = make_key_event('\n');
    editor_handle_widget_mode(ed, &enter);
    editor_handle_widget_mode(ed, &enter);

    /* Both "hello" replaced with "hi" → "hi world hi" */
    TEST_ASSERT_EQUAL_INT(11, buf->line_lens[0]);
}

/* --- Tab cycling in FindReplace ------------------------------------------ */

void test_tab_cycles_find_replace(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    WidgetState *w = editor_active_pane(ed)->widget;
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);

    EditorEvent tab = make_key_event('\t');
    editor_handle_widget_mode(ed, &tab);
    TEST_ASSERT_EQUAL_INT(FOCUS_REPLACE_BAR, w->focus);

    editor_handle_widget_mode(ed, &tab);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, w->focus);

    editor_handle_widget_mode(ed, &tab);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);
}

/* --- ReplaceBar typing --------------------------------------------------- */

void test_replace_bar_typing(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    editor_active_pane(ed)->widget->focus = FOCUS_REPLACE_BAR;

    EditorEvent ev_a = make_char_event(L'a');
    EditorEvent ev_b = make_char_event(L'b');
    editor_handle_widget_mode(ed, &ev_a);
    editor_handle_widget_mode(ed, &ev_b);

    WidgetSession *s = editor_active_pane(ed)->widget->find_replace_session;
    TEST_ASSERT_EQUAL_INT(2, s->replace_len);
}

/* --- F3/F4 within widget mode -------------------------------------------- */

void test_f3_during_search_switches(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    EditorEvent ev = make_key_event(KEY_F(3));
    editor_handle_widget_mode(ed, &ev);

    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, editor_active_pane(ed)->widget->kind);
}

void test_f4_during_find_replace_switches(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    EditorEvent ev = make_key_event(KEY_F(4));
    editor_handle_widget_mode(ed, &ev);

    TEST_ASSERT_EQUAL_INT(WIDGET_SEARCH, editor_active_pane(ed)->widget->kind);
}

/* --- Dual session independence ------------------------------------------- */

void test_dual_sessions_independent(void) {
    const wchar_t *lines[] = {L"hello world hello"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    widget_session_append_query(editor_active_pane(ed)->widget->search_session, L'h');

    editor_open_find_replace_widget(ed);
    widget_session_append_query(editor_active_pane(ed)->widget->find_replace_session, L'w');

    editor_open_search_widget(ed);
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->widget->search_session->query_len);
    TEST_ASSERT_TRUE(editor_active_pane(ed)->widget->search_session->query[0] == L'h');

    editor_open_find_replace_widget(ed);
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->widget->find_replace_session->query_len);
    TEST_ASSERT_TRUE(editor_active_pane(ed)->widget->find_replace_session->query[0] == L'w');
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    /* F4 */
    RUN_TEST(test_f4_opens_search_widget);
    RUN_TEST(test_f4_cycle_focus);
    RUN_TEST(test_f4_switches_from_find_replace);
    /* F3 */
    RUN_TEST(test_f3_opens_find_replace);
    RUN_TEST(test_f3_when_active_is_noop);
    RUN_TEST(test_f3_switches_from_search);
    /* Close */
    RUN_TEST(test_escape_closes_widget);
    RUN_TEST(test_escape_nil_widget_noop);
    RUN_TEST(test_close_confirmed_keeps_cursor);
    /* FindBar typing */
    RUN_TEST(test_find_bar_type_builds_query);
    RUN_TEST(test_find_bar_auto_search_at_2_chars);
    RUN_TEST(test_find_bar_backspace);
    RUN_TEST(test_find_bar_no_matches);
    /* Enter confirms */
    RUN_TEST(test_search_enter_confirms);
    /* n/N navigation */
    RUN_TEST(test_editor_focus_next_prev);
    RUN_TEST(test_editor_focus_wrap_around);
    /* Replace */
    RUN_TEST(test_find_replace_enter_replaces);
    RUN_TEST(test_find_replace_repeated_enter);
    /* Tab cycling */
    RUN_TEST(test_tab_cycles_find_replace);
    /* Replace bar */
    RUN_TEST(test_replace_bar_typing);
    /* F3/F4 in widget */
    RUN_TEST(test_f3_during_search_switches);
    RUN_TEST(test_f4_during_find_replace_switches);
    /* Dual sessions */
    RUN_TEST(test_dual_sessions_independent);
    return UNITY_END();
}
