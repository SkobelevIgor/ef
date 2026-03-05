#include "runes.h"

#include <stdlib.h>
#include <string.h>
#include <wctype.h>

int visual_column(const wchar_t *line, int line_len, int char_col, int tab_stop) {
    if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;
    int vis = 0;
    int limit = char_col < line_len ? char_col : line_len;
    for (int i = 0; i < limit; i++) {
        if (line[i] == L'\t') {
            vis += tab_stop - (vis % tab_stop);
        } else {
            vis++;
        }
    }
    return vis;
}

int visual_line_width(const wchar_t *line, int line_len, int tab_stop) {
    return visual_column(line, line_len, line_len, tab_stop);
}

void normalize_range(int *sr, int *sc, int *er, int *ec) {
    if (*sr > *er || (*sr == *er && *sc > *ec)) {
        int tmp;
        tmp = *sr; *sr = *er; *er = tmp;
        tmp = *sc; *sc = *ec; *ec = tmp;
    }
}

void clamp_position(wchar_t **lines, int line_count,
                    int *row, int *col, const int *line_lens) {
    if (line_count == 0) {
        *row = 0; *col = 0;
        return;
    }
    if (*row < 0) *row = 0;
    if (*row >= line_count) *row = line_count - 1;
    int ll = line_lens[*row];
    if (*col > ll) *col = ll;
    if (*col < 0) *col = 0;
}

bool is_whitespace(wchar_t ch) {
    return ch == L' ' || ch == L'\t';
}

bool is_word_char(wchar_t ch) {
    return iswalnum((wint_t)ch) || ch == L'_';
}

wchar_t *insert_runes(const wchar_t *line, int line_len, int pos,
                      const wchar_t *text, int text_len, int *out_len) {
    int new_len = line_len + text_len;
    wchar_t *result = malloc(sizeof(wchar_t) * (new_len + 1));
    if (!result) return NULL;
    wmemcpy(result, line, pos);
    wmemcpy(result + pos, text, text_len);
    wmemcpy(result + pos + text_len, line + pos, line_len - pos);
    result[new_len] = L'\0';
    if (out_len) *out_len = new_len;
    return result;
}

wchar_t *remove_runes(const wchar_t *line, int line_len,
                      int start, int end, int *out_len) {
    int removed = end - start;
    int new_len = line_len - removed;
    wchar_t *result = malloc(sizeof(wchar_t) * (new_len + 1));
    if (!result) return NULL;
    wmemcpy(result, line, start);
    wmemcpy(result + start, line + end, line_len - end);
    result[new_len] = L'\0';
    if (out_len) *out_len = new_len;
    return result;
}
