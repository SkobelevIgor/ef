#include "unity.h"
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

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
    "        {\"pattern\": \"//.*$\", \"style\": {\"color\": \"gray\", \"bold\": false}, \"priority\": 30},"
    "        {\"pattern\": \"\\\\b(if|else|for)\\\\b\", \"style\": {\"color\": \"yellow\", \"bold\": false}, \"priority\": 10}"
    "      ]"
    "    },"
    "    \"python\": {"
    "      \"extensions\": [\".py\", \".pyw\"],"
    "      \"syntax_highlighting\": true,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4,"
    "      \"expandtab\": true,"
    "      \"syntax_rules\": ["
    "        {\"pattern\": \"#.*$\", \"style\": {\"color\": \"gray\"}, \"priority\": 30}"
    "      ]"
    "    }"
    "  }"
    "}";

static char tmp_path[256];

static void write_tmp_config(void) {
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/ef_test_config_%d.json", getpid());
    FILE *f = fopen(tmp_path, "w");
    fprintf(f, "%s", TEST_CONFIG);
    fclose(f);
}

static void remove_tmp_config(void) {
    unlink(tmp_path);
}

/* --- Tests --------------------------------------------------------------- */

void test_load_nonexistent(void) {
    EditorConfig *cfg = config_load("/tmp/nonexistent_ef_config_xyz");
    TEST_ASSERT_NULL(cfg);
}

void test_load_valid(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(2, cfg->file_type_count);
    remove_tmp_config();
    config_free(cfg);
}

void test_file_type_go(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);
    TEST_ASSERT_NOT_NULL(cfg);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, "go");
    TEST_ASSERT_NOT_NULL(ftc);
    TEST_ASSERT_EQUAL(1, ftc->ext_count);
    TEST_ASSERT_EQUAL_STRING(".go", ftc->extensions[0]);
    TEST_ASSERT_TRUE(ftc->syntax_highlighting);
    TEST_ASSERT_EQUAL(4, ftc->tab_stop);
    TEST_ASSERT_FALSE(ftc->expand_tab);
    TEST_ASSERT_EQUAL(2, ftc->rule_count);

    TEST_ASSERT_EQUAL_STRING("gray", ftc->syntax_rules[0].style.color);
    TEST_ASSERT_EQUAL(30, ftc->syntax_rules[0].priority);
    TEST_ASSERT_EQUAL(10, ftc->syntax_rules[1].priority);

    remove_tmp_config();
    config_free(cfg);
}

void test_file_type_python(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, "python");
    TEST_ASSERT_NOT_NULL(ftc);
    TEST_ASSERT_EQUAL(2, ftc->ext_count);
    TEST_ASSERT_TRUE(ftc->expand_tab);
    TEST_ASSERT_EQUAL(1, ftc->rule_count);

    remove_tmp_config();
    config_free(cfg);
}

void test_detect_go_file(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);

    const char *ft = config_detect_file_type(cfg, "main.go");
    TEST_ASSERT_NOT_NULL(ft);
    TEST_ASSERT_EQUAL_STRING("go", ft);

    remove_tmp_config();
    config_free(cfg);
}

void test_detect_python_file(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);

    const char *ft = config_detect_file_type(cfg, "script.py");
    TEST_ASSERT_NOT_NULL(ft);
    TEST_ASSERT_EQUAL_STRING("python", ft);

    ft = config_detect_file_type(cfg, "app.pyw");
    TEST_ASSERT_NOT_NULL(ft);
    TEST_ASSERT_EQUAL_STRING("python", ft);

    remove_tmp_config();
    config_free(cfg);
}

void test_detect_unknown_file(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);

    const char *ft = config_detect_file_type(cfg, "readme.txt");
    TEST_ASSERT_NULL(ft);

    remove_tmp_config();
    config_free(cfg);
}

void test_detect_no_extension(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);

    const char *ft = config_detect_file_type(cfg, "Makefile");
    TEST_ASSERT_NULL(ft);

    remove_tmp_config();
    config_free(cfg);
}

void test_detect_null_args(void) {
    TEST_ASSERT_NULL(config_detect_file_type(NULL, "main.go"));
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);
    TEST_ASSERT_NULL(config_detect_file_type(cfg, NULL));
    remove_tmp_config();
    config_free(cfg);
}

void test_get_unknown_type(void) {
    write_tmp_config();
    EditorConfig *cfg = config_load(tmp_path);
    TEST_ASSERT_NULL(config_get_file_type(cfg, "rust"));
    remove_tmp_config();
    config_free(cfg);
}

void test_free_null(void) {
    config_free(NULL); /* Should not crash */
}

/* --- Runner -------------------------------------------------------------- */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_load_nonexistent);
    RUN_TEST(test_load_valid);
    RUN_TEST(test_file_type_go);
    RUN_TEST(test_file_type_python);
    RUN_TEST(test_detect_go_file);
    RUN_TEST(test_detect_python_file);
    RUN_TEST(test_detect_unknown_file);
    RUN_TEST(test_detect_no_extension);
    RUN_TEST(test_detect_null_args);
    RUN_TEST(test_get_unknown_type);
    RUN_TEST(test_free_null);

    return UNITY_END();
}
