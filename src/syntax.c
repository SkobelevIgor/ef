#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "syntax.h"
#include "xalloc.h"

#include <limits.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* --- Color name parsing -------------------------------------------------- */

short syntax_parse_color(const char *name) {
    if (!name || !*name) return -1;
    if (strcasecmp(name, "black") == 0)   return COLOR_BLACK;
    if (strcasecmp(name, "red") == 0)     return COLOR_RED;
    if (strcasecmp(name, "green") == 0)   return COLOR_GREEN;
    if (strcasecmp(name, "yellow") == 0)  return COLOR_YELLOW;
    if (strcasecmp(name, "blue") == 0)    return COLOR_BLUE;
    if (strcasecmp(name, "magenta") == 0) return COLOR_MAGENTA;
    if (strcasecmp(name, "purple") == 0)  return COLOR_MAGENTA;
    if (strcasecmp(name, "cyan") == 0)    return COLOR_CYAN;
    if (strcasecmp(name, "white") == 0)   return COLOR_WHITE;
    if (strcasecmp(name, "gray") == 0)    return 8;
    if (strcasecmp(name, "grey") == 0)    return 8;
    if (strcasecmp(name, "orange") == 0)  return 208;
    if (strcasecmp(name, "pink") == 0)    return 205;
    if (strcasecmp(name, "teal") == 0)    return 6;
    return -1;
}

/* --- UTF-8 conversion with offset mapping -------------------------------- */

static char *wcs_to_utf8_mapped(const wchar_t *line, int line_len,
                                 int *out_byte_len, int **out_b2c) {
    size_t max_bytes = (size_t)line_len * 4 + 1;
    char *utf8 = xmalloc(max_bytes);
    int *b2c = xmalloc((max_bytes + 1) * sizeof(int));
    int pos = 0;

    for (int i = 0; i < line_len; i++) {
        unsigned int ch = (unsigned int)line[i];
        int start = pos;
        if (ch < 0x80) {
            utf8[pos++] = (char)ch;
        } else if (ch < 0x800) {
            utf8[pos++] = (char)(0xC0 | (ch >> 6));
            utf8[pos++] = (char)(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            utf8[pos++] = (char)(0xE0 | (ch >> 12));
            utf8[pos++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            utf8[pos++] = (char)(0x80 | (ch & 0x3F));
        } else {
            utf8[pos++] = (char)(0xF0 | (ch >> 18));
            utf8[pos++] = (char)(0x80 | ((ch >> 12) & 0x3F));
            utf8[pos++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            utf8[pos++] = (char)(0x80 | (ch & 0x3F));
        }
        for (int j = start; j < pos; j++) b2c[j] = i;
    }
    utf8[pos] = '\0';
    b2c[pos] = line_len; /* sentinel */
    *out_byte_len = pos;
    *out_b2c = b2c;
    return utf8;
}

/* --- Compiled rule ------------------------------------------------------- */

typedef struct {
    pcre2_code *code;
    short       fg_color;
    bool        bold;
    int         priority;
} CompiledRule;

struct SyntaxHighlighter {
    CompiledRule *rules;
    int           rule_count;
};

/* --- Highlighter lifecycle ----------------------------------------------- */

SyntaxHighlighter *syntax_highlighter_new(const SyntaxRuleConfig *rules,
                                           int rule_count) {
    SyntaxHighlighter *h = xcalloc(1, sizeof(SyntaxHighlighter));
    if (rule_count > 0)
        h->rules = xcalloc(rule_count, sizeof(CompiledRule));

    for (int i = 0; i < rule_count; i++) {
        int errorcode;
        PCRE2_SIZE erroroffset;
        pcre2_code *code = pcre2_compile(
            (PCRE2_SPTR8)rules[i].pattern,
            PCRE2_ZERO_TERMINATED,
            PCRE2_UTF | PCRE2_UCP,
            &errorcode, &erroroffset, NULL);
        if (!code) continue;

        pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);

        CompiledRule *cr = &h->rules[h->rule_count++];
        cr->code = code;
        cr->fg_color = syntax_parse_color(rules[i].style.color);
        cr->bold = rules[i].style.bold;
        cr->priority = rules[i].priority;
    }
    return h;
}

void syntax_highlighter_free(SyntaxHighlighter *h) {
    if (!h) return;
    for (int i = 0; i < h->rule_count; i++)
        pcre2_code_free(h->rules[i].code);
    free(h->rules);
    free(h);
}

/* --- Line highlighting --------------------------------------------------- */

SyntaxToken *syntax_highlight_line(SyntaxHighlighter *h,
                                    const wchar_t *line, int line_len,
                                    int *out_count) {
    *out_count = 0;
    if (!h || line_len == 0 || h->rule_count == 0) return NULL;

    int byte_len;
    int *b2c;
    char *utf8 = wcs_to_utf8_mapped(line, line_len, &byte_len, &b2c);

    int *rule_indices = xmalloc(line_len * sizeof(int));
    int *priorities = xmalloc(line_len * sizeof(int));
    for (int i = 0; i < line_len; i++) {
        rule_indices[i] = -1;
        priorities[i] = INT_MIN;
    }

    pcre2_match_data *md = pcre2_match_data_create(16, NULL);

    for (int ri = 0; ri < h->rule_count; ri++) {
        PCRE2_SIZE offset = 0;
        while (offset < (PCRE2_SIZE)byte_len) {
            /* The first match of each rule validates the whole line's
               UTF-8; re-checking on every later match is quadratic. */
            uint32_t opts = offset > 0 ? PCRE2_NO_UTF_CHECK : 0;
            int rc = pcre2_match(h->rules[ri].code, (PCRE2_SPTR8)utf8,
                                  byte_len, offset, opts, md, NULL);
            if (rc < 1) break;

            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            int start_char = b2c[ov[0]];
            int end_char = b2c[ov[1]];

            for (int c = start_char; c < end_char && c < line_len; c++) {
                if (h->rules[ri].priority > priorities[c]) {
                    rule_indices[c] = ri;
                    priorities[c] = h->rules[ri].priority;
                }
            }

            offset = ov[1];
            if (ov[1] == ov[0]) {
                offset++;
                while (offset < (PCRE2_SIZE)byte_len
                       && ((unsigned char)utf8[offset] & 0xC0) == 0x80)
                    offset++;
            }
        }
    }

    pcre2_match_data_free(md);
    free(utf8);
    free(b2c);
    free(priorities);

    /* Merge adjacent same-rule tokens */
    SyntaxToken *tokens = NULL;
    int count = 0, cap = 0;

    int current_rule = rule_indices[0];
    int start = 0;
    for (int i = 1; i <= line_len; i++) {
        int ri = (i < line_len) ? rule_indices[i] : -2;
        if (ri != current_rule) {
            if (current_rule >= 0) {
                if (count >= cap) {
                    cap = cap == 0 ? 16 : cap * 2;
                    tokens = xrealloc(tokens, cap * sizeof(SyntaxToken));
                }
                tokens[count++] = (SyntaxToken){
                    start, i,
                    h->rules[current_rule].fg_color,
                    h->rules[current_rule].bold
                };
            }
            current_rule = ri;
            start = i;
        }
    }

    free(rule_indices);
    *out_count = count;
    return tokens;
}

/* --- Token lookup -------------------------------------------------------- */

const SyntaxToken *syntax_token_at(const SyntaxToken *tokens, int count,
                                    int col) {
    for (int i = 0; i < count; i++) {
        if (col < tokens[i].start) return NULL;
        if (col < tokens[i].end) return &tokens[i];
    }
    return NULL;
}

/* --- Highlight cache ----------------------------------------------------- */

typedef struct {
    uint64_t     hash;
    SyntaxToken *tokens;
    int          token_count;
    bool         valid;
} CachedLine;

struct HighlightCache {
    SyntaxHighlighter *highlighter;
    CachedLine        *lines;
    int                capacity;
};

static uint64_t hash_line(const wchar_t *line, int len) {
    uint64_t h = 5381;
    for (int i = 0; i < len; i++)
        h = ((h << 5) + h) + (uint64_t)line[i];
    return h;
}

HighlightCache *highlight_cache_new(SyntaxHighlighter *h) {
    HighlightCache *c = xcalloc(1, sizeof(HighlightCache));
    c->highlighter = h;
    return c;
}

void highlight_cache_free(HighlightCache *c) {
    if (!c) return;
    for (int i = 0; i < c->capacity; i++)
        free(c->lines[i].tokens);
    free(c->lines);
    syntax_highlighter_free(c->highlighter);
    free(c);
}

static void cache_ensure(HighlightCache *c, int idx) {
    if (idx < c->capacity) return;
    int new_cap = c->capacity == 0 ? 256 : c->capacity;
    while (new_cap <= idx) new_cap *= 2;
    c->lines = xrealloc(c->lines, new_cap * sizeof(CachedLine));
    memset(c->lines + c->capacity, 0,
           (new_cap - c->capacity) * sizeof(CachedLine));
    c->capacity = new_cap;
}

const SyntaxToken *highlight_cache_get_tokens(HighlightCache *c,
                                               int line_idx,
                                               const wchar_t *line,
                                               int line_len,
                                               int *out_count) {
    *out_count = 0;
    if (!c || !c->highlighter) return NULL;

    uint64_t h = hash_line(line, line_len);
    cache_ensure(c, line_idx);

    CachedLine *cl = &c->lines[line_idx];
    if (cl->valid && cl->hash == h) {
        *out_count = cl->token_count;
        return cl->tokens;
    }

    free(cl->tokens);
    cl->tokens = syntax_highlight_line(c->highlighter, line, line_len,
                                        &cl->token_count);
    cl->hash = h;
    cl->valid = true;
    *out_count = cl->token_count;
    return cl->tokens;
}
