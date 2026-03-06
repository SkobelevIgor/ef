#include "config.h"
#include "default_config.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, len, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static void parse_syntax_rules(cJSON *rules_json, EditorFileTypeConfig *ftc) {
    int count = cJSON_GetArraySize(rules_json);
    ftc->syntax_rules = calloc(count, sizeof(SyntaxRuleConfig));
    ftc->rule_count = 0;

    for (int i = 0; i < count; i++) {
        cJSON *rule = cJSON_GetArrayItem(rules_json, i);
        cJSON *pattern = cJSON_GetObjectItem(rule, "pattern");
        cJSON *style = cJSON_GetObjectItem(rule, "style");
        cJSON *priority = cJSON_GetObjectItem(rule, "priority");
        if (!pattern || !cJSON_IsString(pattern)) continue;

        SyntaxRuleConfig *rc = &ftc->syntax_rules[ftc->rule_count++];
        rc->pattern = strdup(pattern->valuestring);
        rc->priority = priority ? priority->valueint : 0;

        if (style) {
            cJSON *color = cJSON_GetObjectItem(style, "color");
            cJSON *bold_j = cJSON_GetObjectItem(style, "bold");
            rc->style.color = (color && cJSON_IsString(color))
                ? strdup(color->valuestring) : NULL;
            rc->style.bold = (bold_j && cJSON_IsTrue(bold_j));
        }
    }
}

static void parse_file_type(cJSON *ft, EditorFileTypeConfig *ftc) {
    cJSON *exts = cJSON_GetObjectItem(ft, "extensions");
    if (exts) {
        int n = cJSON_GetArraySize(exts);
        ftc->extensions = calloc(n, sizeof(char *));
        for (int j = 0; j < n; j++) {
            cJSON *ext = cJSON_GetArrayItem(exts, j);
            if (cJSON_IsString(ext))
                ftc->extensions[ftc->ext_count++] = strdup(ext->valuestring);
        }
    }

    cJSON *sh = cJSON_GetObjectItem(ft, "syntax_highlighting");
    ftc->syntax_highlighting = sh && cJSON_IsTrue(sh);

    cJSON *ts = cJSON_GetObjectItem(ft, "tabstop");
    ftc->tab_stop = ts ? ts->valueint : 4;

    cJSON *sw = cJSON_GetObjectItem(ft, "shiftwidth");
    ftc->shift_width = sw ? sw->valueint : 4;

    cJSON *ai = cJSON_GetObjectItem(ft, "autoindentation");
    ftc->auto_indentation = !ai || cJSON_IsTrue(ai);

    cJSON *et = cJSON_GetObjectItem(ft, "expandtab");
    ftc->expand_tab = et && cJSON_IsTrue(et);

    cJSON *rules = cJSON_GetObjectItem(ft, "syntax_rules");
    if (rules && cJSON_IsArray(rules))
        parse_syntax_rules(rules, ftc);
}

static bool resolve_config_path(const char *path, char *out, size_t len) {
    if (path) {
        snprintf(out, len, "%s", path);
        return true;
    }
    const char *home = getenv("HOME");
    if (!home) return false;
    snprintf(out, len, "%s/.efconfig", home);
    return true;
}

bool config_ensure_default(const char *path) {
    char config_path[512];
    if (!resolve_config_path(path, config_path, sizeof(config_path)))
        return false;

    struct stat st;
    if (stat(config_path, &st) == 0)
        return false;

    FILE *f = fopen(config_path, "w");
    if (!f) return false;

    fwrite(default_config_data, 1, default_config_len, f);
    fclose(f);
    return true;
}

EditorConfig *config_load(const char *path) {
    char config_path[512];
    if (!resolve_config_path(path, config_path, sizeof(config_path)))
        return NULL;

    char *json_str = read_file_contents(config_path);
    if (!json_str) return NULL;

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return NULL;

    EditorConfig *cfg = calloc(1, sizeof(EditorConfig));

    cJSON *file_types = cJSON_GetObjectItem(root, "file_types");
    if (file_types) {
        int count = cJSON_GetArraySize(file_types);
        cfg->file_types = calloc(count, sizeof(EditorFileTypeConfig));
        cfg->file_type_names = calloc(count, sizeof(char *));

        cJSON *ft;
        int idx = 0;
        cJSON_ArrayForEach(ft, file_types) {
            cfg->file_type_names[idx] = strdup(ft->string);
            parse_file_type(ft, &cfg->file_types[idx]);
            idx++;
        }
        cfg->file_type_count = idx;
    }

    cJSON_Delete(root);
    return cfg;
}

const char *config_detect_file_type(const EditorConfig *cfg,
                                     const char *filename) {
    if (!cfg || !filename) return NULL;
    const char *dot = strrchr(filename, '.');
    if (!dot) return NULL;

    for (int i = 0; i < cfg->file_type_count; i++) {
        for (int j = 0; j < cfg->file_types[i].ext_count; j++) {
            if (strcasecmp(dot, cfg->file_types[i].extensions[j]) == 0)
                return cfg->file_type_names[i];
        }
    }
    return NULL;
}

const EditorFileTypeConfig *config_get_file_type(const EditorConfig *cfg,
                                                  const char *name) {
    if (!cfg || !name) return NULL;
    for (int i = 0; i < cfg->file_type_count; i++) {
        if (strcmp(cfg->file_type_names[i], name) == 0)
            return &cfg->file_types[i];
    }
    return NULL;
}

void config_free(EditorConfig *cfg) {
    if (!cfg) return;
    for (int i = 0; i < cfg->file_type_count; i++) {
        free(cfg->file_type_names[i]);
        EditorFileTypeConfig *ftc = &cfg->file_types[i];
        for (int j = 0; j < ftc->ext_count; j++) free(ftc->extensions[j]);
        free(ftc->extensions);
        for (int j = 0; j < ftc->rule_count; j++) {
            free(ftc->syntax_rules[j].pattern);
            free(ftc->syntax_rules[j].style.color);
        }
        free(ftc->syntax_rules);
    }
    free(cfg->file_types);
    free(cfg->file_type_names);
    free(cfg);
}
