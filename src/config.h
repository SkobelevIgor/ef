#ifndef EF_CONFIG_H
#define EF_CONFIG_H

#include <stdbool.h>

/* Style configuration for a syntax rule. */
typedef struct {
    char *color;   /* Color name: "red", "green", "blue", "yellow", etc. */
    bool  bold;
} SyntaxStyleConfig;

/* A single syntax highlighting rule from config. */
typedef struct {
    char             *pattern;  /* PCRE2 regex pattern */
    SyntaxStyleConfig style;
    int               priority;
} SyntaxRuleConfig;

/* Per-file-type configuration. */
typedef struct {
    char **extensions;
    int    ext_count;
    bool   syntax_highlighting;
    int    tab_stop;
    int    shift_width;
    bool   auto_indentation;
    bool   expand_tab;
    SyntaxRuleConfig *syntax_rules;
    int               rule_count;
} EditorFileTypeConfig;

/* Top-level editor configuration loaded from ~/.efconfig. */
typedef struct {
    EditorFileTypeConfig *file_types;
    char                **file_type_names;
    int                   file_type_count;
} EditorConfig;

/* Write default config to path if it doesn't exist (NULL = ~/.efconfig).
   Returns true if file was created, false if it already existed or on error. */
bool config_ensure_default(const char *path);

/* Load config from path (NULL = ~/.efconfig). Returns NULL on failure. */
EditorConfig *config_load(const char *path);

/* Free config. */
void config_free(EditorConfig *cfg);

/* Detect file type name from filename extension. Returns NULL if unknown. */
const char *config_detect_file_type(const EditorConfig *cfg, const char *filename);

/* Get file type config by name. Returns NULL if not found. */
const EditorFileTypeConfig *config_get_file_type(const EditorConfig *cfg,
                                                  const char *name);

#endif /* EF_CONFIG_H */
