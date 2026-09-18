#include "editor.h"
#include "xalloc.h"
#include "runes.h"

#include <stdlib.h>
#include <string.h>

static void apply_remove_text(Buffer *buf, Change *c) {
    if (c->line_deletion) {
        int end = c->row + c->text_count;
        if (end <= buf->line_count) {
            buffer_splice_lines(buf, c->row, end - c->row, NULL, NULL, 0);
        }
        if (buf->line_count == 0) {
            buffer_ensure_lines(buf, 1);
            buf->lines[0] = xwcsdup(L"", 0);
            buf->line_lens[0] = 0;
            buf->line_caps[0] = 1;
            buf->line_count = 1;
        }
        return;
    }
    if (c->text_count == 1) {
        int tl = c->text_lens[0];
        if (c->col + tl <= buf->line_lens[c->row]) {
            int new_len;
            wchar_t *nl = remove_runes(buf->lines[c->row], buf->line_lens[c->row],
                                       c->col, c->col + tl, &new_len);
            buffer_set_line(buf, c->row, nl, new_len);
        }
    } else {
        int end_row = c->row + c->text_count - 1;
        if (end_row < buf->line_count) {
            int last_text_len = c->text_lens[c->text_count - 1];
            int after_len = buf->line_lens[end_row] - last_text_len;
            if (after_len < 0) after_len = 0;
            int first_len = c->col;
            int merged_len = first_len + after_len;
            wchar_t *merged = xmalloc(sizeof(wchar_t) * (merged_len + 1));
            wmemcpy(merged, buf->lines[c->row], first_len);
            wmemcpy(merged + first_len,
                    buf->lines[end_row] + last_text_len, after_len);
            merged[merged_len] = L'\0';
            buffer_splice_lines(buf, c->row, end_row - c->row + 1,
                                &merged, &merged_len, 1);
        }
    }
}

static void apply_insert_text(Buffer *buf, Change *c) {
    if (c->line_deletion) {
        int *lens;
        wchar_t **lines = buffer_copy_lines(c->text, c->text_lens,
                                            c->text_count, &lens);
        buffer_splice_lines(buf, c->row, 0, lines, lens, c->text_count);
        free(lens);
        free(lines);
        return;
    }
    if (c->text_count == 1) {
        int new_len;
        wchar_t *nl = insert_runes(buf->lines[c->row], buf->line_lens[c->row],
                                   c->col, c->text[0], c->text_lens[0], &new_len);
        buffer_set_line(buf, c->row, nl, new_len);
    } else {
        int first_part_len = c->col;
        int rest_len = buf->line_lens[c->row] - c->col;
        if (rest_len < 0) rest_len = 0;

        int new_first_len = first_part_len + c->text_lens[0];
        wchar_t *new_first = xmalloc(sizeof(wchar_t) * (new_first_len + 1));
        wmemcpy(new_first, buf->lines[c->row], first_part_len);
        wmemcpy(new_first + first_part_len, c->text[0], c->text_lens[0]);
        new_first[new_first_len] = L'\0';

        int last_text_len = c->text_lens[c->text_count - 1];
        int new_last_len = last_text_len + rest_len;
        wchar_t *new_last = xmalloc(sizeof(wchar_t) * (new_last_len + 1));
        wmemcpy(new_last, c->text[c->text_count - 1], last_text_len);
        wmemcpy(new_last + last_text_len,
                buf->lines[c->row] + c->col, rest_len);
        new_last[new_last_len] = L'\0';

        int block_count = c->text_count;
        wchar_t **block = xmalloc(sizeof(wchar_t *) * block_count);
        int *block_lens = xmalloc(sizeof(int) * block_count);
        block[0] = new_first;
        block_lens[0] = new_first_len;
        for (int i = 1; i < block_count - 1; i++) {
            block[i] = xmalloc(sizeof(wchar_t) * (c->text_lens[i] + 1));
            wmemcpy(block[i], c->text[i], c->text_lens[i]);
            block[i][c->text_lens[i]] = L'\0';
            block_lens[i] = c->text_lens[i];
        }
        block[block_count - 1] = new_last;
        block_lens[block_count - 1] = new_last_len;

        buffer_splice_lines(buf, c->row, 1, block, block_lens, block_count);
        free(block);
        free(block_lens);
    }
}

static Pane *pane_for_buffer(Editor *ed, Buffer *buf) {
    Pane *active = editor_active_pane(ed);
    if (active->buffer == buf) return active;
    for (int i = 0; i < ed->pane_count; i++) {
        if (ed->panes[i]->buffer == buf) return ed->panes[i];
    }
    return NULL;
}

static void place_cursor(Pane *pane, int row, int col) {
    if (!pane) return;
    pane->cursor_row = row;
    pane->cursor_col = col;
    pane_clamp_cursor(pane);
}

void editor_undo(Editor *ed) {
    if (ed->read_only) return;
    Change *c = history_undo(ed->history);
    if (!c) return;

    Buffer *buf = c->buffer ? c->buffer : editor_active_buffer(ed);
    Pane *pane = pane_for_buffer(ed, buf);

    switch (c->type) {
    case CHANGE_INSERT:
        apply_remove_text(buf, c);
        place_cursor(pane, c->row, c->col);
        buf->modified = true;
        break;
    case CHANGE_DELETE:
        apply_insert_text(buf, c);
        place_cursor(pane, c->row, c->col);
        buf->modified = true;
        break;
    case CHANGE_REPLACE:
        if (c->old_text) {
            int *lens;
            wchar_t **lines = buffer_copy_lines(c->old_text, c->old_text_lens,
                                                c->old_text_count, &lens);
            buffer_replace_all(buf, lines, lens, c->old_text_count);
            free(lines);
            free(lens);
        }
        place_cursor(pane, c->row, c->col);
        buf->modified = true;
        break;
    }

    editor_schedule_auto_save(ed);
}

void editor_redo(Editor *ed) {
    if (ed->read_only) return;
    Change *c = history_redo(ed->history);
    if (!c) return;

    Buffer *buf = c->buffer ? c->buffer : editor_active_buffer(ed);
    Pane *pane = pane_for_buffer(ed, buf);

    switch (c->type) {
    case CHANGE_INSERT:
        apply_insert_text(buf, c);
        if (c->text_count == 1) {
            place_cursor(pane, c->row, c->col + c->text_lens[0]);
        } else {
            place_cursor(pane, c->row + c->text_count - 1,
                         c->text_lens[c->text_count - 1]);
        }
        buf->modified = true;
        break;
    case CHANGE_DELETE:
        apply_remove_text(buf, c);
        place_cursor(pane, c->row, c->col);
        buf->modified = true;
        break;
    case CHANGE_REPLACE:
        if (c->text) {
            int *lens;
            wchar_t **lines = buffer_copy_lines(c->text, c->text_lens,
                                                c->text_count, &lens);
            buffer_replace_all(buf, lines, lens, c->text_count);
            free(lines);
            free(lens);
        }
        place_cursor(pane, c->row, c->col);
        buf->modified = true;
        break;
    }

    editor_schedule_auto_save(ed);
}
