#include "clipboard.h"
#include "buffer.h"

#include <stdlib.h>

Clipboard *clipboard_new(void) {
    return calloc(1, sizeof(Clipboard));
}

void clipboard_free(Clipboard *cb) {
    if (!cb) return;
    clipboard_clear(cb);
    free(cb);
}

void clipboard_set(Clipboard *cb, wchar_t **lines, const int *lens,
                   int count, bool line_mode) {
    clipboard_clear(cb);
    cb->lines = buffer_copy_lines(lines, lens, count, &cb->line_lens);
    cb->line_count = count;
    cb->is_line_mode = line_mode;
}

void clipboard_clear(Clipboard *cb) {
    buffer_free_lines(cb->lines, cb->line_lens, cb->line_count);
    cb->lines = NULL;
    cb->line_lens = NULL;
    cb->line_count = 0;
    cb->is_line_mode = false;
}
