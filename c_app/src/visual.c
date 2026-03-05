#include "visual.h"
#include "editor.h"
#include "runes.h"

#include <ncurses.h>
#include <stdlib.h>

bool handle_visual_mode(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    InputState *is = ed->input_state;
    int height = 0, width = 0;

    /* Pending find char */
    if (is->pending_find_forward || is->pending_find_backward) {
        if (ev->is_char && ev->ch == 27) {
            input_state_reset(is);
            return false;
        }
        if (ev->is_char) {
            bool forward = is->pending_find_forward;
            if (forward) pane_find_char_forward(pane, ev->ch);
            else pane_find_char_backward(pane, ev->ch);
            input_state_save_last_find(is, ev->ch, forward);
            input_state_reset(is);
        }
        return false;
    }

    /* Pending goto line */
    if (is->pending_goto_line) {
        if (ev->is_char && ev->ch == 27) {
            input_state_reset(is);
            return false;
        }
        if (ev->is_char && ev->ch == L'\n') {
            if (is->goto_line_buf_len > 0) {
                if (is->goto_line_buffer[0] == '$') {
                    pane_goto_line(pane, buf->line_count);
                } else {
                    int ln = atoi(is->goto_line_buffer);
                    if (ln > 0) pane_goto_line(pane, ln);
                }
            }
            input_state_reset(is);
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

    if (ev->type != EV_KEY) return false;

    /* Special keys */
    if (!ev->is_char) {
        switch (ev->key) {
        case KEY_UP:    pane_move_up(pane); break;
        case KEY_DOWN:  pane_move_down(pane); break;
        case KEY_LEFT:  pane_move_left(pane); break;
        case KEY_RIGHT: pane_move_right(pane); break;
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

    wchar_t ch = ev->ch;

    if (ch == 27) { /* Escape */
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
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

    switch (ch) {
    case L'h': pane_move_left(pane); break;
    case L'j': pane_move_down(pane); break;
    case L'k': pane_move_up(pane); break;
    case L'l': pane_move_right(pane); break;
    case L'0': pane_move_to_line_start(pane); break;
    case L'$': pane_move_to_line_end(pane); break;
    case L'w': pane_move_to_next_word(pane); break;
    case L'b': pane_move_to_prev_word(pane); break;
    case L'G': pane_goto_line(pane, buf->line_count); break;
    case L'g': pane_goto_line(pane, 1); break;

    case L'v':
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        break;

    case L'f':
        is->pending_find_forward = true;
        break;
    case L'F':
        is->pending_find_backward = true;
        break;
    case L';':
        if (is->has_last_find) {
            if (is->last_find_forward) pane_find_char_forward(pane, is->last_find_char);
            else pane_find_char_backward(pane, is->last_find_char);
        }
        break;
    case L',':
        if (is->has_last_find) {
            if (is->last_find_forward) pane_find_char_backward(pane, is->last_find_char);
            else pane_find_char_forward(pane, is->last_find_char);
        }
        break;

    case L'y': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        int *rlens; int rcount;
        wchar_t **range = buffer_get_range(buf, sr, sc, er, ec, &rlens, &rcount);
        clipboard_set(ed->clipboard, range, rlens, rcount, false);
        buffer_free_lines(range, rlens, rcount);
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        break;
    }

    case L'd': case L'x': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        int *del_lens; int del_count;
        wchar_t **deleted = buffer_delete_range(buf, sr, sc, er, ec + 1,
                                                &del_lens, &del_count);
        clipboard_set(ed->clipboard, deleted, del_lens, del_count, false);
        history_record_delete(ed->history, buf, sr, sc,
                              deleted, del_lens, del_count);
        buffer_free_lines(deleted, del_lens, del_count);
        pane->cursor_row = sr;
        pane->cursor_col = sc;
        pane_clamp_cursor(pane);
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        editor_schedule_auto_save(ed);
        break;
    }

    case L'>': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        buffer_indent_range(buf, sr, er);
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        editor_schedule_auto_save(ed);
        break;
    }

    case L'<': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        buffer_unindent_range(buf, sr, er);
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        editor_schedule_auto_save(ed);
        break;
    }

    case L'=': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        buffer_reindent_range(buf, sr, er);
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        editor_schedule_auto_save(ed);
        break;
    }

    case L':':
        is->pending_goto_line = true;
        is->goto_line_buffer[0] = '\0';
        is->goto_line_buf_len = 0;
        break;
    }

    return false;
}
