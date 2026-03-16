#include "unity.h"
#include "test_helpers.h"
#include "insert.h"
#include "autocomplete.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>

static Editor *ed;
static Buffer *buf;

static void setup_editor(const wchar_t *lines[], int count) {
    test_setup_editor(lines, count, &ed, &buf);
    editor_enter_insert_mode(ed);
}

static void send_char(wchar_t ch) {
    test_send_char(ed, ch);
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

    test_send_key(ed, KEY_LEFT);
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

    test_send_key(ed, KEY_DOWN);
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
    /* Enter should accept suggestion -- word completed, no newline inserted */
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

/* --- Map expansion tests ------------------------------------------------- */

static EditorConfig *make_map_config(void) {
    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));
    cfg->map_count = 2;
    cfg->maps = calloc(2, sizeof(EditorMapConfig));
    cfg->maps[0].trigger = strdup("((");
    cfg->maps[0].expansion = strdup("()<Esc>ha");
    cfg->maps[1].trigger = strdup("[[");
    cfg->maps[1].expansion = strdup("[]<Esc>ha");
    return cfg;
}

static void setup_editor_with_maps(const wchar_t *lines[], int count) {
    setup_editor(lines, count);
    ed->config = make_map_config();
}

void test_map_paren_expansion(void) {
    const wchar_t *lines[] = {L""};
    setup_editor_with_maps(lines, 1);

    /* Type "((" -- should expand to "()" with cursor between */
    send_char(L'(');
    send_char(L'(');

    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"()", 2));
    /* Cursor should be between parens (col 1) */
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);
}

void test_map_bracket_expansion(void) {
    const wchar_t *lines[] = {L""};
    setup_editor_with_maps(lines, 1);

    send_char(L'[');
    send_char(L'[');

    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"[]", 2));
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);
}

void test_map_no_match_single_paren(void) {
    const wchar_t *lines[] = {L""};
    setup_editor_with_maps(lines, 1);

    send_char(L'(');
    send_char(L'x');

    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"(x", 2));
}

void test_map_no_config_no_crash(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    /* ed->config is NULL, type "((" normally */
    send_char(L'(');
    send_char(L'(');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"((", 2));
}

void test_map_double_quote_no_infinite_recursion(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    /* Add "" map whose expansion starts with its own trigger */
    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));
    cfg->map_count = 1;
    cfg->maps = calloc(1, sizeof(EditorMapConfig));
    cfg->maps[0].trigger = strdup("\"\"");
    cfg->maps[0].expansion = strdup("\"\"<Esc>ha");
    ed->config = cfg;

    /* Type "" -- should expand to "" with cursor between, NOT segfault */
    send_char(L'"');
    send_char(L'"');

    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"\"\"", 2));
    /* Cursor should be between quotes (col 1) */
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);
}

void test_map_single_quote_no_infinite_recursion(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));
    cfg->map_count = 1;
    cfg->maps = calloc(1, sizeof(EditorMapConfig));
    cfg->maps[0].trigger = strdup("''");
    cfg->maps[0].expansion = strdup("''<Esc>ha");
    ed->config = cfg;

    send_char(L'\'');
    send_char(L'\'');

    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"''", 2));
    TEST_ASSERT_EQUAL_INT(1, editor_active_pane(ed)->cursor_col);
}

void test_map_expansion_with_existing_text(void) {
    const wchar_t *lines[] = {L"hello "};
    setup_editor_with_maps(lines, 1);
    editor_active_pane(ed)->cursor_col = 6;

    send_char(L'(');
    send_char(L'(');

    TEST_ASSERT_EQUAL_INT(8, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"hello ()", 8));
    TEST_ASSERT_EQUAL_INT(7, editor_active_pane(ed)->cursor_col);
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
    RUN_TEST(test_map_paren_expansion);
    RUN_TEST(test_map_bracket_expansion);
    RUN_TEST(test_map_no_match_single_paren);
    RUN_TEST(test_map_no_config_no_crash);
    RUN_TEST(test_map_double_quote_no_infinite_recursion);
    RUN_TEST(test_map_single_quote_no_infinite_recursion);
    RUN_TEST(test_map_expansion_with_existing_text);
    return UNITY_END();
}
