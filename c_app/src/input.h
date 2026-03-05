#ifndef EF_INPUT_H
#define EF_INPUT_H

#include <stdbool.h>
#include <wchar.h>

/* Forward declaration */
typedef struct AutocompleteState AutocompleteState;

/* InputState tracks multi-key sequence state. */
typedef struct {
    int  count;
    bool has_count;

    bool pending_find_forward;
    bool pending_find_backward;

    bool pending_goto_line;
    char goto_line_buffer[64];
    int  goto_line_buf_len;

    wchar_t last_find_char;
    bool    last_find_forward;
    bool    has_last_find;

    wchar_t pending_operator;

    bool pending_mark;
    bool pending_jump_to_mark;

    /* Autocomplete state (NULL when not active) */
    AutocompleteState *autocomplete;
} InputState;

InputState *input_state_new(void);
void input_state_free(InputState *s);
void input_state_reset(InputState *s);
int  input_state_get_count(const InputState *s);
void input_state_add_digit(InputState *s, int d);
bool input_state_has_pending(const InputState *s);
void input_state_save_last_find(InputState *s, wchar_t ch, bool forward);

#endif /* EF_INPUT_H */
