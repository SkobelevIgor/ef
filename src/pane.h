#ifndef EF_PANE_H
#define EF_PANE_H

#include <stdbool.h>
#include "buffer.h"

/* Forward declaration */
typedef struct WidgetState WidgetState;

/* Pane represents a visual view of a buffer with independent navigation state. */
typedef struct Pane {
    Buffer *buffer;

    int cursor_row;
    int cursor_col;
    int scroll_offset;
    int scroll_wrap;  /* Wrap rows of line scroll_offset hidden above the view */

    bool selection_active;
    int  selection_start_row;
    int  selection_start_col;

    WidgetState *widget;  /* Per-pane widget state (NULL when no widget) */
} Pane;

/* Check if pane has an active widget */
bool pane_has_active_widget(const Pane *p);

Pane *pane_new(Buffer *buf);
Pane *pane_new_at_line(Buffer *buf, int line);
void  pane_free(Pane *p);

void pane_goto_line(Pane *p, int line);
void pane_clamp_cursor(Pane *p);
void pane_clamp_cursor_col(Pane *p);

void pane_adjust_cursor_for_edit(Pane *p, int edit_row, int lines_delta);
void pane_adjust_scroll(Pane *p, int text_width, int screen_height);

void pane_start_selection(Pane *p);
void pane_clear_selection(Pane *p);
void pane_get_selection(const Pane *p, int *sr, int *sc, int *er, int *ec);
bool pane_is_in_selection(const Pane *p, int row, int col);

/* Movement */
void pane_move_up(Pane *p);
void pane_move_down(Pane *p);
void pane_move_left(Pane *p);
void pane_move_right(Pane *p);
void pane_move_up_n(Pane *p, int n);
void pane_move_down_n(Pane *p, int n);
void pane_move_left_n(Pane *p, int n);
void pane_move_right_n(Pane *p, int n);
void pane_move_to_line_start(Pane *p);
void pane_move_to_line_end(Pane *p);
void pane_page_down(Pane *p, int height);
void pane_page_up(Pane *p, int height);
void pane_move_to_next_word(Pane *p);
void pane_move_to_prev_word(Pane *p);
void pane_move_to_word_end(Pane *p);
void pane_find_char_forward(Pane *p, wchar_t ch);
void pane_find_char_backward(Pane *p, wchar_t ch);

#endif /* EF_PANE_H */
