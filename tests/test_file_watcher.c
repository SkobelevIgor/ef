#include "unity.h"
#include "file_watcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static FileWatcher *fw;
static char tmpfile_path[256];

static void create_tmp_file(const char *content) {
    snprintf(tmpfile_path, sizeof(tmpfile_path),
             "/tmp/ef_test_fw_XXXXXX");
    int fd = mkstemp(tmpfile_path);
    if (fd < 0) return;
    write(fd, content, strlen(content));
    close(fd);
}

static void remove_tmp_file(void) {
    if (tmpfile_path[0]) unlink(tmpfile_path);
    tmpfile_path[0] = '\0';
}

void setUp(void) {
    fw = calloc(1, sizeof(FileWatcher));
    tmpfile_path[0] = '\0';
}

void tearDown(void) {
    for (int i = 0; i < fw->file_count; i++)
        free(fw->files[i].filename);
    free(fw);
    fw = NULL;
    remove_tmp_file();
}

/* --- fw_watch tests ------------------------------------------------------ */

void test_watch_adds_file(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    TEST_ASSERT_EQUAL_INT(1, fw->file_count);
    TEST_ASSERT_EQUAL_STRING(tmpfile_path, fw->files[0].filename);
    TEST_ASSERT_TRUE(fw->files[0].last_mod_time > 0);
}

void test_watch_duplicate_does_not_add(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    fw_watch(fw, tmpfile_path);
    TEST_ASSERT_EQUAL_INT(1, fw->file_count);
}

void test_watch_nonexistent_file(void) {
    fw_watch(fw, "/tmp/ef_no_such_file_999");
    TEST_ASSERT_EQUAL_INT(1, fw->file_count);
    TEST_ASSERT_EQUAL_INT(0, fw->files[0].last_mod_time);
}

void test_watch_null_filename(void) {
    fw_watch(fw, NULL);
    TEST_ASSERT_EQUAL_INT(0, fw->file_count);
}

void test_watch_empty_filename(void) {
    fw_watch(fw, "");
    TEST_ASSERT_EQUAL_INT(0, fw->file_count);
}

/* --- fw_find_file tests -------------------------------------------------- */

void test_find_file_found(void) {
    create_tmp_file("test");
    fw_watch(fw, tmpfile_path);
    TEST_ASSERT_EQUAL_INT(0, fw_find_file(fw, tmpfile_path));
}

void test_find_file_not_found(void) {
    TEST_ASSERT_EQUAL_INT(-1, fw_find_file(fw, "/tmp/missing"));
}

void test_find_file_null(void) {
    TEST_ASSERT_EQUAL_INT(-1, fw_find_file(fw, NULL));
}

/* --- fw_check tests ------------------------------------------------------ */

void test_check_no_change(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    const char *changed = fw_check(fw);
    TEST_ASSERT_NULL(changed);
}

void test_check_detects_external_change(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    /* Force mod time into the past */
    fw->files[0].last_mod_time -= 2;
    /* Rewrite file to get newer mod time */
    FILE *f = fopen(tmpfile_path, "w");
    fprintf(f, "changed");
    fclose(f);
    const char *changed = fw_check(fw);
    TEST_ASSERT_NOT_NULL(changed);
    TEST_ASSERT_EQUAL_STRING(tmpfile_path, changed);
}

void test_check_empty_watcher(void) {
    const char *changed = fw_check(fw);
    TEST_ASSERT_NULL(changed);
}

void test_check_deleted_file(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    remove_tmp_file();
    const char *changed = fw_check(fw);
    TEST_ASSERT_NULL(changed);
}

void test_check_detects_same_second_size_change(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    FILE *f = fopen(tmpfile_path, "w");
    fprintf(f, "hello world");
    fclose(f);
    const char *changed = fw_check(fw);
    TEST_ASSERT_NOT_NULL(changed);
}

void test_check_detects_same_second_same_size_change(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    FILE *f = fopen(tmpfile_path, "w");
    fprintf(f, "world");
    fclose(f);
    const char *changed = fw_check(fw);
    TEST_ASSERT_NOT_NULL(changed);
}

/* --- fw_update_mod_time tests -------------------------------------------- */

void test_update_mod_time_after_save(void) {
    create_tmp_file("hello");
    fw_watch(fw, tmpfile_path);
    time_t original = fw->files[0].last_mod_time;
    /* Simulate save: rewrite file */
    FILE *f = fopen(tmpfile_path, "w");
    fprintf(f, "saved");
    fclose(f);
    fw_update_mod_time(fw, tmpfile_path);
    TEST_ASSERT_TRUE(fw->files[0].last_mod_time >= original);
    /* After update, check should not report change */
    const char *changed = fw_check(fw);
    TEST_ASSERT_NULL(changed);
}

void test_update_mod_time_unknown_file(void) {
    /* Should not crash */
    fw_update_mod_time(fw, "/tmp/unknown");
    TEST_ASSERT_EQUAL_INT(0, fw->file_count);
}

void test_update_mod_time_null(void) {
    fw_update_mod_time(fw, NULL);
    TEST_ASSERT_EQUAL_INT(0, fw->file_count);
}

int main(void) {
    UNITY_BEGIN();
    /* watch */
    RUN_TEST(test_watch_adds_file);
    RUN_TEST(test_watch_duplicate_does_not_add);
    RUN_TEST(test_watch_nonexistent_file);
    RUN_TEST(test_watch_null_filename);
    RUN_TEST(test_watch_empty_filename);
    /* find */
    RUN_TEST(test_find_file_found);
    RUN_TEST(test_find_file_not_found);
    RUN_TEST(test_find_file_null);
    /* check */
    RUN_TEST(test_check_no_change);
    RUN_TEST(test_check_detects_external_change);
    RUN_TEST(test_check_empty_watcher);
    RUN_TEST(test_check_deleted_file);
    RUN_TEST(test_check_detects_same_second_size_change);
    RUN_TEST(test_check_detects_same_second_same_size_change);
    /* update */
    RUN_TEST(test_update_mod_time_after_save);
    RUN_TEST(test_update_mod_time_unknown_file);
    RUN_TEST(test_update_mod_time_null);
    return UNITY_END();
}
