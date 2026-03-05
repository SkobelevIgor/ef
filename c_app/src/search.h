#ifndef EF_SEARCH_H
#define EF_SEARCH_H

#include "buffer.h"

/* Find first match at or after cursor position. Returns -1 if no matches. */
int find_first_match_after_cursor(const SearchMatch *matches, int match_count,
                                  int cursor_row, int cursor_col);

/* Find all case-insensitive matches of query in buffer. Caller must free result. */
SearchMatch *buffer_find_all_matches(const Buffer *buf, const wchar_t *query,
                                     int query_len, int *out_count);

#endif /* EF_SEARCH_H */
