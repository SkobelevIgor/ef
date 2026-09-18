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

void test_insert_latin_extended_char_not_treated_as_f10(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    /* U+0112 == KEY_F(10) numerically, but is_char must win */
    EditorEvent ev = {EV_KEY, KEY_F(10), L'\u0112', true, false};
    TEST_ASSERT_FALSE(editor_handle_key(ed, &ev));
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, ed->mode);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_TRUE(buf->lines[0][0] == L'\u0112');
}

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

void test_map_cleared_by_cursor_movement(void) {
    const wchar_t *lines[] = {L"abcdef"};
    setup_editor(lines, 1);
    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));
    cfg->map_count = 1;
    cfg->maps = calloc(1, sizeof(EditorMapConfig));
    cfg->maps[0].trigger = strdup("xy");
    cfg->maps[0].expansion = strdup("ZZ");
    ed->config = cfg;

    send_char(L'x');
    test_send_key(ed, KEY_RIGHT);
    test_send_key(ed, KEY_RIGHT);
    test_send_key(ed, KEY_RIGHT);
    send_char(L'y');

    TEST_ASSERT_EQUAL_INT(8, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"xabcydef", 8));
}

void test_map_cleared_by_backspace(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));
    cfg->map_count = 1;
    cfg->maps = calloc(1, sizeof(EditorMapConfig));
    cfg->maps[0].trigger = strdup("xy");
    cfg->maps[0].expansion = strdup("ZZ");
    ed->config = cfg;

    send_char(L'x');
    send_char(127);
    send_char(L'y');

    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_TRUE(buf->lines[0][0] == L'y');
}

/* --- Paste mode tests ---------------------------------------------------- */

static void send_paste_char(wchar_t ch) {
    test_send_paste_char(ed, ch);
}

void test_insert_cyrillic_char(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    send_char(L'\x41f'); /* Cyrillic П (U+041F) */
    send_char(L'\x440'); /* Cyrillic р (U+0440) */
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\x41f', buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(L'\x440', buf->lines[0][1]);
}

void test_paste_inserts_chars(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    send_paste_char(L'h');
    send_paste_char(L'i');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"hi", 2));
}

void test_paste_preserves_newlines(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    /* Paste: "ab\ncd" */
    send_paste_char(L'a');
    send_paste_char(L'b');
    send_paste_char(L'\n');
    send_paste_char(L'c');
    send_paste_char(L'd');

    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]); /* "ab" */
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"ab", 2));
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[1]); /* "cd" */
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"cd", 2));
}

void test_paste_skips_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    /* Paste "he" — should NOT trigger autocomplete */
    send_paste_char(L'h');
    send_paste_char(L'e');
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
}

void test_paste_newline_with_active_autocomplete(void) {
    /* This is the critical bug: typing "pd" triggers AC suggesting "pandas",
       then pasting \n should create a newline, NOT accept autocomplete */
    const wchar_t *lines[] = {L"pandas numpy", L""};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 0;

    /* Type "pd" normally to trigger AC */
    send_char(L'p');
    send_char(L'd');
    TEST_ASSERT_NOT_NULL(ed->input_state->autocomplete);

    /* Now paste a newline — should dismiss AC and insert newline */
    send_paste_char(L'\n');
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_EQUAL_INT(3, buf->line_count);
    /* Line 1 should still be "pd", not "pandas" */
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"pd", 2));
}

void test_paste_skips_map_triggers(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);
    ed->config = make_map_config();

    /* Paste "((" — should NOT trigger map expansion */
    send_paste_char(L'(');
    send_paste_char(L'(');

    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"((", 2));
}

void test_paste_no_auto_indent(void) {
    const wchar_t *lines[] = {L"    indented"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 12; /* end of line */

    /* Paste newline + unindented text */
    send_paste_char(L'\n');
    send_paste_char(L'x');

    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    /* New line should have just "x", no auto-indent */
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(L'x', buf->lines[1][0]);
}

void test_paste_ignored_in_normal_mode(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);
    /* Stay in normal mode (don't call editor_enter_insert_mode) */
    ed->mode = MODE_NORMAL;

    /* Paste "dd" — should NOT delete the line */
    test_send_paste_char(ed, L'd');
    test_send_paste_char(ed, L'd');

    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"hello", 5));
}

void test_paste_cyrillic_chars(void) {
    const wchar_t *lines[] = {L""};
    setup_editor(lines, 1);

    /* Paste Russian text */
    send_paste_char(L'\x41f'); /* П */
    send_paste_char(L'\x440'); /* р */
    send_paste_char(L'\x438'); /* и */
    send_paste_char(L'\x432'); /* в */
    send_paste_char(L'\x435'); /* е */
    send_paste_char(L'\x442'); /* т */

    TEST_ASSERT_EQUAL_INT(6, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\x41f', buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(L'\x442', buf->lines[0][5]);
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
    RUN_TEST(test_insert_latin_extended_char_not_treated_as_f10);
    RUN_TEST(test_map_paren_expansion);
    RUN_TEST(test_map_bracket_expansion);
    RUN_TEST(test_map_no_match_single_paren);
    RUN_TEST(test_map_no_config_no_crash);
    RUN_TEST(test_map_double_quote_no_infinite_recursion);
    RUN_TEST(test_map_single_quote_no_infinite_recursion);
    RUN_TEST(test_map_expansion_with_existing_text);
    RUN_TEST(test_map_cleared_by_cursor_movement);
    RUN_TEST(test_map_cleared_by_backspace);
    /* Paste mode tests */
    RUN_TEST(test_insert_cyrillic_char);
    RUN_TEST(test_paste_inserts_chars);
    RUN_TEST(test_paste_preserves_newlines);
    RUN_TEST(test_paste_skips_autocomplete);
    RUN_TEST(test_paste_newline_with_active_autocomplete);
    RUN_TEST(test_paste_skips_map_triggers);
    RUN_TEST(test_paste_no_auto_indent);
    RUN_TEST(test_paste_ignored_in_normal_mode);
    RUN_TEST(test_paste_cyrillic_chars);
    return UNITY_END();
}
