#include "input.h"
#include "pane.h"
#include "screen.h"
#include "autocomplete.h"

#include <stdlib.h>
#include <string.h>

InputState *input_state_new(void) {
    InputState *s = calloc(1, sizeof(InputState));
    return s;
}

void input_state_free(InputState *s) {
    if (!s) return;
    ac_state_free(s->autocomplete);
    free(s);
}

void input_state_reset(InputState *s) {
    s->count = 0;
    s->has_count = false;
    s->pending_find_forward = false;
    s->pending_find_backward = false;
    s->pending_goto_line = false;
    s->goto_line_buffer[0] = '\0';
    s->goto_line_buf_len = 0;
    s->pending_operator = 0;
    s->pending_mark = false;
    s->pending_jump_to_mark = false;
    s->map_buf_len = 0;
}

int input_state_get_count(const InputState *s) {
    if (s->has_count) return s->count;
    return 1;
}

void input_state_add_digit(InputState *s, int d) {
    s->count = s->count * 10 + d;
    s->has_count = true;
}

bool input_state_has_pending(const InputState *s) {
    return s->pending_find_forward || s->pending_find_backward
        || s->pending_goto_line || s->pending_operator != 0
        || s->pending_mark || s->pending_jump_to_mark;
}

void input_state_save_last_find(InputState *s, wchar_t ch, bool forward) {
    s->last_find_char = ch;
    s->last_find_forward = forward;
    s->has_last_find = true;
}

void input_state_map_push(InputState *s, wchar_t ch) {
    if (s->map_buf_len < MAP_BUF_SIZE) {
        s->map_buf[s->map_buf_len++] = ch;
    } else {
        /* Shift left to make room */
        for (int i = 0; i < MAP_BUF_SIZE - 1; i++)
            s->map_buf[i] = s->map_buf[i + 1];
        s->map_buf[MAP_BUF_SIZE - 1] = ch;
    }
}

void input_state_map_clear(InputState *s) {
    s->map_buf_len = 0;
}

bool input_handle_find_char(InputState *is, Pane *pane,
                            void *ev_ptr, int count) {
    EditorEvent *ev = (EditorEvent *)ev_ptr;
    if (ev->is_char && ev->ch == 27) {
        input_state_reset(is);
        return false;
    }
    if (ev->is_char) {
        wchar_t ch = ev->ch;
        bool forward = is->pending_find_forward;
        for (int i = 0; i < count; i++) {
            if (forward) pane_find_char_forward(pane, ch);
            else pane_find_char_backward(pane, ch);
        }
        input_state_save_last_find(is, ch, forward);
        input_state_reset(is);
    }
    return false;
}

bool input_handle_goto_line(InputState *is, Pane *pane, void *ev_ptr) {
    EditorEvent *ev = (EditorEvent *)ev_ptr;
    if (ev->is_char && ev->ch == 27) {
        input_state_reset(is);
        return false;
    }
    if (ev->is_char && (ev->ch == L'\n' || ev->ch == L'\r')) {
        if (is->goto_line_buf_len > 0) {
            if (strcmp(is->goto_line_buffer, "0") == 0) {
                pane_goto_line(pane, 1);
            } else if (is->goto_line_buffer[0] == '$') {
                pane_goto_line(pane, pane->buffer->line_count);
            } else {
                int line_num = atoi(is->goto_line_buffer);
                if (line_num > 0) pane_goto_line(pane, line_num);
            }
        }
        input_state_reset(is);
        return false;
    }
    if (ev->is_char && (ev->ch == 127 || ev->ch == 8)) {
        if (is->goto_line_buf_len > 0) {
            is->goto_line_buffer[--is->goto_line_buf_len] = '\0';
            if (is->goto_line_buf_len == 0) {
                input_state_reset(is);
            }
        }
        return false;
    }
    if (ev->is_char) {
        wchar_t ch = ev->ch;
        if ((ch >= L'0' && ch <= L'9') || ch == L'$') {
            if (is->goto_line_buf_len < 62) {
                is->goto_line_buffer[is->goto_line_buf_len++] = (char)ch;
                is->goto_line_buffer[is->goto_line_buf_len] = '\0';
            }
        }
    }
    return false;
}
