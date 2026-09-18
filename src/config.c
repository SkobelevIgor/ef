#include "config.h"
#include "xalloc.h"
#include "default_config.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

/* Strip trailing commas before } and ] (JSONC tolerance).
   Modifies the string in place. */
static bool is_json_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Strip trailing commas before } and ] (JSONC tolerance).
   Modifies the string in place. Handles strings/escapes to avoid
   stripping commas inside quoted values. */
static void strip_trailing_commas(char *json) {
    size_t len = strlen(json);
    size_t w = 0;
    bool in_string = false;

    for (size_t i = 0; i < len; i++) {
        if (in_string) {
            if (json[i] == '\\' && i + 1 < len) {
                json[w++] = json[i++];
                json[w++] = json[i];
                continue;
            }
            if (json[i] == '"')
                in_string = false;
            json[w++] = json[i];
            continue;
        }
        if (json[i] == '"') {
            in_string = true;
            json[w++] = json[i];
            continue;
        }
        if (json[i] == ',') {
            size_t j = i + 1;
            while (j < len && is_json_ws(json[j]))
                j++;
            if (j < len && (json[j] == '}' || json[j] == ']'))
                continue; /* skip trailing comma */
        }
        json[w++] = json[i];
    }
    json[w] = '\0';
}

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = xmalloc(len + 1);
    size_t read = fread(buf, 1, len, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

static void parse_syntax_rules(cJSON *rules_json, EditorFileTypeConfig *ftc) {
    int count = cJSON_GetArraySize(rules_json);
    if (count <= 0) return;
    ftc->syntax_rules = xcalloc(count, sizeof(SyntaxRuleConfig));
    ftc->rule_count = 0;

    for (int i = 0; i < count; i++) {
        cJSON *rule = cJSON_GetArrayItem(rules_json, i);
        cJSON *pattern = cJSON_GetObjectItem(rule, "pattern");
        cJSON *style = cJSON_GetObjectItem(rule, "style");
        cJSON *priority = cJSON_GetObjectItem(rule, "priority");
        if (!pattern || !cJSON_IsString(pattern)) continue;

        SyntaxRuleConfig *rc = &ftc->syntax_rules[ftc->rule_count++];
        rc->pattern = xstrdup(pattern->valuestring);
        rc->priority = priority ? priority->valueint : 0;

        if (style) {
            cJSON *color = cJSON_GetObjectItem(style, "color");
            cJSON *bold_j = cJSON_GetObjectItem(style, "bold");
            rc->style.color = (color && cJSON_IsString(color))
                ? xstrdup(color->valuestring) : NULL;
            rc->style.bold = (bold_j && cJSON_IsTrue(bold_j));
        }
    }
}

static void parse_file_type(cJSON *ft, EditorFileTypeConfig *ftc) {
    cJSON *exts = cJSON_GetObjectItem(ft, "extensions");
    int n = exts ? cJSON_GetArraySize(exts) : 0;
    if (n > 0) {
        ftc->extensions = xcalloc(n, sizeof(char *));
        for (int j = 0; j < n; j++) {
            cJSON *ext = cJSON_GetArrayItem(exts, j);
            if (cJSON_IsString(ext))
                ftc->extensions[ftc->ext_count++] = xstrdup(ext->valuestring);
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

    size_t written = fwrite(default_config_data, 1, default_config_len, f);
    fclose(f);
    return written == default_config_len;
}

EditorConfig *config_load(const char *path) {
    char config_path[512];
    if (!resolve_config_path(path, config_path, sizeof(config_path)))
        return NULL;

    char *json_str = read_file_contents(config_path);
    if (!json_str) return NULL;

    strip_trailing_commas(json_str);
    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return NULL;

    EditorConfig *cfg = xcalloc(1, sizeof(EditorConfig));

    cJSON *file_types = cJSON_GetObjectItem(root, "file_types");
    int ft_count = file_types && cJSON_IsObject(file_types)
        ? cJSON_GetArraySize(file_types) : 0;
    if (ft_count > 0) {
        cfg->file_types = xcalloc(ft_count, sizeof(EditorFileTypeConfig));
        cfg->file_type_names = xcalloc(ft_count, sizeof(char *));

        cJSON *ft;
        int idx = 0;
        cJSON_ArrayForEach(ft, file_types) {
            if (!ft->string) continue;
            cfg->file_type_names[idx] = xstrdup(ft->string);
            parse_file_type(ft, &cfg->file_types[idx]);
            idx++;
        }
        cfg->file_type_count = idx;
    }

    cJSON *maps = cJSON_GetObjectItem(root, "maps");
    if (maps && cJSON_IsObject(maps)) {
        int count = cJSON_GetArraySize(maps);
        if (count > 0) {
            cfg->maps = xcalloc(count, sizeof(EditorMapConfig));
            cJSON *m;
            int idx = 0;
            cJSON_ArrayForEach(m, maps) {
                if (cJSON_IsString(m) && m->string) {
                    cfg->maps[idx].trigger = xstrdup(m->string);
                    cfg->maps[idx].expansion = xstrdup(m->valuestring);
                    idx++;
                }
            }
            cfg->map_count = idx;
        }
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
    for (int i = 0; i < cfg->map_count; i++) {
        free(cfg->maps[i].trigger);
        free(cfg->maps[i].expansion);
    }
    free(cfg->maps);
    free(cfg);
}
