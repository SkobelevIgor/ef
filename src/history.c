#include "history.h"
#include "xalloc.h"

#include <stdlib.h>
#include <string.h>

/* --- Change -------------------------------------------------------------- */

Change *change_new(ChangeType type, Buffer *buf, int row, int col) {
    Change *c = xcalloc(1, sizeof(Change));
    c->type = type;
    c->buffer = buf;
    c->row = row;
    c->col = col;
    return c;
}

void change_free(Change *c) {
    if (!c) return;
    buffer_free_lines(c->text, c->text_lens, c->text_count);
    buffer_free_lines(c->old_text, c->old_text_lens, c->old_text_count);
    free(c);
}

/* --- History ------------------------------------------------------------- */

static void clear_stack(Change **stack, int count) {
    for (int i = 0; i < count; i++) {
        change_free(stack[i]);
    }
}

History *history_new(int max_size) {
    if (max_size <= 0) max_size = DEFAULT_HISTORY_SIZE;
    History *h = xcalloc(1, sizeof(History));
    h->max_size = max_size;
    return h;
}

void history_free(History *h) {
    if (!h) return;
    clear_stack(h->undo_stack, h->undo_count);
    free(h->undo_stack);
    clear_stack(h->redo_stack, h->redo_count);
    free(h->redo_stack);
    buffer_free_lines(h->session_lines, h->session_line_lens,
                      h->session_line_count);
    change_free(h->pending);
    free(h);
}

static void stack_push(Change ***stack, int *count, int *cap, Change *c) {
    if (*count >= *cap) {
        int new_cap = (*cap == 0) ? 16 : *cap * 2;
        *stack = xrealloc(*stack, sizeof(Change *) * new_cap);
        *cap = new_cap;
    }
    (*stack)[(*count)++] = c;
}

static Change *stack_pop(Change ***stack, int *count) {
    if (*count == 0) return NULL;
    return (*stack)[--(*count)];
}

void history_push(History *h, Change *c) {
    if (!c) return;

    /* Clear redo stack */
    clear_stack(h->redo_stack, h->redo_count);
    h->redo_count = 0;

    if (h->in_session && h->pending) {
        change_free(h->pending);
        h->pending = c;
        return;
    }

    stack_push(&h->undo_stack, &h->undo_count, &h->undo_cap, c);

    /* Trim to max size */
    if (h->undo_count > h->max_size) {
        int excess = h->undo_count - h->max_size;
        for (int i = 0; i < excess; i++) {
            change_free(h->undo_stack[i]);
        }
        memmove(h->undo_stack, h->undo_stack + excess,
                sizeof(Change *) * h->max_size);
        h->undo_count = h->max_size;
    }
}

Change *history_undo(History *h) {
    Change *c = stack_pop(&h->undo_stack, &h->undo_count);
    if (!c) return NULL;
    stack_push(&h->redo_stack, &h->redo_count, &h->redo_cap, c);
    return c;
}

Change *history_redo(History *h) {
    Change *c = stack_pop(&h->redo_stack, &h->redo_count);
    if (!c) return NULL;
    stack_push(&h->undo_stack, &h->undo_count, &h->undo_cap, c);
    return c;
}

bool history_can_undo(const History *h) { return h->undo_count > 0; }
bool history_can_redo(const History *h) { return h->redo_count > 0; }

void history_start_session(History *h, Buffer *buf, int row, int col) {
    h->in_session = true;
    change_free(h->pending);
    h->pending = NULL;
    h->session_buffer = buf;
    h->session_start_row = row;
    h->session_start_col = col;
    buffer_free_lines(h->session_lines, h->session_line_lens,
                      h->session_line_count);
    h->session_lines = buffer_copy_lines(buf->lines, buf->line_lens,
                                         buf->line_count,
                                         &h->session_line_lens);
    h->session_line_count = buf->line_count;
}

void history_commit_session(History *h, wchar_t **current_lines,
                            const int *current_lens, int current_count) {
    if (!h->in_session || !h->session_lines || !h->session_buffer) goto done;

    bool changed = (h->session_line_count != current_count);
    if (!changed) {
        for (int i = 0; i < current_count; i++) {
            if (h->session_line_lens[i] != current_lens[i]) {
                changed = true; break;
            }
            if (wmemcmp(h->session_lines[i], current_lines[i],
                        current_lens[i]) != 0) {
                changed = true; break;
            }
        }
    }

    if (changed) {
        Change *c = change_new(CHANGE_REPLACE, h->session_buffer,
                               h->session_start_row, h->session_start_col);
        c->text = buffer_copy_lines(current_lines, current_lens,
                                    current_count, &c->text_lens);
        c->text_count = current_count;
        c->old_text = h->session_lines;
        c->old_text_lens = h->session_line_lens;
        c->old_text_count = h->session_line_count;
        h->session_lines = NULL;
        h->session_line_lens = NULL;
        h->session_line_count = 0;
        history_push(h, c);
    }

done:
    h->in_session = false;
    h->session_buffer = NULL;
    buffer_free_lines(h->session_lines, h->session_line_lens,
                      h->session_line_count);
    h->session_lines = NULL;
    h->session_line_lens = NULL;
    h->session_line_count = 0;
}

void history_record_insert(History *h, Buffer *buf, int row, int col,
                           wchar_t **text, const int *text_lens, int text_count) {
    Change *c = change_new(CHANGE_INSERT, buf, row, col);
    c->text = buffer_copy_lines(text, text_lens, text_count, &c->text_lens);
    c->text_count = text_count;
    history_push(h, c);
}

void history_record_delete(History *h, Buffer *buf, int row, int col,
                           wchar_t **text, const int *text_lens, int text_count) {
    Change *c = change_new(CHANGE_DELETE, buf, row, col);
    c->text = buffer_copy_lines(text, text_lens, text_count, &c->text_lens);
    c->text_count = text_count;
    history_push(h, c);
}

void history_record_delete_lines(History *h, Buffer *buf, int row,
                                 wchar_t **lines, const int *lens, int count) {
    Change *c = change_new(CHANGE_DELETE, buf, row, 0);
    c->text = buffer_copy_lines(lines, lens, count, &c->text_lens);
    c->text_count = count;
    c->line_deletion = true;
    history_push(h, c);
}
