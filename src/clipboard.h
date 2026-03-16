#ifndef EF_CLIPBOARD_H
#define EF_CLIPBOARD_H

#include <stdbool.h>
#include <wchar.h>

/* Clipboard stores yanked (copied) data. */
typedef struct {
    wchar_t **lines;
    int      *line_lens;
    int       line_count;
    bool      is_line_mode; /* true = whole lines (yy), false = char mode */
} Clipboard;

Clipboard *clipboard_new(void);
void clipboard_free(Clipboard *cb);
void clipboard_set(Clipboard *cb, wchar_t **lines, const int *lens,
                   int count, bool line_mode);
void clipboard_clear(Clipboard *cb);

#endif /* EF_CLIPBOARD_H */
