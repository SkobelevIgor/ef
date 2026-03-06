#ifndef EF_SYNTAX_H
#define EF_SYNTAX_H

#include "config.h"

#include <stdbool.h>
#include <wchar.h>

/* A syntax token: a highlighted range in a line. */
typedef struct {
    int   start;     /* Start column (inclusive) */
    int   end;       /* End column (exclusive) */
    short fg_color;  /* ncurses color constant, -1 = default */
    bool  bold;
} SyntaxToken;

typedef struct SyntaxHighlighter SyntaxHighlighter;
typedef struct HighlightCache HighlightCache;

/* Create a highlighter from syntax rule configs. */
SyntaxHighlighter *syntax_highlighter_new(const SyntaxRuleConfig *rules,
                                           int rule_count);
void syntax_highlighter_free(SyntaxHighlighter *h);

/* Highlight a single line. Caller must free returned array. */
SyntaxToken *syntax_highlight_line(SyntaxHighlighter *h,
                                    const wchar_t *line, int line_len,
                                    int *out_count);

/* Create a highlight cache (takes ownership of highlighter). */
HighlightCache *highlight_cache_new(SyntaxHighlighter *h);
void highlight_cache_free(HighlightCache *c);

/* Get tokens for a line (cached). Returned pointer owned by cache. */
const SyntaxToken *highlight_cache_get_tokens(HighlightCache *c,
                                               int line_idx,
                                               const wchar_t *line,
                                               int line_len,
                                               int *out_count);

void highlight_cache_invalidate(HighlightCache *c, int line_idx);
void highlight_cache_invalidate_all(HighlightCache *c);

/* Find token covering the given column. Returns NULL if none. */
const SyntaxToken *syntax_token_at(const SyntaxToken *tokens, int count,
                                    int col);

/* Parse a color name to ncurses color constant. Returns -1 if unknown. */
short syntax_parse_color(const char *name);

#endif /* EF_SYNTAX_H */
