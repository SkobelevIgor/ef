#ifndef EF_RUNES_H
#define EF_RUNES_H

#include <wchar.h>
#include <stdbool.h>

#include "constants.h"

/* VisualColumn calculates the visual column position accounting for tab expansion. */
int visual_column(const wchar_t *line, int line_len, int char_col, int tab_stop);

/* VisualLineWidth calculates the visual width of an entire line accounting for tabs. */
int visual_line_width(const wchar_t *line, int line_len, int tab_stop);

/* NormalizeRange ensures start position is before end position. */
void normalize_range(int *start_row, int *start_col, int *end_row, int *end_col);

/* ClampPosition clamps row and col to valid buffer bounds. */
void clamp_position(wchar_t **lines, int line_count, int *row, int *col, const int *line_lens);

/* isWhitespace returns true for space and tab characters. */
bool is_whitespace(wchar_t ch);

/* isWordChar returns true for alphanumeric and underscore characters. */
bool is_word_char(wchar_t ch);

/* insertRunes inserts text into line at the given position.
   Returns new line (caller owns). Sets *out_len to new length. */
wchar_t *insert_runes(const wchar_t *line, int line_len, int pos,
                      const wchar_t *text, int text_len, int *out_len);

/* removeRunes removes the range [start, end) from line.
   Returns new line (caller owns). Sets *out_len to new length. */
wchar_t *remove_runes(const wchar_t *line, int line_len, int start, int end, int *out_len);

#endif /* EF_RUNES_H */
