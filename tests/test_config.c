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

/* --- Trailing comma tolerance -------------------------------------------- */

static const char *TRAILING_COMMA_CONFIG =
    "{"
    "  \"file_types\": {"
    "    \"yaml\": {"
    "      \"extensions\": [\".yml\", \".yaml\"],"
    "      \"syntax_highlighting\": false,"
    "      \"tabstop\": 2,"
    "      \"shiftwidth\": 2,"
    "      \"autoindentation\": true,"
    "      \"expandtab\": true,"
    "      \"syntax_rules\": []"
    "    },"
    "  },"
    "  \"maps\": {}"
    "}";

void test_load_trailing_comma_in_object(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_trailing_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", TRAILING_COMMA_CONFIG);
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(1, cfg->file_type_count);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, "yaml");
    TEST_ASSERT_NOT_NULL(ftc);
    TEST_ASSERT_EQUAL(2, ftc->tab_stop);
    TEST_ASSERT_EQUAL(2, ftc->shift_width);
    TEST_ASSERT_TRUE(ftc->auto_indentation);
    TEST_ASSERT_TRUE(ftc->expand_tab);

    unlink(path);
    config_free(cfg);
}

static const char *TRAILING_COMMA_ARRAY_CONFIG =
    "{"
    "  \"file_types\": {"
    "    \"go\": {"
    "      \"extensions\": [\".go\",],"
    "      \"syntax_highlighting\": false,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4,"
    "      \"expandtab\": false,"
    "      \"syntax_rules\": []"
    "    }"
    "  }"
    "}";

void test_load_trailing_comma_in_array(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_trail_arr_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", TRAILING_COMMA_ARRAY_CONFIG);
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, "go");
    TEST_ASSERT_NOT_NULL(ftc);
    TEST_ASSERT_EQUAL(1, ftc->ext_count);
    TEST_ASSERT_EQUAL_STRING(".go", ftc->extensions[0]);

    unlink(path);
    config_free(cfg);
}

static const char *COMMA_IN_STRING_CONFIG =
    "{"
    "  \"file_types\": {"
    "    \"csv\": {"
    "      \"extensions\": [\".csv\"],"
    "      \"syntax_highlighting\": false,"
    "      \"tabstop\": 4,"
    "      \"shiftwidth\": 4,"
    "      \"expandtab\": false,"
    "      \"syntax_rules\": ["
    "        {\"pattern\": \"a,}\", \"style\": {\"color\": \"red,}\"}, \"priority\": 1}"
    "      ]"
    "    }"
    "  }"
    "}";

void test_load_comma_in_string_preserved(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_comma_str_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", COMMA_IN_STRING_CONFIG);
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, "csv");
    TEST_ASSERT_NOT_NULL(ftc);
    TEST_ASSERT_EQUAL(1, ftc->rule_count);
    TEST_ASSERT_EQUAL_STRING("a,}", ftc->syntax_rules[0].pattern);
    TEST_ASSERT_EQUAL_STRING("red,}", ftc->syntax_rules[0].style.color);

    unlink(path);
    config_free(cfg);
}

/* --- config_ensure_default tests ----------------------------------------- */

void test_ensure_default_creates_file(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_ensure_%d.json", getpid());
    unlink(path);

    bool created = config_ensure_default(path);
    TEST_ASSERT_TRUE(created);

    /* File must exist and be valid config */
    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_GREATER_THAN(0, cfg->file_type_count);
    config_free(cfg);
    unlink(path);
}

void test_ensure_default_skips_existing(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_ensure_%d.json", getpid());

    /* Write a custom config */
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"file_types\":{}}");
    fclose(f);

    bool created = config_ensure_default(path);
    TEST_ASSERT_FALSE(created);

    /* File content should be unchanged (empty file_types) */
    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(0, cfg->file_type_count);
    config_free(cfg);
    unlink(path);
}

void test_ensure_default_null_uses_home(void) {
    /* Just verify it doesn't crash with NULL path */
    config_ensure_default(NULL);
}

void test_ensure_default_bad_path(void) {
    bool created = config_ensure_default("/nonexistent_dir/sub/ef.json");
    TEST_ASSERT_FALSE(created);
}

/* --- Maps tests ---------------------------------------------------------- */

static const char *MAPS_CONFIG =
    "{"
    "  \"file_types\": {},"
    "  \"maps\": {"
    "    \"((\": \"()<Esc>ha\","
    "    \"{{\": \"{}<Esc>ha<Enter><Esc>ko<Tab>\""
    "  }"
    "}";

static const char *NO_MAPS_CONFIG =
    "{"
    "  \"file_types\": {}"
    "}";

void test_maps_parsed(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_maps_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", MAPS_CONFIG);
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(2, cfg->map_count);

    /* Find the "((" mapping */
    bool found_paren = false, found_brace = false;
    for (int i = 0; i < cfg->map_count; i++) {
        if (strcmp(cfg->maps[i].trigger, "((") == 0) {
            TEST_ASSERT_EQUAL_STRING("()<Esc>ha", cfg->maps[i].expansion);
            found_paren = true;
        }
        if (strcmp(cfg->maps[i].trigger, "{{") == 0) {
            TEST_ASSERT_EQUAL_STRING("{}<Esc>ha<Enter><Esc>ko<Tab>",
                                     cfg->maps[i].expansion);
            found_brace = true;
        }
    }
    TEST_ASSERT_TRUE(found_paren);
    TEST_ASSERT_TRUE(found_brace);

    unlink(path);
    config_free(cfg);
}

void test_maps_no_maps_section(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_nomaps_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "%s", NO_MAPS_CONFIG);
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(0, cfg->map_count);
    TEST_ASSERT_NULL(cfg->maps);

    unlink(path);
    config_free(cfg);
}

void test_maps_empty_maps_section(void) {
    char path[256];
    snprintf(path, sizeof(path), "/tmp/ef_emptymaps_%d.json", getpid());
    FILE *f = fopen(path, "w");
    fprintf(f, "{\"file_types\":{},\"maps\":{}}");
    fclose(f);

    EditorConfig *cfg = config_load(path);
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL(0, cfg->map_count);

    unlink(path);
    config_free(cfg);
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
    RUN_TEST(test_load_trailing_comma_in_object);
    RUN_TEST(test_load_trailing_comma_in_array);
    RUN_TEST(test_load_comma_in_string_preserved);
    RUN_TEST(test_ensure_default_creates_file);
    RUN_TEST(test_ensure_default_skips_existing);
    RUN_TEST(test_ensure_default_null_uses_home);
    RUN_TEST(test_ensure_default_bad_path);
    RUN_TEST(test_maps_parsed);
    RUN_TEST(test_maps_no_maps_section);
    RUN_TEST(test_maps_empty_maps_section);

    return UNITY_END();
}
