#include "unity.h"
#include "test_helpers.h"
#include "file_watcher.h"
#include "widget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

static Editor *ed;
static char tmppath[256];

static void create_file(const char *content) {
    snprintf(tmppath, sizeof(tmppath),
             "/tmp/ef_test_fwi_XXXXXX");
    int fd = mkstemp(tmppath);
    write(fd, content, strlen(content));
    close(fd);
}

static void setup_editor_with_file(const char *content) {
    create_file(content);
    Buffer *buf = buffer_new_from_file(tmppath);
    Pane *p = pane_new(buf);
    ed = editor_new_with_deps(&test_mock_screen, &buf, &p, 1,
                              SPLIT_HORIZONTAL);
}

void setUp(void) {
    setlocale(LC_ALL, "");
    ed = NULL;
    tmppath[0] = '\0';
}

void tearDown(void) {
    if (ed) { editor_free(ed); ed = NULL; }
    if (tmppath[0]) { unlink(tmppath); tmppath[0] = '\0'; }
}

/* --- editor_handle_file_change tests ------------------------------------ */

void test_file_change_reloads_buffer(void) {
    setup_editor_with_file("hello\n");
    Buffer *buf = editor_active_buffer(ed);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);

    /* Write different content externally */
    FILE *f = fopen(tmppath, "w");
    fprintf(f, "line1\nline2\nline3\n");
    fclose(f);

    editor_handle_file_change(ed, tmppath);

    TEST_ASSERT_EQUAL_INT(3, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"line1", 5));
}

void test_file_change_clamps_cursor(void) {
    setup_editor_with_file("a\nb\nc\nd\ne\n");
    Pane *p = editor_active_pane(ed);
    p->cursor_row = 4; /* on line "e" */

    /* Shrink the file */
    FILE *f = fopen(tmppath, "w");
    fprintf(f, "only\n");
    fclose(f);

    editor_handle_file_change(ed, tmppath);

    TEST_ASSERT_TRUE(p->cursor_row < editor_active_buffer(ed)->line_count);
}

void test_file_change_records_undo(void) {
    setup_editor_with_file("original\n");

    FILE *f = fopen(tmppath, "w");
    fprintf(f, "changed\n");
    fclose(f);

    editor_handle_file_change(ed, tmppath);

    /* Undo should restore original content */
    editor_undo(ed);
    Buffer *buf = editor_active_buffer(ed);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"original", 8));
}

void test_file_change_unknown_file_noop(void) {
    setup_editor_with_file("hello\n");
    Buffer *buf = editor_active_buffer(ed);
    int orig_count = buf->line_count;

    editor_handle_file_change(ed, "/tmp/no_such_file");

    TEST_ASSERT_EQUAL_INT(orig_count, buf->line_count);
}

void test_file_change_null_filename(void) {
    setup_editor_with_file("hello\n");
    /* Should not crash */
    editor_handle_file_change(ed, NULL);
}

void test_file_change_refreshes_widget_matches(void) {
    setup_editor_with_file("a\nb\nc\nhello\n");
    editor_open_find_replace_widget(ed);
    WidgetSession *s = editor_active_pane(ed)->widget->find_replace_session;
    for (const wchar_t *c = L"hello"; *c; c++) {
        EditorEvent ev = {EV_KEY, (int)*c, *c, true, false};
        editor_handle_widget_mode(ed, &ev);
    }
    TEST_ASSERT_EQUAL_INT(3, s->matches[0].row);

    FILE *f = fopen(tmppath, "w");
    fprintf(f, "hello\n");
    fclose(f);

    editor_handle_file_change(ed, tmppath);

    TEST_ASSERT_EQUAL_INT(1, s->match_count);
    TEST_ASSERT_EQUAL_INT(0, s->matches[0].row);
    TEST_ASSERT_EQUAL_INT(0, s->current_index);
}

/* --- editor_check_file_changes tests ------------------------------------ */

void test_check_no_change_is_noop(void) {
    setup_editor_with_file("hello\n");
    Buffer *buf = editor_active_buffer(ed);
    int orig_count = buf->line_count;

    editor_check_file_changes(ed);

    TEST_ASSERT_EQUAL_INT(orig_count, buf->line_count);
}

void test_check_detects_external_modification(void) {
    setup_editor_with_file("hello\n");

    /* Force watcher to think file is older */
    FileWatcher *fw = (FileWatcher *)ed->watcher->impl;
    if (fw->file_count > 0)
        fw->files[0].last_mod_time -= 2;

    FILE *f = fopen(tmppath, "w");
    fprintf(f, "modified\n");
    fclose(f);

    editor_check_file_changes(ed);

    Buffer *buf = editor_active_buffer(ed);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"modified", 8));
}

void test_save_updates_watcher_prevents_false_change(void) {
    setup_editor_with_file("hello\n");
    Buffer *buf = editor_active_buffer(ed);

    /* Modify and save */
    buffer_insert_char(buf, 0, 5, L'!');
    editor_save_all_modified(ed);

    /* Check should NOT detect this as external change */
    editor_check_file_changes(ed);

    /* Buffer should still have the saved content */
    TEST_ASSERT_EQUAL_INT(6, buf->line_lens[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_file_change_reloads_buffer);
    RUN_TEST(test_file_change_clamps_cursor);
    RUN_TEST(test_file_change_records_undo);
    RUN_TEST(test_file_change_unknown_file_noop);
    RUN_TEST(test_file_change_null_filename);
    RUN_TEST(test_file_change_refreshes_widget_matches);
    RUN_TEST(test_check_no_change_is_noop);
    RUN_TEST(test_check_detects_external_modification);
    RUN_TEST(test_save_updates_watcher_prevents_false_change);
    return UNITY_END();
}
