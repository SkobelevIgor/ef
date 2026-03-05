#include "input.h"
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
