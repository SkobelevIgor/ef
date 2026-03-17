#include "editor.h"
#include "xalloc.h"
#include "runes.h"

#include <stdlib.h>
#include <string.h>

static void paste_lines_mode(Editor *ed, bool before) {
    Clipboard *cb = ed->clipboard;
    Pane *pane = editor_active_pane(ed);
    for (int i = cb->line_count - 1; i >= 0; i--) {
        int len = cb->line_lens[i];
        wchar_t *line = xmalloc(sizeof(wchar_t) * (len + 1));
        wmemcpy(line, cb->lines[i], len);
        line[len] = L'\0';
        if (before) buffer_insert_line_before(pane->buffer, pane->cursor_row, line, len);
        else        buffer_insert_line_after(pane->buffer, pane->cursor_row, line, len);
    }
    if (!before) pane->cursor_row++;
    pane->cursor_col = 0;
}

static void paste_single_line(Editor *ed, int insert_pos) {
    Clipboard *cb = ed->clipboard;
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int new_len;
    wchar_t *nl = insert_runes(buf->lines[pane->cursor_row],
                               buf->line_lens[pane->cursor_row],
                               insert_pos, cb->lines[0], cb->line_lens[0],
                               &new_len);
    buffer_set_line(buf, pane->cursor_row, nl, new_len);
    pane->cursor_col = insert_pos + cb->line_lens[0] - 1;
    if (pane->cursor_col < 0) pane->cursor_col = 0;
    buf->modified = true;
}

static void paste_multiline(Editor *ed, int insert_pos) {
    Clipboard *cb = ed->clipboard;
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    wchar_t *line = buf->lines[pane->cursor_row];
    int line_len = buf->line_lens[pane->cursor_row];

    /* First part: prefix of current line + first clipboard line */
    int first_len = insert_pos + cb->line_lens[0];
    wchar_t *first = xmalloc(sizeof(wchar_t) * (first_len + 1));
    wmemcpy(first, line, insert_pos);
    wmemcpy(first + insert_pos, cb->lines[0], cb->line_lens[0]);
    first[first_len] = L'\0';

    /* Last part: last clipboard line + suffix of current line */
    int last_cb_len = cb->line_lens[cb->line_count - 1];
    int suffix_len = line_len - insert_pos;
    int last_len = last_cb_len + suffix_len;
    wchar_t *last = xmalloc(sizeof(wchar_t) * (last_len + 1));
    wmemcpy(last, cb->lines[cb->line_count - 1], last_cb_len);
    wmemcpy(last + last_cb_len, line + insert_pos, suffix_len);
    last[last_len] = L'\0';

    /* Replace current line with first part */
    buffer_set_line(buf, pane->cursor_row, first, first_len);

    /* Insert middle clipboard lines */
    for (int i = cb->line_count - 2; i >= 1; i--) {
        int ml = cb->line_lens[i];
        wchar_t *m = xmalloc(sizeof(wchar_t) * (ml + 1));
        wmemcpy(m, cb->lines[i], ml);
        m[ml] = L'\0';
        buffer_insert_line_after(buf, pane->cursor_row, m, ml);
    }

    /* Insert last part */
    int last_row = pane->cursor_row + cb->line_count - 1;
    buffer_insert_line_after(buf, pane->cursor_row + cb->line_count - 2,
                             last, last_len);

    pane->cursor_row = last_row;
    pane->cursor_col = last_cb_len > 0 ? last_cb_len - 1 : 0;
    buf->modified = true;
}

static void perform_paste(Editor *ed, bool before) {
    Clipboard *cb = ed->clipboard;
    if (cb->line_count == 0) return;

    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;

    int *old_lens;
    wchar_t **old_lines = buffer_copy_lines(buf->lines, buf->line_lens,
                                            buf->line_count, &old_lens);
    int old_count = buf->line_count;

    if (cb->is_line_mode) {
        paste_lines_mode(ed, before);
    } else {
        int insert_pos = before ? pane->cursor_col : pane->cursor_col + 1;
        if (!before && insert_pos > buf->line_lens[pane->cursor_row])
            insert_pos = buf->line_lens[pane->cursor_row];
        if (cb->line_count == 1) paste_single_line(ed, insert_pos);
        else                     paste_multiline(ed, insert_pos);
    }

    Change *c = change_new(CHANGE_REPLACE, buf, pane->cursor_row, pane->cursor_col);
    c->text = buffer_copy_lines(buf->lines, buf->line_lens, buf->line_count, &c->text_lens);
    c->text_count = buf->line_count;
    c->old_text = old_lines;
    c->old_text_lens = old_lens;
    c->old_text_count = old_count;
    history_push(ed->history, c);
    editor_schedule_auto_save(ed);
}

void editor_paste_after(Editor *ed)  { perform_paste(ed, false); }
void editor_paste_before(Editor *ed) { perform_paste(ed, true); }
