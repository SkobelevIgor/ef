#include "insert.h"
#include "editor.h"

#include <ncurses.h>

bool handle_insert_mode(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int height = 0, width = 0;

    if (ev->type != EV_KEY) return false;

    /* Special keys */
    if (!ev->is_char) {
        switch (ev->key) {
        case KEY_UP:    pane_move_up(pane); break;
        case KEY_DOWN:  pane_move_down(pane); break;
        case KEY_LEFT:  pane_move_left(pane); break;
        case KEY_RIGHT: pane_move_right(pane); break;
        case KEY_BACKSPACE:
            buffer_delete_char(buf, pane->cursor_row, pane->cursor_col,
                               &pane->cursor_row, &pane->cursor_col);
            editor_schedule_auto_save(ed);
            break;
        case KEY_DC: /* Delete */
            buffer_delete_char_forward(buf, pane->cursor_row, pane->cursor_col);
            editor_schedule_auto_save(ed);
            break;
        case KEY_BTAB: /* Shift+Tab: switch pane */
            if (ed->pane_count > 1) {
                history_commit_session(ed->history, buf->lines,
                                       buf->line_lens, buf->line_count);
                ed->active_pane_idx = (ed->active_pane_idx + 1) % ed->pane_count;
                Pane *np = editor_active_pane(ed);
                history_start_session(ed->history, np->buffer,
                                      np->cursor_row, np->cursor_col);
            }
            break;
        case KEY_PPAGE:
            ed->screen->get_size(ed->screen->impl, &width, &height);
            pane_page_up(pane, height);
            break;
        case KEY_NPAGE:
            ed->screen->get_size(ed->screen->impl, &width, &height);
            pane_page_down(pane, height);
            break;
        }
        return false;
    }

    /* Character keys */
    wchar_t ch = ev->ch;

    if (ch == 27) { /* Escape */
        ed->mode = MODE_NORMAL;
        history_commit_session(ed->history, buf->lines,
                               buf->line_lens, buf->line_count);
        input_state_reset(ed->input_state);
        if (pane->cursor_col > 0) pane->cursor_col--;
        return false;
    }

    if (ch == 127 || ch == 8) { /* Backspace */
        buffer_delete_char(buf, pane->cursor_row, pane->cursor_col,
                           &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        return false;
    }

    if (ch == L'\n' || ch == L'\r') { /* Enter */
        buffer_insert_newline_with_indent(buf, pane->cursor_row, pane->cursor_col,
                                         &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        return false;
    }

    if (ch == L'\t') { /* Tab */
        pane->cursor_col = buffer_insert_tab(buf, pane->cursor_row,
                                             pane->cursor_col);
        editor_schedule_auto_save(ed);
        return false;
    }

    if (ch == 4) { /* Ctrl+D */
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_down(pane, height);
        return false;
    }

    if (ch == 21) { /* Ctrl+U */
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_up(pane, height);
        return false;
    }

    /* Regular character */
    if (ch >= 32) { /* Printable */
        pane->cursor_col = buffer_insert_char(buf, pane->cursor_row,
                                              pane->cursor_col, ch);
        editor_schedule_auto_save(ed);
    }

    return false;
}
