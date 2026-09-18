#include "unity.h"
#include "config.h"
#include "default_config.h"
#include "syntax.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <time.h>

/* ncurses color constants */
#define C_RED    1
#define C_GREEN  2
#define C_YELLOW 3
#define C_BLUE   4
#define C_PURPLE 5
#define C_CYAN   6
#define C_GRAY   8

static char tmp_path[256];
static EditorConfig *cfg;

void setUp(void) {
    snprintf(tmp_path, sizeof(tmp_path),
             "/tmp/ef_test_default_config_%d.json", getpid());
    unlink(tmp_path);
    TEST_ASSERT_TRUE(config_ensure_default(tmp_path));
    cfg = config_load(tmp_path);
    TEST_ASSERT_NOT_NULL(cfg);
}

void tearDown(void) {
    config_free(cfg);
    unlink(tmp_path);
}

/* Color of the token covering `col` in `line` highlighted as `type`. */
static SyntaxToken token_at(const char *type, const wchar_t *line, int col) {
    const EditorFileTypeConfig *ft = config_get_file_type(cfg, type);
    TEST_ASSERT_NOT_NULL(ft);
    SyntaxHighlighter *h = syntax_highlighter_new(ft->syntax_rules,
                                                  ft->rule_count);
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, (int)wcslen(line),
                                                &count);
    const SyntaxToken *t = syntax_token_at(tokens, count, col);
    SyntaxToken result = t ? *t : (SyntaxToken){0, 0, -1, false};
    free(tokens);
    syntax_highlighter_free(h);
    return result;
}

static short color_at(const char *type, const wchar_t *line, int col) {
    return token_at(type, line, col).fg_color;
}

static bool bold_at(const char *type, const wchar_t *line, int col) {
    return token_at(type, line, col).bold;
}

static void assert_type_enabled(const char *filename, const char *type) {
    TEST_ASSERT_EQUAL_STRING(type, config_detect_file_type(cfg, filename));
    const EditorFileTypeConfig *ft = config_get_file_type(cfg, type);
    TEST_ASSERT_NOT_NULL(ft);
    TEST_ASSERT_TRUE(ft->syntax_highlighting);
    TEST_ASSERT_GREATER_THAN(0, ft->rule_count);
}

/* --- Embedded copy ------------------------------------------------------- */

void test_embedded_matches_efconfig(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s", __FILE__);
    char *slash = strrchr(path, '/');
    TEST_ASSERT_NOT_NULL(slash);
    snprintf(slash, sizeof(path) - (size_t)(slash - path), "/../.efconfig");
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    unsigned char buf[16384];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL(default_config_len, n);
    TEST_ASSERT_EQUAL_MEMORY(default_config_data, buf, n);
}

void test_empty_and_unicode_lines(void) {
    TEST_ASSERT_EQUAL(-1, color_at("json", L"", 0));
    TEST_ASSERT_EQUAL(-1, color_at("yaml", L"", 0));
    const wchar_t *line = L"\"k\": \"日本語 🚀\"";
    TEST_ASSERT_EQUAL(C_CYAN, color_at("json", line, 1));
    TEST_ASSERT_EQUAL(C_GREEN, color_at("json", line, 8));
}

/* A key-looking prefix, a long whitespace run, then a letter: no key. */
static void assert_whitespace_run_is_fast(const char *type) {
    const int len = 100000;
    wchar_t *line = malloc((len + 1) * sizeof(wchar_t));
    for (int i = 0; i < len; i++) line[i] = L' ';
    line[0] = L'x';
    line[len - 1] = L'y';
    line[len] = 0;
    const EditorFileTypeConfig *ft = config_get_file_type(cfg, type);
    SyntaxHighlighter *h = syntax_highlighter_new(ft->syntax_rules,
                                                  ft->rule_count);
    int count;
    clock_t t0 = clock();
    SyntaxToken *tokens = syntax_highlight_line(h, line, len, &count);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    free(tokens);
    syntax_highlighter_free(h);
    free(line);
    TEST_ASSERT_TRUE_MESSAGE(secs < 0.5, "key rule backtracks quadratically");
}

void test_yaml_key_rule_whitespace_run_is_fast(void) {
    assert_whitespace_run_is_fast("yaml");
}

void test_ini_key_rule_whitespace_run_is_fast(void) {
    assert_whitespace_run_is_fast("ini");
}

/* --- JSON ---------------------------------------------------------------- */

void test_json_detected(void) {
    assert_type_enabled("data.json", "json");
}

void test_json_key_and_string(void) {
    const wchar_t *line = L"  \"name\": \"ef\",";
    TEST_ASSERT_EQUAL(C_CYAN, color_at("json", line, 3));    /* name */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("json", line, 11));  /* ef */
}

void test_json_literals(void) {
    const wchar_t *line = L"\"a\": [true, null, 42]";
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("json", line, 5));  /* [ */
    TEST_ASSERT_EQUAL(C_BLUE, color_at("json", line, 6));    /* true */
    TEST_ASSERT_EQUAL(C_BLUE, color_at("json", line, 12));   /* null */
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("json", line, 18)); /* 42 */
    TEST_ASSERT_TRUE(bold_at("json", line, 6));
}

void test_json_escape(void) {
    const wchar_t *line = L"\"esc\": \"a\\nb\"";
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("json", line, 10)); /* \n */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("json", line, 12));  /* b */
}

/* --- XML ----------------------------------------------------------------- */

void test_xml_detected(void) {
    assert_type_enabled("data.xml", "xml");
}

void test_xml_tag_attribute_string(void) {
    const wchar_t *line = L"<setting key=\"x\">4</setting>";
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("xml", line, 0));  /* < */
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("xml", line, 1));  /* setting */
    TEST_ASSERT_EQUAL(C_CYAN, color_at("xml", line, 9));    /* key */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("xml", line, 14));  /* "x" */
    TEST_ASSERT_EQUAL(-1, color_at("xml", line, 17));       /* 4 */
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("xml", line, 20)); /* setting */
    TEST_ASSERT_TRUE(bold_at("xml", line, 1));
}

void test_xml_cdata_and_processing_instruction(void) {
    TEST_ASSERT_EQUAL(C_GREEN, color_at("xml", L"<a><![CDATA[x<y]]></a>", 13));
    TEST_ASSERT_EQUAL(C_GREEN, color_at("xml", L"  ]]></a>", 3));
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("xml", L"<?xml version=\"1\"?>", 1));
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("xml", L"<?xml version=\"1\"?>", 2));
}

void test_xml_comment_and_entity(void) {
    TEST_ASSERT_EQUAL(C_GRAY, color_at("xml", L"<!-- hi -->", 5));
    TEST_ASSERT_EQUAL(C_RED, color_at("xml", L"<a>&amp;</a>", 4));
}

/* --- YAML ---------------------------------------------------------------- */

void test_yaml_detected(void) {
    assert_type_enabled("data.yaml", "yaml");
}

void test_yaml_key_value_comment(void) {
    const wchar_t *line = L"name: ef # note";
    TEST_ASSERT_EQUAL(C_CYAN, color_at("yaml", line, 0));   /* name */
    TEST_ASSERT_EQUAL(-1, color_at("yaml", line, 6));       /* ef */
    TEST_ASSERT_EQUAL(C_GRAY, color_at("yaml", line, 9));   /* # note */
}

void test_yaml_sequence_item(void) {
    const wchar_t *line = L"  - key: \"v\"";
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("yaml", line, 2)); /* - */
    TEST_ASSERT_EQUAL(C_CYAN, color_at("yaml", line, 4));   /* key */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("yaml", line, 10)); /* "v" */
}

void test_yaml_anchor_bool_number(void) {
    TEST_ASSERT_EQUAL(C_RED, color_at("yaml", L"x: &a true", 3));    /* &a */
    TEST_ASSERT_EQUAL(C_BLUE, color_at("yaml", L"x: &a true", 6));   /* true */
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("yaml", L"n: -42", 3));     /* -42 */
    TEST_ASSERT_EQUAL(-1, color_at("yaml", L"  No trailing text", 2)); /* prose */
    TEST_ASSERT_EQUAL(-1, color_at("yaml", L"msg: hello world!", 16));  /* ! */
    TEST_ASSERT_EQUAL(-1, color_at("yaml", L"msg: A&B *bold*", 6));     /* &B */
    TEST_ASSERT_EQUAL(C_RED, color_at("yaml", L"t: !!str 5", 3));       /* !!str */
}

void test_yaml_markers_flow_key_quotes_timestamp(void) {
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("yaml", L"---", 0));
    TEST_ASSERT_TRUE(bold_at("yaml", L"---", 0));
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("yaml", L"...", 0));
    TEST_ASSERT_EQUAL(C_CYAN, color_at("yaml", L"m: {a: 1}", 4));      /* a */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("yaml", L"s: 'it''s'", 5));
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("yaml", L"d: 2026-09-18", 5));
}

/* --- INI ----------------------------------------------------------------- */

void test_ini_detected(void) {
    assert_type_enabled("data.ini", "ini");
}

void test_ini_section_key_value(void) {
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("ini", L"[general]", 1));
    TEST_ASSERT_TRUE(bold_at("ini", L"[general]", 1));
    TEST_ASSERT_EQUAL(C_PURPLE, color_at("ini", L"[a] ; note", 1));
    const wchar_t *line = L"name = ef";
    TEST_ASSERT_EQUAL(C_CYAN, color_at("ini", line, 0));   /* name */
    TEST_ASSERT_EQUAL(C_GREEN, color_at("ini", line, 7));  /* ef */
}

void test_ini_comment_and_number(void) {
    TEST_ASSERT_EQUAL(C_GRAY, color_at("ini", L"; comment", 3));
    TEST_ASSERT_EQUAL(C_GRAY, color_at("ini", L"# comment", 3));
    TEST_ASSERT_EQUAL(C_YELLOW, color_at("ini", L"n = 42", 4));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_embedded_matches_efconfig);
    RUN_TEST(test_empty_and_unicode_lines);
    RUN_TEST(test_yaml_key_rule_whitespace_run_is_fast);
    RUN_TEST(test_ini_key_rule_whitespace_run_is_fast);
    RUN_TEST(test_json_detected);
    RUN_TEST(test_json_key_and_string);
    RUN_TEST(test_json_literals);
    RUN_TEST(test_json_escape);
    RUN_TEST(test_xml_detected);
    RUN_TEST(test_xml_tag_attribute_string);
    RUN_TEST(test_xml_comment_and_entity);
    RUN_TEST(test_xml_cdata_and_processing_instruction);
    RUN_TEST(test_yaml_detected);
    RUN_TEST(test_yaml_key_value_comment);
    RUN_TEST(test_yaml_sequence_item);
    RUN_TEST(test_yaml_anchor_bool_number);
    RUN_TEST(test_yaml_markers_flow_key_quotes_timestamp);
    RUN_TEST(test_ini_detected);
    RUN_TEST(test_ini_section_key_value);
    RUN_TEST(test_ini_comment_and_number);
    return UNITY_END();
}
