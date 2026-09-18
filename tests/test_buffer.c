#include "unity.h"
#include "buffer.h"
#include "runes.h"

#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

static Buffer *buf;

void setUp(void) {
    buf = buffer_new();
}

void tearDown(void) {
    buffer_free(buf);
    buf = NULL;
}

/* --- New buffer tests ---------------------------------------------------- */

void test_new_buffer_has_one_empty_line(void) {
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, buf->line_lens[0]);
}

void test_new_buffer_not_modified(void) {
    TEST_ASSERT_FALSE(buf->modified);
}

/* --- InsertChar tests ---------------------------------------------------- */

void test_insert_char_at_start(void) {
    int col = buffer_insert_char(buf, 0, 0, L'a');
    TEST_ASSERT_EQUAL_INT(1, col);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'a', buf->lines[0][0]);
    TEST_ASSERT_TRUE(buf->modified);
}

void test_insert_multiple_chars(void) {
    buffer_insert_char(buf, 0, 0, L'h');
    buffer_insert_char(buf, 0, 1, L'i');
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"hi", 2));
}

void test_insert_char_unicode(void) {
    buffer_insert_char(buf, 0, 0, L'\u00e9'); /* e-acute */
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\u00e9', buf->lines[0][0]);
}

/* --- DeleteChar (backspace) tests ---------------------------------------- */

void test_delete_char_in_middle(void) {
    buffer_insert_char(buf, 0, 0, L'a');
    buffer_insert_char(buf, 0, 1, L'b');
    buffer_insert_char(buf, 0, 2, L'c');
    int nr, nc;
    buffer_delete_char(buf, 0, 2, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(0, nr);
    TEST_ASSERT_EQUAL_INT(1, nc);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"ac", 2));
}

void test_delete_char_at_line_start_joins(void) {
    /* Setup two lines: "hello" and "world" */
    buffer_free(buf);
    buf = buffer_new();
    wchar_t *line1 = malloc(sizeof(wchar_t) * 6);
    wmemcpy(line1, L"hello", 5); line1[5] = L'\0';
    buffer_set_line(buf, 0, line1, 5);
    wchar_t *line2 = malloc(sizeof(wchar_t) * 6);
    wmemcpy(line2, L"world", 5); line2[5] = L'\0';
    buffer_insert_line_after(buf, 0, line2, 5);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);

    int nr, nc;
    buffer_delete_char(buf, 1, 0, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(0, nr);
    TEST_ASSERT_EQUAL_INT(5, nc);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(10, buf->line_lens[0]);
}

void test_delete_char_at_start_of_first_line_noop(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    int nr, nc;
    buffer_delete_char(buf, 0, 0, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(0, nr);
    TEST_ASSERT_EQUAL_INT(0, nc);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
}

/* --- DeleteCharForward tests --------------------------------------------- */

void test_delete_char_forward(void) {
    buffer_insert_char(buf, 0, 0, L'a');
    buffer_insert_char(buf, 0, 1, L'b');
    buffer_delete_char_forward(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'b', buf->lines[0][0]);
}

void test_delete_char_forward_at_end_joins(void) {
    wchar_t *l1 = malloc(sizeof(wchar_t) * 3);
    wmemcpy(l1, L"ab", 2); l1[2] = L'\0';
    buffer_set_line(buf, 0, l1, 2);
    wchar_t *l2 = malloc(sizeof(wchar_t) * 3);
    wmemcpy(l2, L"cd", 2); l2[2] = L'\0';
    buffer_insert_line_after(buf, 0, l2, 2);

    buffer_delete_char_forward(buf, 0, 2);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(4, buf->line_lens[0]);
}

/* --- InsertNewline tests ------------------------------------------------- */

void test_insert_newline_splits_line(void) {
    wchar_t *line = malloc(sizeof(wchar_t) * 6);
    wmemcpy(line, L"hello", 5); line[5] = L'\0';
    buffer_set_line(buf, 0, line, 5);

    int nr, nc;
    buffer_insert_newline(buf, 0, 2, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(0, nc);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[1]);
}

/* --- DeleteLine tests ---------------------------------------------------- */

void test_delete_line_single_line_empties(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    int dl;
    wchar_t *deleted = buffer_delete_line(buf, 0, &dl);
    TEST_ASSERT_EQUAL_INT(1, dl);
    TEST_ASSERT_EQUAL_INT(L'x', deleted[0]);
    free(deleted);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, buf->line_lens[0]);
}

void test_delete_line_out_of_bounds(void) {
    int dl;
    wchar_t *deleted = buffer_delete_line(buf, 5, &dl);
    TEST_ASSERT_NULL(deleted);
    TEST_ASSERT_EQUAL_INT(0, dl);
}

void test_delete_line_removes_line(void) {
    wchar_t *l1 = malloc(sizeof(wchar_t) * 2);
    l1[0] = L'a'; l1[1] = L'\0';
    buffer_set_line(buf, 0, l1, 1);
    wchar_t *l2 = malloc(sizeof(wchar_t) * 2);
    l2[0] = L'b'; l2[1] = L'\0';
    buffer_insert_line_after(buf, 0, l2, 1);
    wchar_t *l3 = malloc(sizeof(wchar_t) * 2);
    l3[0] = L'c'; l3[1] = L'\0';
    buffer_insert_line_after(buf, 1, l3, 1);

    int dl;
    wchar_t *deleted = buffer_delete_line(buf, 1, &dl);
    TEST_ASSERT_EQUAL_INT(1, dl);
    TEST_ASSERT_EQUAL_INT(L'b', deleted[0]);
    free(deleted);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(L'a', buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(L'c', buf->lines[1][0]);
}

/* --- CopyLine tests ------------------------------------------------------ */

void test_copy_line(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    int cl;
    wchar_t *copied = buffer_copy_line(buf, 0, &cl);
    TEST_ASSERT_EQUAL_INT(1, cl);
    TEST_ASSERT_EQUAL_INT(L'x', copied[0]);
    free(copied);
    /* Original unchanged */
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
}

void test_copy_line_out_of_bounds(void) {
    int cl;
    wchar_t *copied = buffer_copy_line(buf, -1, &cl);
    TEST_ASSERT_NULL(copied);
}

/* --- InsertLineAfter / InsertLineBefore tests ----------------------------- */

void test_insert_line_after(void) {
    wchar_t *nl = malloc(sizeof(wchar_t) * 4);
    wmemcpy(nl, L"new", 3); nl[3] = L'\0';
    buffer_insert_line_after(buf, 0, nl, 3);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[1]);
}

void test_insert_line_before(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    wchar_t *nl = malloc(sizeof(wchar_t) * 4);
    wmemcpy(nl, L"new", 3); nl[3] = L'\0';
    buffer_insert_line_before(buf, 0, nl, 3);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[1]);
}

/* --- Range tests --------------------------------------------------------- */

void test_delete_range_negative_end_is_safe(void) {
    wchar_t *line = malloc(sizeof(wchar_t) * 6);
    wmemcpy(line, L"hello", 5); line[5] = L'\0';
    buffer_set_line(buf, 0, line, 5);

    int *dlens, dcount;
    wchar_t **deleted = buffer_delete_range(buf, -1, -1, -1, -1,
                                            &dlens, &dcount);
    TEST_ASSERT_EQUAL_INT(1, dcount);
    TEST_ASSERT_EQUAL_INT(0, dlens[0]);
    buffer_free_lines(deleted, dlens, dcount);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

void test_delete_range_start_beyond_end_is_safe(void) {
    wchar_t *line = malloc(sizeof(wchar_t) * 6);
    wmemcpy(line, L"hello", 5); line[5] = L'\0';
    buffer_set_line(buf, 0, line, 5);

    int *dlens, dcount;
    wchar_t **deleted = buffer_delete_range(buf, 3, 0, 5, 0,
                                            &dlens, &dcount);
    TEST_ASSERT_EQUAL_INT(1, dcount);
    TEST_ASSERT_EQUAL_INT(0, dlens[0]);
    buffer_free_lines(deleted, dlens, dcount);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
}

/* --- File I/O tests ------------------------------------------------------ */

void test_save_and_load(void) {
    const char *tmp = "/tmp/ef_test_buffer.txt";
    buffer_free(buf);
    buf = buffer_new();
    buf->filename = strdup(tmp);

    buffer_insert_char(buf, 0, 0, L'h');
    buffer_insert_char(buf, 0, 1, L'i');
    TEST_ASSERT_EQUAL_INT(0, buffer_save(buf));
    TEST_ASSERT_FALSE(buf->modified);

    /* Load into fresh buffer */
    Buffer *buf2 = buffer_new();
    buf2->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf2));
    TEST_ASSERT_EQUAL_INT(1, buf2->line_count);
    TEST_ASSERT_EQUAL_INT(2, buf2->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf2->lines[0], L"hi", 2));
    buffer_free(buf2);
    remove(tmp);
}

static void write_file(const char *path, const char *data, size_t len) {
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_INT(len, fwrite(data, 1, len, f));
    fclose(f);
}

void test_load_long_line_not_split(void) {
    const char *tmp = "/tmp/ef_test_long_line.txt";
    char *data = malloc(10001);
    memset(data, 'x', 10000);
    data[10000] = '\n';
    write_file(tmp, data, 10001);
    free(data);

    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(10000, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'x', buf->lines[0][9999]);
    remove(tmp);
}

void test_load_failure_keeps_buffer_intact(void) {
    buffer_insert_char(buf, 0, 0, L'k');
    buf->filename = strdup("/tmp");
    TEST_ASSERT_EQUAL_INT(-1, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'k', buf->lines[0][0]);
}

static void assert_file_equals(const char *path, const char *expected) {
    char got[64] = {0};
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    size_t n = fread(got, 1, sizeof(got) - 1, f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(strlen(expected), n);
    TEST_ASSERT_EQUAL_MEMORY(expected, got, n);
}

void test_new_buffer_saves_trailing_newline(void) {
    const char *tmp = "/tmp/ef_test_trailing_new.txt";
    TEST_ASSERT_TRUE(buf->trailing_newline);
    buf->filename = strdup(tmp);
    buffer_insert_char(buf, 0, 0, L'a');
    TEST_ASSERT_EQUAL_INT(0, buffer_save(buf));
    assert_file_equals(tmp, "a\n");
    remove(tmp);
}

void test_load_preserves_missing_trailing_newline(void) {
    const char *tmp = "/tmp/ef_test_no_trailing.txt";
    write_file(tmp, "ab\ncd", 5);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_FALSE(buf->trailing_newline);
    TEST_ASSERT_EQUAL_INT(0, buffer_save(buf));
    assert_file_equals(tmp, "ab\ncd");
    remove(tmp);
}

void test_load_newline_only_file(void) {
    const char *tmp = "/tmp/ef_test_newline_only.txt";
    write_file(tmp, "\n", 1);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, buf->line_lens[0]);
    TEST_ASSERT_TRUE(buf->trailing_newline);
    TEST_ASSERT_EQUAL_INT(0, buffer_save(buf));
    assert_file_equals(tmp, "\n");
    remove(tmp);
}

void test_load_empty_file(void) {
    const char *tmp = "/tmp/ef_test_empty.txt";
    write_file(tmp, "", 0);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(0, buf->line_lens[0]);
    TEST_ASSERT_TRUE(buf->trailing_newline);
    remove(tmp);
}

void test_save_long_line_not_truncated(void) {
    const char *tmp = "/tmp/ef_test_save_long.txt";
    wchar_t *line = malloc(sizeof(wchar_t) * 10001);
    wmemset(line, L'x', 10000);
    line[10000] = L'\0';
    buffer_set_line(buf, 0, line, 10000);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_save(buf));

    struct stat st;
    TEST_ASSERT_EQUAL_INT(0, stat(tmp, &st));
    TEST_ASSERT_EQUAL_INT(10001, (int)st.st_size);
    remove(tmp);
}

void test_save_unencodable_char_fails(void) {
    const char *tmp = "/tmp/ef_test_save_bad.txt";
    write_file(tmp, "keep\n", 5);
    buffer_insert_char(buf, 0, 0, (wchar_t)0xD800);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(-1, buffer_save(buf));
    TEST_ASSERT_TRUE(buf->modified);
    assert_file_equals(tmp, "keep\n");
    remove(tmp);
}

void test_load_cjk_line(void) {
    const char *tmp = "/tmp/ef_test_cjk.txt";
    write_file(tmp, "\xe6\x97\xa5\xe6\x9c\xac\n", 7);
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(0, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0x65E5, buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(0x672C, buf->lines[0][1]);
    remove(tmp);
}

void test_load_invalid_utf8_fails(void) {
    const char *tmp = "/tmp/ef_test_invalid_utf8.txt";
    write_file(tmp, "a\xffz\n", 4);
    buffer_insert_char(buf, 0, 0, L'k');
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(-1, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(L'k', buf->lines[0][0]);
    remove(tmp);
}

void test_load_embedded_nul_fails(void) {
    const char *tmp = "/tmp/ef_test_nul.txt";
    write_file(tmp, "ab\0cd\n", 6);
    buffer_insert_char(buf, 0, 0, L'k');
    buf->filename = strdup(tmp);
    TEST_ASSERT_EQUAL_INT(-1, buffer_load(buf));
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(L'k', buf->lines[0][0]);
    remove(tmp);
}

/* --- NewlineWithIndent tests --------------------------------------------- */

void test_newline_with_indent_at_end_of_indented_line(void) {
    /* Line: "\thello", cursor at end (col 6) — new line should get "\t" */
    wchar_t *l = malloc(sizeof(wchar_t) * 7);
    wmemcpy(l, L"\thello", 6); l[6] = L'\0';
    buffer_set_line(buf, 0, l, 6);
    buf->config.auto_indentation = true;

    int nr, nc;
    buffer_insert_newline_with_indent(buf, 0, 6, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(1, nc);  /* cursor at indent_len */
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[1][0]);
}

void test_newline_with_indent_cursor_within_indentation(void) {
    /* Line: "\t\t" (2 tabs), cursor at col 1 (between tabs).
       Bug: right part "\t" gets full indent "\t\t" prepended → 3 tabs.
       Expected: new line should have "\t\t" (same indent), not "\t\t\t". */
    wchar_t *l = malloc(sizeof(wchar_t) * 3);
    wmemcpy(l, L"\t\t", 2); l[2] = L'\0';
    buffer_set_line(buf, 0, l, 2);
    buf->config.auto_indentation = true;

    int nr, nc;
    buffer_insert_newline_with_indent(buf, 0, 1, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(2, nc);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[1][0]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[1][1]);
}

void test_newline_with_indent_cursor_at_start_of_indented_line(void) {
    /* Line: "    hello" (4 spaces + text), cursor at col 0.
       Right part is "    hello". Should become "    hello" (indent + content),
       not "        hello" (double indent). */
    wchar_t *l = malloc(sizeof(wchar_t) * 10);
    wmemcpy(l, L"    hello", 9); l[9] = L'\0';
    buffer_set_line(buf, 0, l, 9);
    buf->config.auto_indentation = true;

    int nr, nc;
    buffer_insert_newline_with_indent(buf, 0, 0, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(4, nc);
    TEST_ASSERT_EQUAL_INT(0, buf->line_lens[0]); /* left part is empty */
    TEST_ASSERT_EQUAL_INT(9, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"    hello", 9));
}

void test_newline_with_indent_repeated_enter_no_accumulation(void) {
    /* Simulate: indented line, Tab, Enter, Enter — indent must not grow.
       Line: "\t" (1 tab), cursor at col 1 (end). First Enter should produce
       a new line with "\t". Second Enter on that new line should also produce "\t",
       not "\t\t". */
    wchar_t *l = malloc(sizeof(wchar_t) * 2);
    wmemcpy(l, L"\t", 1); l[1] = L'\0';
    buffer_set_line(buf, 0, l, 1);
    buf->config.auto_indentation = true;

    int nr, nc;
    buffer_insert_newline_with_indent(buf, 0, 1, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(1, nc);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[1]);

    /* Second Enter on the new line (row 1, cursor at indent end col 1) */
    int nr2, nc2;
    buffer_insert_newline_with_indent(buf, 1, 1, &nr2, &nc2);
    TEST_ASSERT_EQUAL_INT(2, nr2);
    TEST_ASSERT_EQUAL_INT(1, nc2);
    TEST_ASSERT_EQUAL_INT(1, buf->line_lens[2]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[2][0]);
}

void test_newline_with_indent_spaces_cursor_mid_indent(void) {
    /* Line: "        " (8 spaces), cursor at col 4 (middle of indent).
       Right part is "    " (4 spaces). Should become "        " (8 spaces),
       not "            " (12 spaces). */
    wchar_t *l = malloc(sizeof(wchar_t) * 9);
    wmemcpy(l, L"        ", 8); l[8] = L'\0';
    buffer_set_line(buf, 0, l, 8);
    buf->config.auto_indentation = true;

    int nr, nc;
    buffer_insert_newline_with_indent(buf, 0, 4, &nr, &nc);
    TEST_ASSERT_EQUAL_INT(1, nr);
    TEST_ASSERT_EQUAL_INT(8, nc);
    TEST_ASSERT_EQUAL_INT(8, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"        ", 8));
}

/* --- IndentRange tests --------------------------------------------------- */

void test_indent_range_with_tab(void) {
    wchar_t *l = malloc(sizeof(wchar_t) * 4);
    wmemcpy(l, L"abc", 3); l[3] = L'\0';
    buffer_set_line(buf, 0, l, 3);
    buf->config.expand_tab = false;

    buffer_indent_range(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(4, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[0][0]);
}

void test_indent_range_with_spaces(void) {
    wchar_t *l = malloc(sizeof(wchar_t) * 4);
    wmemcpy(l, L"abc", 3); l[3] = L'\0';
    buffer_set_line(buf, 0, l, 3);
    buf->config.expand_tab = true;
    buf->config.shift_width = 2;

    buffer_indent_range(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(5, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L' ', buf->lines[0][0]);
    TEST_ASSERT_EQUAL_INT(L' ', buf->lines[0][1]);
    TEST_ASSERT_EQUAL_INT(L'a', buf->lines[0][2]);
}

/* --- UnindentRange tests ------------------------------------------------- */

void test_unindent_tab(void) {
    wchar_t *l = malloc(sizeof(wchar_t) * 5);
    wmemcpy(l, L"\tabc", 4); l[4] = L'\0';
    buffer_set_line(buf, 0, l, 4);

    buffer_unindent_range(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(L'a', buf->lines[0][0]);
}

/* --- GetShiftWidth tests ------------------------------------------------- */

void test_get_shift_width_default(void) {
    buf->config.shift_width = 0;
    TEST_ASSERT_EQUAL_INT(DEFAULT_TAB_STOP, buffer_get_shift_width(buf));
}

void test_get_shift_width_custom(void) {
    buf->config.shift_width = 2;
    TEST_ASSERT_EQUAL_INT(2, buffer_get_shift_width(buf));
}

/* --- ModCount tests ------------------------------------------------------ */

void test_mod_count_increments(void) {
    uint64_t initial = buf->mod_count;
    buffer_insert_char(buf, 0, 0, L'a');
    TEST_ASSERT_EQUAL_UINT64(initial + 1, buf->mod_count);
    buffer_insert_char(buf, 0, 1, L'b');
    TEST_ASSERT_EQUAL_UINT64(initial + 2, buf->mod_count);
}

/* --- InsertTab tests ----------------------------------------------------- */

void test_insert_tab_literal(void) {
    buf->config.expand_tab = false;
    int col = buffer_insert_tab(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(1, col);
    TEST_ASSERT_EQUAL_INT(L'\t', buf->lines[0][0]);
}

void test_insert_tab_expand(void) {
    buf->config.expand_tab = true;
    buf->config.tab_stop = 4;
    int col = buffer_insert_tab(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(4, col);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(L' ', buf->lines[0][i]);
    }
}

/* --- DeleteCharAt tests -------------------------------------------------- */

void test_delete_char_at(void) {
    buffer_insert_char(buf, 0, 0, L'a');
    buffer_insert_char(buf, 0, 1, L'b');
    buffer_insert_char(buf, 0, 2, L'c');
    wchar_t del = buffer_delete_char_at(buf, 0, 1);
    TEST_ASSERT_EQUAL_INT(L'b', del);
    TEST_ASSERT_EQUAL_INT(2, buf->line_lens[0]);
}

void test_delete_char_at_out_of_bounds(void) {
    wchar_t del = buffer_delete_char_at(buf, 0, 5);
    TEST_ASSERT_EQUAL_INT(0, del);
}

/* --- buffer_replace_all tests -------------------------------------------- */

void test_replace_all_replaces_content(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    /* Prepare source lines */
    wchar_t *src[2];
    int src_lens[2];
    src[0] = malloc(sizeof(wchar_t) * 4);
    wmemcpy(src[0], L"abc", 3); src[0][3] = L'\0'; src_lens[0] = 3;
    src[1] = malloc(sizeof(wchar_t) * 4);
    wmemcpy(src[1], L"def", 3); src[1][3] = L'\0'; src_lens[1] = 3;

    buffer_replace_all(buf, src, src_lens, 2);
    TEST_ASSERT_EQUAL_INT(2, buf->line_count);
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[0]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[0], L"abc", 3));
    TEST_ASSERT_EQUAL_INT(3, buf->line_lens[1]);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(buf->lines[1], L"def", 3));
    /* src entries are owned by buffer now, don't free */
}

void test_replace_all_single_line(void) {
    wchar_t *src[1];
    int src_lens[1];
    src[0] = malloc(sizeof(wchar_t) * 5);
    wmemcpy(src[0], L"test", 4); src[0][4] = L'\0'; src_lens[0] = 4;

    buffer_replace_all(buf, src, src_lens, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->line_count);
    TEST_ASSERT_EQUAL_INT(4, buf->line_lens[0]);
}

void test_replace_all_empty_source(void) {
    buffer_insert_char(buf, 0, 0, L'x');
    /* Replace with zero lines - should result in 0 line_count */
    buffer_replace_all(buf, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, buf->line_count);
}

/* --- buffer_check_writable tests ---------------------------------------- */

void test_check_writable_null_filename_fails(void) {
    TEST_ASSERT_EQUAL_INT(-1, buffer_check_writable(NULL));
}

void test_check_writable_nonexistent_file_ok(void) {
    TEST_ASSERT_EQUAL_INT(0, buffer_check_writable("/tmp/ef_no_such_file_xyz123.txt"));
}

void test_check_writable_writable_file_ok(void) {
    const char *tmp = "/tmp/ef_test_writable.txt";
    FILE *f = fopen(tmp, "w");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);
    chmod(tmp, 0644);

    TEST_ASSERT_EQUAL_INT(0, buffer_check_writable(tmp));
    remove(tmp);
}

void test_check_writable_readonly_file_fails(void) {
    if (getuid() == 0) {
        TEST_IGNORE_MESSAGE("skipped: root bypasses file permission checks");
        return;
    }
    const char *tmp = "/tmp/ef_test_readonly.txt";
    FILE *f = fopen(tmp, "w");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);
    chmod(tmp, 0444);

    TEST_ASSERT_EQUAL_INT(-1, buffer_check_writable(tmp));
    chmod(tmp, 0644);
    remove(tmp);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    RUN_TEST(test_new_buffer_has_one_empty_line);
    RUN_TEST(test_new_buffer_not_modified);
    RUN_TEST(test_insert_char_at_start);
    RUN_TEST(test_insert_multiple_chars);
    RUN_TEST(test_insert_char_unicode);
    RUN_TEST(test_delete_char_in_middle);
    RUN_TEST(test_delete_char_at_line_start_joins);
    RUN_TEST(test_delete_char_at_start_of_first_line_noop);
    RUN_TEST(test_delete_char_forward);
    RUN_TEST(test_delete_char_forward_at_end_joins);
    RUN_TEST(test_insert_newline_splits_line);
    RUN_TEST(test_newline_with_indent_at_end_of_indented_line);
    RUN_TEST(test_newline_with_indent_cursor_within_indentation);
    RUN_TEST(test_newline_with_indent_cursor_at_start_of_indented_line);
    RUN_TEST(test_newline_with_indent_repeated_enter_no_accumulation);
    RUN_TEST(test_newline_with_indent_spaces_cursor_mid_indent);
    RUN_TEST(test_delete_line_single_line_empties);
    RUN_TEST(test_delete_line_out_of_bounds);
    RUN_TEST(test_delete_line_removes_line);
    RUN_TEST(test_copy_line);
    RUN_TEST(test_copy_line_out_of_bounds);
    RUN_TEST(test_insert_line_after);
    RUN_TEST(test_insert_line_before);
    RUN_TEST(test_delete_range_negative_end_is_safe);
    RUN_TEST(test_delete_range_start_beyond_end_is_safe);
    RUN_TEST(test_save_and_load);
    RUN_TEST(test_load_long_line_not_split);
    RUN_TEST(test_load_failure_keeps_buffer_intact);
    RUN_TEST(test_new_buffer_saves_trailing_newline);
    RUN_TEST(test_load_preserves_missing_trailing_newline);
    RUN_TEST(test_load_newline_only_file);
    RUN_TEST(test_load_empty_file);
    RUN_TEST(test_save_long_line_not_truncated);
    RUN_TEST(test_save_unencodable_char_fails);
    RUN_TEST(test_load_cjk_line);
    RUN_TEST(test_load_invalid_utf8_fails);
    RUN_TEST(test_load_embedded_nul_fails);
    RUN_TEST(test_indent_range_with_tab);
    RUN_TEST(test_indent_range_with_spaces);
    RUN_TEST(test_unindent_tab);
    RUN_TEST(test_get_shift_width_default);
    RUN_TEST(test_get_shift_width_custom);
    RUN_TEST(test_mod_count_increments);
    RUN_TEST(test_insert_tab_literal);
    RUN_TEST(test_insert_tab_expand);
    RUN_TEST(test_delete_char_at);
    RUN_TEST(test_delete_char_at_out_of_bounds);
    RUN_TEST(test_replace_all_replaces_content);
    RUN_TEST(test_replace_all_single_line);
    RUN_TEST(test_replace_all_empty_source);
    RUN_TEST(test_check_writable_null_filename_fails);
    RUN_TEST(test_check_writable_nonexistent_file_ok);
    RUN_TEST(test_check_writable_writable_file_ok);
    RUN_TEST(test_check_writable_readonly_file_fails);
    return UNITY_END();
}
