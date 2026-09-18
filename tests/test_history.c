#include "unity.h"
#include "history.h"
#include "buffer.h"
#include <stdlib.h>
#include <locale.h>

static History *hist;
static Buffer *buf;

void setUp(void) {
    hist = history_new(100);
    buf = buffer_new();
}

void tearDown(void) {
    history_free(hist);
    buffer_free(buf);
}

void test_new_history_empty(void) {
    TEST_ASSERT_FALSE(history_can_undo(hist));
    TEST_ASSERT_FALSE(history_can_redo(hist));
}

void test_push_and_undo(void) {
    Change *c = change_new(CHANGE_INSERT, buf, 0, 0);
    history_push(hist, c);
    TEST_ASSERT_TRUE(history_can_undo(hist));
    Change *undone = history_undo(hist);
    TEST_ASSERT_NOT_NULL(undone);
    TEST_ASSERT_EQUAL_INT(CHANGE_INSERT, undone->type);
    TEST_ASSERT_FALSE(history_can_undo(hist));
    TEST_ASSERT_TRUE(history_can_redo(hist));
}

void test_push_and_redo(void) {
    Change *c = change_new(CHANGE_DELETE, buf, 1, 2);
    history_push(hist, c);
    history_undo(hist);
    Change *redone = history_redo(hist);
    TEST_ASSERT_NOT_NULL(redone);
    TEST_ASSERT_EQUAL_INT(CHANGE_DELETE, redone->type);
    TEST_ASSERT_EQUAL_INT(1, redone->row);
    TEST_ASSERT_EQUAL_INT(2, redone->col);
}

void test_push_clears_redo(void) {
    Change *c1 = change_new(CHANGE_INSERT, buf, 0, 0);
    history_push(hist, c1);
    history_undo(hist);
    TEST_ASSERT_TRUE(history_can_redo(hist));

    Change *c2 = change_new(CHANGE_INSERT, buf, 1, 0);
    history_push(hist, c2);
    TEST_ASSERT_FALSE(history_can_redo(hist));
}

void test_push_nil_ignored(void) {
    history_push(hist, NULL);
    TEST_ASSERT_FALSE(history_can_undo(hist));
}

void test_undo_empty_returns_null(void) {
    TEST_ASSERT_NULL(history_undo(hist));
}

void test_redo_empty_returns_null(void) {
    TEST_ASSERT_NULL(history_redo(hist));
}

void test_max_size_trims(void) {
    history_free(hist);
    hist = history_new(3);
    for (int i = 0; i < 5; i++) {
        Change *c = change_new(CHANGE_INSERT, buf, i, 0);
        history_push(hist, c);
    }
    /* Should only have 3 items */
    int count = 0;
    while (history_undo(hist)) count++;
    TEST_ASSERT_EQUAL_INT(3, count);
}

void test_session_commit(void) {
    /* Add a char so buffer is different from snapshot */
    history_start_session(hist, buf, 0, 0);
    buffer_insert_char(buf, 0, 0, L'a');
    history_commit_session(hist, buf->lines, buf->line_lens, buf->line_count);
    TEST_ASSERT_TRUE(history_can_undo(hist));
}

void test_session_commit_no_change(void) {
    history_start_session(hist, buf, 0, 0);
    /* Don't change anything */
    history_commit_session(hist, buf->lines, buf->line_lens, buf->line_count);
    TEST_ASSERT_FALSE(history_can_undo(hist));
}

void test_session_commit_respects_max_size(void) {
    history_free(hist);
    hist = history_new(3);
    for (int i = 0; i < 20; i++) {
        history_start_session(hist, buf, 0, 0);
        buffer_insert_char(buf, 0, 0, L'a');
        history_commit_session(hist, buf->lines, buf->line_lens,
                               buf->line_count);
    }
    TEST_ASSERT_EQUAL_INT(3, hist->undo_count);
}

void test_record_insert(void) {
    wchar_t *text = malloc(sizeof(wchar_t) * 4);
    wmemcpy(text, L"abc", 3); text[3] = L'\0';
    int len = 3;
    history_record_insert(hist, buf, 0, 0, &text, &len, 1);
    free(text);
    TEST_ASSERT_TRUE(history_can_undo(hist));
    Change *c = history_undo(hist);
    TEST_ASSERT_EQUAL_INT(CHANGE_INSERT, c->type);
    TEST_ASSERT_EQUAL_INT(1, c->text_count);
    TEST_ASSERT_EQUAL_INT(3, c->text_lens[0]);
}

void test_record_delete_lines(void) {
    wchar_t *text = malloc(sizeof(wchar_t) * 4);
    wmemcpy(text, L"abc", 3); text[3] = L'\0';
    int len = 3;
    history_record_delete_lines(hist, buf, 0, &text, &len, 1);
    free(text);
    Change *c = history_undo(hist);
    TEST_ASSERT_TRUE(c->line_deletion);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_new_history_empty);
    RUN_TEST(test_push_and_undo);
    RUN_TEST(test_push_and_redo);
    RUN_TEST(test_push_clears_redo);
    RUN_TEST(test_push_nil_ignored);
    RUN_TEST(test_undo_empty_returns_null);
    RUN_TEST(test_redo_empty_returns_null);
    RUN_TEST(test_max_size_trims);
    RUN_TEST(test_session_commit);
    RUN_TEST(test_session_commit_no_change);
    RUN_TEST(test_session_commit_respects_max_size);
    RUN_TEST(test_record_insert);
    RUN_TEST(test_record_delete_lines);
    return UNITY_END();
}
