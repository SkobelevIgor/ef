#include "unity.h"
#include "widget.h"
#include <stdlib.h>
#include <locale.h>

void setUp(void) {}
void tearDown(void) {}

/* --- WidgetKind/FocusTarget constants ------------------------------------ */

void test_widget_kind_constants(void) {
    TEST_ASSERT_EQUAL_INT(0, WIDGET_NONE);
    TEST_ASSERT_EQUAL_INT(1, WIDGET_SEARCH);
    TEST_ASSERT_EQUAL_INT(2, WIDGET_FIND_REPLACE);
}

void test_focus_target_constants(void) {
    TEST_ASSERT_EQUAL_INT(0, FOCUS_FIND_BAR);
    TEST_ASSERT_EQUAL_INT(1, FOCUS_REPLACE_BAR);
    TEST_ASSERT_EQUAL_INT(2, FOCUS_EDITOR);
}

/* --- WidgetSession ------------------------------------------------------- */

void test_new_widget_session(void) {
    WidgetSession *s = widget_session_new(5, 10);
    TEST_ASSERT_EQUAL_INT(0, s->query_len);
    TEST_ASSERT_EQUAL_INT(0, s->replace_len);
    TEST_ASSERT_EQUAL_INT(-1, s->current_index);
    TEST_ASSERT_FALSE(s->no_matches);
    TEST_ASSERT_EQUAL_INT(5, s->cursor_row);
    TEST_ASSERT_EQUAL_INT(10, s->cursor_col);
    TEST_ASSERT_EQUAL_INT(0, s->match_count);
    widget_session_free(s);
}

void test_new_widget_session_zero(void) {
    WidgetSession *s = widget_session_new(0, 0);
    TEST_ASSERT_EQUAL_INT(0, s->cursor_row);
    TEST_ASSERT_EQUAL_INT(0, s->cursor_col);
    widget_session_free(s);
}

void test_session_append_query(void) {
    WidgetSession *s = widget_session_new(0, 0);
    widget_session_append_query(s, L'h');
    widget_session_append_query(s, L'e');
    TEST_ASSERT_EQUAL_INT(2, s->query_len);
    TEST_ASSERT_TRUE(s->query[0] == L'h' && s->query[1] == L'e');
    widget_session_free(s);
}

void test_session_backspace_query(void) {
    WidgetSession *s = widget_session_new(0, 0);
    widget_session_append_query(s, L'h');
    widget_session_append_query(s, L'e');
    widget_session_backspace_query(s);
    TEST_ASSERT_EQUAL_INT(1, s->query_len);
    /* Backspace on empty = no-op */
    widget_session_backspace_query(s);
    widget_session_backspace_query(s);
    TEST_ASSERT_EQUAL_INT(0, s->query_len);
    widget_session_free(s);
}

void test_session_append_replace(void) {
    WidgetSession *s = widget_session_new(0, 0);
    widget_session_append_replace(s, L'a');
    widget_session_append_replace(s, L'b');
    TEST_ASSERT_EQUAL_INT(2, s->replace_len);
    widget_session_free(s);
}

void test_session_backspace_replace(void) {
    WidgetSession *s = widget_session_new(0, 0);
    widget_session_append_replace(s, L'a');
    widget_session_backspace_replace(s);
    TEST_ASSERT_EQUAL_INT(0, s->replace_len);
    widget_session_backspace_replace(s); /* no-op */
    TEST_ASSERT_EQUAL_INT(0, s->replace_len);
    widget_session_free(s);
}

/* --- WidgetState --------------------------------------------------------- */

void test_new_widget_state_search(void) {
    WidgetState *w = widget_state_new(WIDGET_SEARCH, 3, 7);
    TEST_ASSERT_TRUE(w->active);
    TEST_ASSERT_EQUAL_INT(WIDGET_SEARCH, w->kind);
    TEST_ASSERT_EQUAL_INT(FOCUS_FIND_BAR, w->focus);
    TEST_ASSERT_EQUAL_INT(3, w->anchor_row);
    TEST_ASSERT_EQUAL_INT(7, w->anchor_col);
    TEST_ASSERT_NOT_NULL(w->search_session);
    TEST_ASSERT_NULL(w->find_replace_session);
    widget_state_free(w);
}

void test_new_widget_state_find_replace(void) {
    WidgetState *w = widget_state_new(WIDGET_FIND_REPLACE, 1, 2);
    TEST_ASSERT_TRUE(w->active);
    TEST_ASSERT_EQUAL_INT(WIDGET_FIND_REPLACE, w->kind);
    TEST_ASSERT_NOT_NULL(w->find_replace_session);
    TEST_ASSERT_NULL(w->search_session);
    widget_state_free(w);
}

/* --- CurrentSession ------------------------------------------------------ */

void test_current_session_search(void) {
    WidgetState *w = widget_state_new(WIDGET_SEARCH, 0, 0);
    TEST_ASSERT_EQUAL_PTR(w->search_session, widget_current_session(w));
    widget_state_free(w);
}

void test_current_session_find_replace(void) {
    WidgetState *w = widget_state_new(WIDGET_FIND_REPLACE, 0, 0);
    TEST_ASSERT_EQUAL_PTR(w->find_replace_session, widget_current_session(w));
    widget_state_free(w);
}

void test_current_session_none(void) {
    WidgetState w = {.kind = WIDGET_NONE};
    TEST_ASSERT_NULL(widget_current_session(&w));
}

void test_current_session_nil(void) {
    TEST_ASSERT_NULL(widget_current_session(NULL));
}

/* --- CalculateBarRows ---------------------------------------------------- */

void test_bar_rows_empty(void) {
    TEST_ASSERT_EQUAL_INT(1, calculate_bar_rows(L"", 0, 80));
}

void test_bar_rows_short(void) {
    TEST_ASSERT_EQUAL_INT(1, calculate_bar_rows(L"hello", 5, 80));
}

void test_bar_rows_exact(void) {
    wchar_t text[21]; for (int i = 0; i < 20; i++) text[i] = L'x'; text[20] = L'\0';
    TEST_ASSERT_EQUAL_INT(1, calculate_bar_rows(text, 20, 20));
}

void test_bar_rows_overflow(void) {
    wchar_t text[22]; for (int i = 0; i < 21; i++) text[i] = L'x'; text[21] = L'\0';
    TEST_ASSERT_EQUAL_INT(2, calculate_bar_rows(text, 21, 20));
}

void test_bar_rows_very_long(void) {
    wchar_t text[61]; for (int i = 0; i < 60; i++) text[i] = L'x'; text[60] = L'\0';
    TEST_ASSERT_EQUAL_INT(3, calculate_bar_rows(text, 60, 20));
}

void test_bar_rows_zero_width(void) {
    TEST_ASSERT_EQUAL_INT(1, calculate_bar_rows(L"hello", 5, 0));
}

void test_bar_rows_negative_width(void) {
    TEST_ASSERT_EQUAL_INT(1, calculate_bar_rows(L"hello", 5, -5));
}

/* --- BarHeight ----------------------------------------------------------- */

void test_bar_height_search(void) {
    WidgetState *w = widget_state_new(WIDGET_SEARCH, 0, 0);
    TEST_ASSERT_EQUAL_INT(1, widget_bar_height(w, 80));
    widget_state_free(w);
}

void test_bar_height_find_replace(void) {
    WidgetState *w = widget_state_new(WIDGET_FIND_REPLACE, 0, 0);
    TEST_ASSERT_EQUAL_INT(2, widget_bar_height(w, 80));
    widget_state_free(w);
}

void test_bar_height_none(void) {
    WidgetState w = {.kind = WIDGET_NONE};
    TEST_ASSERT_EQUAL_INT(0, widget_bar_height(&w, 80));
}

void test_bar_height_null(void) {
    TEST_ASSERT_EQUAL_INT(0, widget_bar_height(NULL, 80));
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_widget_kind_constants);
    RUN_TEST(test_focus_target_constants);
    RUN_TEST(test_new_widget_session);
    RUN_TEST(test_new_widget_session_zero);
    RUN_TEST(test_session_append_query);
    RUN_TEST(test_session_backspace_query);
    RUN_TEST(test_session_append_replace);
    RUN_TEST(test_session_backspace_replace);
    RUN_TEST(test_new_widget_state_search);
    RUN_TEST(test_new_widget_state_find_replace);
    RUN_TEST(test_current_session_search);
    RUN_TEST(test_current_session_find_replace);
    RUN_TEST(test_current_session_none);
    RUN_TEST(test_current_session_nil);
    RUN_TEST(test_bar_rows_empty);
    RUN_TEST(test_bar_rows_short);
    RUN_TEST(test_bar_rows_exact);
    RUN_TEST(test_bar_rows_overflow);
    RUN_TEST(test_bar_rows_very_long);
    RUN_TEST(test_bar_rows_zero_width);
    RUN_TEST(test_bar_rows_negative_width);
    RUN_TEST(test_bar_height_search);
    RUN_TEST(test_bar_height_find_replace);
    RUN_TEST(test_bar_height_none);
    RUN_TEST(test_bar_height_null);
    return UNITY_END();
}
