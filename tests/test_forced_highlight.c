#include "unity.h"
#include "test_helpers.h"
#include "editor.h"
#include "config.h"
#include "syntax.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static Editor *ed;
static Buffer *buf;
static char tmp_path[256];

static const char *TEST_CONFIG =
    "{"
    "  \"file_types\": {"
    "    \"go\": {"
    "      \"extensions\": [\".go\"],"
    "      \"syntax_highlighting\": true,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4,"
    "      \"autoindentation\": true,"
    "      \"expandtab\": false,"
    "      \"syntax_rules\": ["
    "        {\"pattern\": \"//.*$\","
    "         \"style\": {\"color\": \"gray\", \"bold\": false},"
    "         \"priority\": 30}"
    "      ]"
    "    },"
    "    \"python\": {"
    "      \"extensions\": [\".py\"],"
    "      \"syntax_highlighting\": true,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4,"
    "      \"expandtab\": true,"
    "      \"syntax_rules\": ["
    "        {\"pattern\": \"#.*$\","
    "         \"style\": {\"color\": \"green\", \"bold\": false},"
    "         \"priority\": 30}"
    "      ]"
    "    },"
    "    \"plain\": {"
    "      \"extensions\": [\".txt\"],"
    "      \"syntax_highlighting\": false,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4"
    "    }"
    "  }"
    "}";

static void write_tmp_config(void) {
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/ef_test_fh_%d.json", getpid());
    FILE *f = fopen(tmp_path, "w");
    fprintf(f, "%s", TEST_CONFIG);
    fclose(f);
}

static void remove_tmp_config(void) {
    unlink(tmp_path);
}

void setUp(void)    { ed = NULL; buf = NULL; }
void tearDown(void) {
    if (ed) { editor_free(ed); ed = NULL; buf = NULL; }
}

/* --- apply NULL is a no-op ------------------------------------------------ */

void test_forced_highlight_null_is_noop(void) {
    const wchar_t *lines[] = {L"hello"};
    test_setup_editor(lines, 1, &ed, &buf);

    editor_apply_forced_highlighting(ed, NULL);

    TEST_ASSERT_NULL(buf->file_type);
    TEST_ASSERT_NULL(buf->highlight_cache);
}

/* --- apply NULL with no config doesn't crash ----------------------------- */

void test_forced_highlight_no_config_is_noop(void) {
    const wchar_t *lines[] = {L"hello"};
    test_setup_editor(lines, 1, &ed, &buf);

    editor_apply_forced_highlighting(ed, "go");

    TEST_ASSERT_NULL(buf->file_type);
    TEST_ASSERT_NULL(buf->highlight_cache);
}

/* --- apply sets file_type on buffer -------------------------------------- */

void test_forced_highlight_sets_file_type(void) {
    const wchar_t *lines[] = {L"// hello"};
    test_setup_editor(lines, 1, &ed, &buf);
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "go");

    TEST_ASSERT_NOT_NULL(buf->file_type);
    TEST_ASSERT_EQUAL_STRING("go", buf->file_type);
}

/* --- apply stores forced type on editor ---------------------------------- */

void test_forced_highlight_stores_type_on_editor(void) {
    const wchar_t *lines[] = {L"hello"};
    test_setup_editor(lines, 1, &ed, &buf);
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "python");

    TEST_ASSERT_NOT_NULL(ed->forced_file_type);
    TEST_ASSERT_EQUAL_STRING("python", ed->forced_file_type);
}

/* --- apply creates highlight_cache for type with rules ------------------- */

void test_forced_highlight_creates_cache(void) {
    const wchar_t *lines[] = {L"// hello"};
    test_setup_editor(lines, 1, &ed, &buf);
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "go");

    TEST_ASSERT_NOT_NULL(buf->highlight_cache);
}

/* --- forced type overrides filename extension ---------------------------- */

void test_forced_highlight_overrides_extension(void) {
    const wchar_t *lines[] = {L"// code"};
    test_setup_editor(lines, 1, &ed, &buf);
    /* Buffer has no filename — auto-detect would fail, forced type must win */
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "python");

    TEST_ASSERT_EQUAL_STRING("python", buf->file_type);
}

/* --- switching to type with no syntax rules clears old cache ------------- */

void test_forced_highlight_clears_cache_on_no_rules(void) {
    const wchar_t *lines[] = {L"// hello"};
    test_setup_editor(lines, 1, &ed, &buf);
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "go");
    TEST_ASSERT_NOT_NULL(buf->highlight_cache);

    editor_apply_forced_highlighting(ed, "plain");
    TEST_ASSERT_NULL(buf->highlight_cache);
}

/* --- second apply replaces previous forced type -------------------------- */

void test_forced_highlight_replaces_previous(void) {
    const wchar_t *lines[] = {L"hello"};
    test_setup_editor(lines, 1, &ed, &buf);
    write_tmp_config();
    ed->config = config_load(tmp_path);
    remove_tmp_config();

    editor_apply_forced_highlighting(ed, "go");
    editor_apply_forced_highlighting(ed, "python");

    TEST_ASSERT_EQUAL_STRING("python", ed->forced_file_type);
    TEST_ASSERT_EQUAL_STRING("python", buf->file_type);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_forced_highlight_null_is_noop);
    RUN_TEST(test_forced_highlight_no_config_is_noop);
    RUN_TEST(test_forced_highlight_sets_file_type);
    RUN_TEST(test_forced_highlight_stores_type_on_editor);
    RUN_TEST(test_forced_highlight_creates_cache);
    RUN_TEST(test_forced_highlight_overrides_extension);
    RUN_TEST(test_forced_highlight_clears_cache_on_no_rules);
    RUN_TEST(test_forced_highlight_replaces_previous);
    return UNITY_END();
}
