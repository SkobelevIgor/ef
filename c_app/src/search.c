#include "search.h"

#include <stdlib.h>
#include <wctype.h>
#include <string.h>

int find_first_match_after_cursor(const SearchMatch *matches, int match_count,
                                  int cursor_row, int cursor_col) {
    if (!matches || match_count == 0) return -1;

    for (int i = 0; i < match_count; i++) {
        if (matches[i].row > cursor_row ||
            (matches[i].row == cursor_row && matches[i].col >= cursor_col)) {
            return i;
        }
    }
    /* Wrap to first match */
    return 0;
}

/* Case-insensitive wchar_t substring search. Returns index or -1. */
static int wcs_case_find(const wchar_t *haystack, int hay_len,
                         int start, const wchar_t *needle, int needle_len) {
    if (needle_len == 0) return -1;
    for (int i = start; i <= hay_len - needle_len; i++) {
        bool match = true;
        for (int j = 0; j < needle_len; j++) {
            if (towlower(haystack[i + j]) != towlower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return -1;
}

SearchMatch *buffer_find_all_matches(const Buffer *buf, const wchar_t *query,
                                     int query_len, int *out_count) {
    *out_count = 0;
    if (!buf || !query || query_len == 0) return NULL;

    int cap = 32;
    SearchMatch *matches = malloc(sizeof(SearchMatch) * cap);
    int count = 0;

    for (int row = 0; row < buf->line_count; row++) {
        int col = 0;
        while (col <= buf->line_lens[row] - query_len) {
            int idx = wcs_case_find(buf->lines[row], buf->line_lens[row],
                                    col, query, query_len);
            if (idx < 0) break;

            if (count >= cap) {
                cap *= 2;
                matches = realloc(matches, sizeof(SearchMatch) * cap);
            }
            matches[count].row = row;
            matches[count].col = idx;
            matches[count].length = query_len;
            count++;
            col = idx + 1;
        }
    }

    *out_count = count;
    if (count == 0) { free(matches); return NULL; }
    return matches;
}
