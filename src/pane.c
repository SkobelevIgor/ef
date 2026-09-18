#include "pane.h"
#include "xalloc.h"
#include "widget.h"
#include "runes.h"

#include <stdlib.h>

Pane *pane_new(Buffer *buf) {
    Pane *p = xcalloc(1, sizeof(Pane));
    p->buffer = buf;
    return p;
}

Pane *pane_new_at_line(Buffer *buf, int line) {
    Pane *p = pane_new(buf);
    if (line > 0) pane_goto_line(p, line);
    return p;
}

void pane_free(Pane *p) {
    if (!p) return;
    widget_state_free(p->widget);
    free(p);
}

bool pane_has_active_widget(const Pane *p) {
    return p && p->widget && p->widget->active;
}

void pane_goto_line(Pane *p, int line) {
    if (line < 1) line = 1;
    if (line > p->buffer->line_count) line = p->buffer->line_count;
    p->cursor_row = line - 1;
    p->cursor_col = 0;
    pane_clamp_cursor(p);
}

void pane_clamp_cursor(Pane *p) {
    if (p->cursor_row < 0) p->cursor_row = 0;
    if (p->cursor_row >= p->buffer->line_count) {
        p->cursor_row = p->buffer->line_count - 1;
    }
    if (p->cursor_row < 0) p->cursor_row = 0;
    pane_clamp_cursor_col(p);
    if (!p->selection_active) return;
    if (p->selection_start_row >= p->buffer->line_count)
        p->selection_start_row = p->buffer->line_count - 1;
    int ll = p->buffer->line_lens[p->selection_start_row];
    if (p->selection_start_col > ll) p->selection_start_col = ll;
}

void pane_clamp_cursor_col(Pane *p) {
    if (p->buffer->line_count == 0) {
        p->cursor_col = 0;
        return;
    }
    int ll = p->buffer->line_lens[p->cursor_row];
    if (p->cursor_col > ll) p->cursor_col = ll;
    if (p->cursor_col < 0) p->cursor_col = 0;
}

void pane_adjust_cursor_for_edit(Pane *p, int edit_row, int lines_delta) {
    if (lines_delta == 0) return;
    if (lines_delta > 0) {
        if (p->cursor_row >= edit_row) {
            p->cursor_row += lines_delta;
        }
    } else {
        int del_start = edit_row;
        int del_end = edit_row + (-lines_delta) - 1;
        if (p->cursor_row > del_end) {
            p->cursor_row += lines_delta;
        } else if (p->cursor_row >= del_start) {
            p->cursor_row = del_start;
        }
    }
    pane_clamp_cursor(p);
}

void pane_adjust_scroll(Pane *p, int text_width, int screen_height) {
    if (text_width < 1) text_width = 1;

    int margin = SCROLL_MARGIN;
    if (screen_height < margin * 2 + 1) {
        margin = (screen_height - 1) / 2;
    }
    if (margin < 0) margin = 0;

    int cursor_wrap = 0;
    if (p->cursor_col > 0 && text_width > 0) {
        int vis = buffer_get_visual_column(p->buffer,
            p->buffer->lines[p->cursor_row],
            p->buffer->line_lens[p->cursor_row],
            p->cursor_col);
        cursor_wrap = vis / text_width;
    }

    int rows_from_top = 0;
    for (int i = p->scroll_offset; i < p->cursor_row
             && i < p->buffer->line_count; i++) {
        int vw = buffer_get_visual_line_width(p->buffer,
            p->buffer->lines[i], p->buffer->line_lens[i]);
        rows_from_top += (vw == 0) ? 1 : (vw + text_width - 1) / text_width;
    }
    rows_from_top += cursor_wrap;

    /* Scroll up if cursor too close to top */
    if (rows_from_top < margin && p->scroll_offset > 0) {
        while (rows_from_top < margin && p->scroll_offset > 0) {
            p->scroll_offset--;
            int vw = buffer_get_visual_line_width(p->buffer,
                p->buffer->lines[p->scroll_offset],
                p->buffer->line_lens[p->scroll_offset]);
            rows_from_top += (vw == 0) ? 1 : (vw + text_width - 1) / text_width;
        }
    }

    int rows_used = rows_from_top + 1;

    /* Scroll down if cursor too close to bottom */
    if (rows_used > screen_height - margin) {
        int excess = rows_used - (screen_height - margin);
        while (excess > 0 && p->scroll_offset < p->cursor_row) {
            int vw = buffer_get_visual_line_width(p->buffer,
                p->buffer->lines[p->scroll_offset],
                p->buffer->line_lens[p->scroll_offset]);
            int lr = (vw == 0) ? 1 : (vw + text_width - 1) / text_width;
            if (lr <= excess) {
                excess -= lr;
                p->scroll_offset++;
            } else {
                break;
            }
        }
        if (excess > 0) p->scroll_offset++;
    }

    if (p->cursor_row < p->scroll_offset) {
        p->scroll_offset = p->cursor_row - margin;
        if (p->scroll_offset < 0) p->scroll_offset = 0;
    }
}

/* --- Selection ----------------------------------------------------------- */

void pane_start_selection(Pane *p) {
    p->selection_active = true;
    p->selection_start_row = p->cursor_row;
    p->selection_start_col = p->cursor_col;
}

void pane_clear_selection(Pane *p) {
    p->selection_active = false;
}

void pane_get_selection(const Pane *p, int *sr, int *sc, int *er, int *ec) {
    *sr = p->selection_start_row;
    *sc = p->selection_start_col;
    *er = p->cursor_row;
    *ec = p->cursor_col;
    normalize_range(sr, sc, er, ec);
}

bool pane_is_in_selection(const Pane *p, int row, int col) {
    if (!p->selection_active) return false;
    int sr, sc, er, ec;
    pane_get_selection(p, &sr, &sc, &er, &ec);
    if (row < sr || (row == sr && col < sc)) return false;
    if (row > er || (row == er && col > ec)) return false;
    return true;
}

/* --- Movement ------------------------------------------------------------ */

void pane_move_up(Pane *p) {
    if (p->cursor_row > 0) {
        p->cursor_row--;
        pane_clamp_cursor_col(p);
    }
}

void pane_move_down(Pane *p) {
    if (p->cursor_row < p->buffer->line_count - 1) {
        p->cursor_row++;
        pane_clamp_cursor_col(p);
    }
}

void pane_move_left(Pane *p) {
    if (p->cursor_col > 0) {
        p->cursor_col--;
    } else if (p->cursor_row > 0) {
        p->cursor_row--;
        p->cursor_col = p->buffer->line_lens[p->cursor_row];
    }
}

void pane_move_right(Pane *p) {
    if (p->cursor_col < p->buffer->line_lens[p->cursor_row]) {
        p->cursor_col++;
    } else if (p->cursor_row < p->buffer->line_count - 1) {
        p->cursor_row++;
        p->cursor_col = 0;
    }
}

void pane_move_up_n(Pane *p, int n) {
    for (int i = 0; i < n && p->cursor_row > 0; i++) {
        p->cursor_row--;
    }
    pane_clamp_cursor_col(p);
}

void pane_move_down_n(Pane *p, int n) {
    for (int i = 0; i < n && p->cursor_row < p->buffer->line_count - 1; i++) {
        p->cursor_row++;
    }
    pane_clamp_cursor_col(p);
}

void pane_move_left_n(Pane *p, int n) {
    for (int i = 0; i < n; i++) pane_move_left(p);
}

void pane_move_right_n(Pane *p, int n) {
    for (int i = 0; i < n; i++) pane_move_right(p);
}

void pane_move_to_line_start(Pane *p) {
    p->cursor_col = 0;
}

void pane_move_to_line_end(Pane *p) {
    p->cursor_col = p->buffer->line_lens[p->cursor_row];
}

void pane_page_down(Pane *p, int height) {
    p->cursor_row += height / 2;
    if (p->cursor_row >= p->buffer->line_count) {
        p->cursor_row = p->buffer->line_count - 1;
    }
    pane_clamp_cursor_col(p);
}

void pane_page_up(Pane *p, int height) {
    p->cursor_row -= height / 2;
    if (p->cursor_row < 0) p->cursor_row = 0;
    pane_clamp_cursor_col(p);
}

void pane_move_to_next_word(Pane *p) {
    wchar_t *line = p->buffer->lines[p->cursor_row];
    int ll = p->buffer->line_lens[p->cursor_row];

    if (p->cursor_col < ll) {
        wchar_t ch = line[p->cursor_col];
        if (is_word_char(ch)) {
            while (p->cursor_col < ll && is_word_char(line[p->cursor_col]))
                p->cursor_col++;
        } else if (!is_whitespace(ch)) {
            while (p->cursor_col < ll && !is_word_char(line[p->cursor_col])
                   && !is_whitespace(line[p->cursor_col]))
                p->cursor_col++;
        }
    }

    while (p->cursor_col < ll && is_whitespace(line[p->cursor_col]))
        p->cursor_col++;

    if (p->cursor_col >= ll && p->cursor_row < p->buffer->line_count - 1) {
        p->cursor_row++;
        p->cursor_col = 0;
        line = p->buffer->lines[p->cursor_row];
        ll = p->buffer->line_lens[p->cursor_row];
        while (p->cursor_col < ll && is_whitespace(line[p->cursor_col]))
            p->cursor_col++;
    }
}

void pane_move_to_prev_word(Pane *p) {
    if (p->cursor_col == 0 && p->cursor_row > 0) {
        p->cursor_row--;
        p->cursor_col = p->buffer->line_lens[p->cursor_row];
    }

    wchar_t *line = p->buffer->lines[p->cursor_row];
    int ll = p->buffer->line_lens[p->cursor_row];

    if (p->cursor_col > 0) p->cursor_col--;

    while (p->cursor_col > 0 && is_whitespace(line[p->cursor_col]))
        p->cursor_col--;

    if (p->cursor_col < ll) {
        wchar_t ch = line[p->cursor_col];
        if (is_word_char(ch)) {
            while (p->cursor_col > 0 && is_word_char(line[p->cursor_col - 1]))
                p->cursor_col--;
        } else if (!is_whitespace(ch)) {
            while (p->cursor_col > 0 && !is_word_char(line[p->cursor_col - 1])
                   && !is_whitespace(line[p->cursor_col - 1]))
                p->cursor_col--;
        }
    }
}

static void skip_ws_crossing_lines(Pane *p) {
    wchar_t *line = p->buffer->lines[p->cursor_row];
    int ll = p->buffer->line_lens[p->cursor_row];
    while (p->cursor_col >= ll || is_whitespace(line[p->cursor_col])) {
        if (p->cursor_col >= ll) {
            if (p->cursor_row >= p->buffer->line_count - 1) return;
            p->cursor_row++;
            p->cursor_col = 0;
            line = p->buffer->lines[p->cursor_row];
            ll = p->buffer->line_lens[p->cursor_row];
        } else p->cursor_col++;
    }
}

void pane_move_to_word_end(Pane *p) {
    int ll = p->buffer->line_lens[p->cursor_row];
    if (p->cursor_col < ll - 1) p->cursor_col++;
    else if (p->cursor_row < p->buffer->line_count - 1) {
        p->cursor_row++;
        p->cursor_col = 0;
    } else return;

    skip_ws_crossing_lines(p);
    wchar_t *line = p->buffer->lines[p->cursor_row];
    ll = p->buffer->line_lens[p->cursor_row];
    if (p->cursor_col >= ll) return;

    if (is_word_char(line[p->cursor_col])) {
        while (p->cursor_col + 1 < ll
               && is_word_char(line[p->cursor_col + 1]))
            p->cursor_col++;
    } else if (!is_whitespace(line[p->cursor_col])) {
        while (p->cursor_col + 1 < ll
               && !is_word_char(line[p->cursor_col + 1])
               && !is_whitespace(line[p->cursor_col + 1]))
            p->cursor_col++;
    }
}

void pane_find_char_forward(Pane *p, wchar_t ch) {
    wchar_t *line = p->buffer->lines[p->cursor_row];
    int ll = p->buffer->line_lens[p->cursor_row];
    for (int i = p->cursor_col + 1; i < ll; i++) {
        if (line[i] == ch) { p->cursor_col = i; return; }
    }
}

void pane_find_char_backward(Pane *p, wchar_t ch) {
    wchar_t *line = p->buffer->lines[p->cursor_row];
    for (int i = p->cursor_col - 1; i >= 0; i--) {
        if (line[i] == ch) { p->cursor_col = i; return; }
    }
}
