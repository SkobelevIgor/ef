#include "unity.h"
#include "test_helpers.h"
#include "widget.h"
#include "search.h"
#include <stdlib.h>
#include <locale.h>
#include <ncurses.h>

static Editor *ed;
static Buffer *buf;

static void setup_editor(const wchar_t *lines[], int count) {
    test_setup_editor(lines, count, &ed, &buf);
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

void setUp(void) { ed = NULL; buf = NULL; }
void tearDown(void) {
    if (ed) editor_free(ed);
    ed = NULL; buf = NULL;
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

void test_f3_when_active_resets_focus_to_findbar(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    WidgetState *w = editor_active_pane(ed)->widget;
    w->focus = FOCUS_EDITOR; /* simulate being on occurrences */

    editor_open_find_replace_widget(ed);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);
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

    /* Escape after confirm -> keeps cursor */
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

    /* Both "hello" replaced with "hi" -> "hi world hi" */
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

    /* Tab toggles back to FindBar (not to Editor) */
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

/* --- Control chars as is_char=true (real ncurses behavior) --------------- */

void test_escape_as_char_closes_widget(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    TEST_ASSERT_TRUE(pane_has_active_widget(editor_active_pane(ed)));

    /* Real ncurses sends Escape as is_char=true, ch=27 */
    EditorEvent esc = make_char_event(27);
    editor_handle_widget_mode(ed, &esc);
    TEST_ASSERT_NULL(editor_active_pane(ed)->widget);
}

void test_enter_as_char_confirms_search(void) {
    const wchar_t *lines[] = {L"hello world", L"foo hello bar"};
    setup_editor(lines, 2);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    /* Real ncurses sends Enter as is_char=true, ch='\r' */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);

    WidgetState *w = editor_active_pane(ed)->widget;
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, w->focus);
    TEST_ASSERT_TRUE(w->confirmed);
}

void test_backspace_as_char_deletes_query(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    EditorEvent ev_h = make_char_event(L'h');
    EditorEvent ev_e = make_char_event(L'e');
    editor_handle_widget_mode(ed, &ev_h);
    editor_handle_widget_mode(ed, &ev_e);

    /* Real ncurses sends Backspace as is_char=true, ch=127 */
    EditorEvent bs = make_char_event(127);
    editor_handle_widget_mode(ed, &bs);

    WidgetSession *s = editor_active_pane(ed)->widget->search_session;
    TEST_ASSERT_EQUAL_INT(1, s->query_len);
}

void test_tab_as_char_cycles_focus(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    WidgetState *w = editor_active_pane(ed)->widget;
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);

    /* Real ncurses sends Tab as is_char=true, ch='\t' */
    EditorEvent tab = make_char_event(L'\t');
    editor_handle_widget_mode(ed, &tab);
    TEST_ASSERT_EQUAL_INT(FOCUS_REPLACE_BAR, w->focus);

    /* Tab toggles back to FindBar (not to Editor) */
    editor_handle_widget_mode(ed, &tab);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);
}

void test_enter_as_char_noop_in_editor_focus(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_search_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    /* Confirm with Enter */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, editor_active_pane(ed)->widget->focus);

    Pane *p = editor_active_pane(ed);
    int row_before = p->cursor_row, col_before = p->cursor_col;

    /* Another Enter in FocusEditor for Search mode is a no-op */
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(row_before, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(col_before, p->cursor_col);
}

void test_full_search_lifecycle_with_real_keys(void) {
    const wchar_t *lines[] = {L"hello world hello again hello"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 0; p->cursor_col = 0;

    /* 1. F4 opens search */
    EditorEvent f4 = make_key_event(KEY_F(4));
    editor_handle_key(ed, &f4);
    TEST_ASSERT_TRUE(pane_has_active_widget(p));

    /* 2. Type "hello" */
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    WidgetSession *s = p->widget->search_session;
    TEST_ASSERT_TRUE(s->match_count >= 3);

    /* 3. Enter confirms (real ncurses sends \r) */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, p->widget->focus);
    TEST_ASSERT_TRUE(p->widget->confirmed);

    /* 4. n navigates to next match */
    int col_after_enter = p->cursor_col;
    EditorEvent ev_n = make_char_event(L'n');
    editor_handle_widget_mode(ed, &ev_n);
    int col_after_n = p->cursor_col;
    TEST_ASSERT_TRUE(col_after_n != col_after_enter
                     || s->current_index != 0);

    /* 5. N navigates back */
    EditorEvent ev_N = make_char_event(L'N');
    editor_handle_widget_mode(ed, &ev_N);

    /* 6. Escape keeps cursor (confirmed=true) */
    int final_row = p->cursor_row, final_col = p->cursor_col;
    EditorEvent esc = make_char_event(27);
    editor_handle_widget_mode(ed, &esc);
    TEST_ASSERT_NULL(p->widget);
    TEST_ASSERT_EQUAL_INT(final_row, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(final_col, p->cursor_col);
}

void test_escape_before_enter_restores_anchor(void) {
    const wchar_t *lines[] = {L"hello world hello"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 0; p->cursor_col = 6;

    editor_open_search_widget(ed);

    /* Type "hello" - auto-navigates at 2+ chars */
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    /* Escape WITHOUT Enter -> restore anchor */
    EditorEvent esc = make_char_event(27);
    editor_handle_widget_mode(ed, &esc);
    TEST_ASSERT_NULL(p->widget);
    TEST_ASSERT_EQUAL_INT(0, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(6, p->cursor_col);
}

/* --- FindReplace Enter flow: FindBar->ReplaceBar->Editor->Replace ------- */

void test_fr_enter_on_findbar_moves_to_replacebar(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }

    /* Enter on FindBar -> focus moves to ReplaceBar */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_REPLACE_BAR, editor_active_pane(ed)->widget->focus);
}

void test_fr_enter_on_replacebar_moves_to_editor(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    WidgetState *w = editor_active_pane(ed)->widget;
    w->focus = FOCUS_REPLACE_BAR;

    /* Enter on ReplaceBar (even empty) -> focus moves to Editor */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, w->focus);
    TEST_ASSERT_TRUE(w->confirmed);
}

void test_fr_enter_on_editor_replaces(void) {
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

    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);

    wchar_t *line = buf->lines[0];
    TEST_ASSERT_TRUE(line[0] == L'h' && line[1] == L'i' && line[2] == L' ');
}

void test_fr_full_lifecycle(void) {
    const wchar_t *lines[] = {L"hello world hello again hello"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);

    /* 1. F3 opens find-replace */
    EditorEvent f3 = make_key_event(KEY_F(3));
    editor_handle_key(ed, &f3);
    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, p->widget->kind);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, p->widget->focus);

    /* 2. Type "hello" in find bar */
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    WidgetSession *s = p->widget->find_replace_session;
    TEST_ASSERT_TRUE(s->match_count >= 3);

    /* 3. Enter -> moves to replace bar */
    EditorEvent enter = make_char_event(L'\r');
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_REPLACE_BAR, p->widget->focus);

    /* 4. Type "hi" in replace bar */
    for (const wchar_t *c = L"hi"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    TEST_ASSERT_EQUAL_INT(2, s->replace_len);

    /* 5. Enter -> moves to editor */
    editor_handle_widget_mode(ed, &enter);
    TEST_ASSERT_EQUAL_INT(FOCUS_EDITOR, p->widget->focus);
    TEST_ASSERT_TRUE(p->widget->confirmed);

    /* 6. n/N navigate between matches */
    int idx_before = s->current_index;
    EditorEvent ev_n = make_char_event(L'n');
    editor_handle_widget_mode(ed, &ev_n);
    TEST_ASSERT_TRUE(s->current_index != idx_before
                     || s->match_count == 1);

    EditorEvent ev_N = make_char_event(L'N');
    editor_handle_widget_mode(ed, &ev_N);
    TEST_ASSERT_EQUAL_INT(idx_before, s->current_index);

    /* 7. Enter -> replaces current match */
    editor_handle_widget_mode(ed, &enter);
    /* At least one "hello" replaced with "hi" */
    TEST_ASSERT_TRUE(buf->line_lens[0] < 29);

    /* 8. Escape keeps cursor (confirmed) */
    int final_row = p->cursor_row, final_col = p->cursor_col;
    EditorEvent esc = make_char_event(27);
    editor_handle_widget_mode(ed, &esc);
    TEST_ASSERT_NULL(p->widget);
    TEST_ASSERT_EQUAL_INT(final_row, p->cursor_row);
    TEST_ASSERT_EQUAL_INT(final_col, p->cursor_col);
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

void test_switch_back_recovers_from_no_matches(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);

    editor_open_find_replace_widget(ed);
    for (const wchar_t *c = L"xyz"; *c; c++) {
        EditorEvent ev = make_char_event(*c);
        editor_handle_widget_mode(ed, &ev);
    }
    WidgetSession *s = editor_active_pane(ed)->widget->find_replace_session;
    TEST_ASSERT_EQUAL_INT(-1, s->current_index);

    /* Switch away, make the text match, switch back */
    editor_open_search_widget(ed);
    wchar_t *l = malloc(sizeof(wchar_t) * 4);
    wmemcpy(l, L"xyz", 3); l[3] = L'\0';
    buffer_set_line(buf, 0, l, 3);
    editor_open_find_replace_widget(ed);

    TEST_ASSERT_EQUAL_INT(1, s->match_count);
    TEST_ASSERT_EQUAL_INT(0, s->current_index);
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
    RUN_TEST(test_f3_when_active_resets_focus_to_findbar);
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
    /* FindReplace Enter flow */
    RUN_TEST(test_fr_enter_on_findbar_moves_to_replacebar);
    RUN_TEST(test_fr_enter_on_replacebar_moves_to_editor);
    RUN_TEST(test_fr_enter_on_editor_replaces);
    RUN_TEST(test_fr_full_lifecycle);
    /* F3/F4 in widget */
    RUN_TEST(test_f3_during_search_switches);
    RUN_TEST(test_f4_during_find_replace_switches);
    /* Dual sessions */
    RUN_TEST(test_dual_sessions_independent);
    RUN_TEST(test_switch_back_recovers_from_no_matches);
    /* Real ncurses key behavior (is_char=true for control chars) */
    RUN_TEST(test_escape_as_char_closes_widget);
    RUN_TEST(test_enter_as_char_confirms_search);
    RUN_TEST(test_backspace_as_char_deletes_query);
    RUN_TEST(test_tab_as_char_cycles_focus);
    RUN_TEST(test_enter_as_char_noop_in_editor_focus);
    RUN_TEST(test_full_search_lifecycle_with_real_keys);
    RUN_TEST(test_escape_before_enter_restores_anchor);
    return UNITY_END();
}
