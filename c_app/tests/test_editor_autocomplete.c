#include "unity.h"
#include "editor.h"
#include "autocomplete.h"
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
        if (i == 0) buffer_set_line(buf, 0, l, len);
        else buffer_insert_line_after(buf, buf->line_count - 1, l, len);
    }
    Pane *pane = pane_new(buf);
    ed = editor_new_with_deps(scr, &buf, &pane, 1, SPLIT_HORIZONTAL);
}

void setUp(void) { ed = NULL; scr = NULL; buf = NULL; }
void tearDown(void) {
    if (ed) editor_free(ed);
    free(scr);
}

/* --- triggerAutocomplete ------------------------------------------------- */

void test_trigger_autocomplete_short_prefix(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 0;
    p->cursor_col = 1; /* only 1 char prefix 'h' */

    editor_trigger_autocomplete(ed);
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
}

void test_trigger_autocomplete_at_col_zero(void) {
    const wchar_t *lines[] = {L"hello world"};
    setup_editor(lines, 1);
    Pane *p = editor_active_pane(ed);
    p->cursor_col = 0;

    editor_trigger_autocomplete(ed);
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
}

void test_trigger_autocomplete_finds_suggestions(void) {
    const wchar_t *lines[] = {L"hello help hero", L"hel"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 3; /* typed "hel" */

    editor_trigger_autocomplete(ed);
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);
    TEST_ASSERT_TRUE(ac->active);
    TEST_ASSERT_TRUE(ac->suggestion_count > 0);
    TEST_ASSERT_EQUAL_INT(0, ac->selected_idx);
}

void test_trigger_autocomplete_no_suggestions(void) {
    const wchar_t *lines[] = {L"hello world", L"xyz"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 3; /* typed "xyz" - no matching words */

    editor_trigger_autocomplete(ed);
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
}

/* --- acceptAutocomplete -------------------------------------------------- */

void test_accept_autocomplete(void) {
    const wchar_t *lines[] = {L"hello help hero", L"hel"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 3;

    editor_trigger_autocomplete(ed);
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);

    /* Accept first suggestion */
    editor_accept_autocomplete(ed);

    /* After accept, autocomplete should be cleared */
    TEST_ASSERT_NULL(ed->input_state->autocomplete);

    /* Cursor should have moved past the completed word */
    TEST_ASSERT_TRUE(p->cursor_col > 3);

    /* The line should now contain the full word */
    TEST_ASSERT_TRUE(buf->line_lens[1] > 3);
}

void test_accept_autocomplete_nil(void) {
    const wchar_t *lines[] = {L"hello"};
    setup_editor(lines, 1);

    /* Should not crash */
    editor_accept_autocomplete(ed);
}

void test_accept_autocomplete_navigated(void) {
    const wchar_t *lines[] = {L"hello help hero", L"hel"};
    setup_editor(lines, 2);
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 1;
    p->cursor_col = 3;

    editor_trigger_autocomplete(ed);
    AutocompleteState *ac = ed->input_state->autocomplete;
    TEST_ASSERT_NOT_NULL(ac);

    /* Navigate to next suggestion */
    ac_next(ac);
    int sel_idx = ac->selected_idx;
    TEST_ASSERT_TRUE(sel_idx > 0);

    /* Accept the navigated suggestion */
    editor_accept_autocomplete(ed);
    TEST_ASSERT_NULL(ed->input_state->autocomplete);
    TEST_ASSERT_TRUE(p->cursor_col > 3);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_trigger_autocomplete_short_prefix);
    RUN_TEST(test_trigger_autocomplete_at_col_zero);
    RUN_TEST(test_trigger_autocomplete_finds_suggestions);
    RUN_TEST(test_trigger_autocomplete_no_suggestions);
    RUN_TEST(test_accept_autocomplete);
    RUN_TEST(test_accept_autocomplete_nil);
    RUN_TEST(test_accept_autocomplete_navigated);
    return UNITY_END();
}
