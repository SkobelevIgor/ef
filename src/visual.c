#include "visual.h"
#include "xalloc.h"
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
        return input_handle_find_char(is, pane, ev, 1);
    }

    /* Pending goto line */
    if (is->pending_goto_line) {
        return input_handle_goto_line(is, pane, ev);
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

    if (ch == KEY_ESC) { /* Escape */
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        return false;
    }

    if (ch == CTRL_D) { /* Ctrl+D */
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_down(pane, height);
        return false;
    }
    if (ch == CTRL_U) { /* Ctrl+U */
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
    case L'e': pane_move_to_word_end(pane); break;
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
    case L'n':
        if (is->has_last_find) {
            if (is->last_find_forward) pane_find_char_forward(pane, is->last_find_char);
            else pane_find_char_backward(pane, is->last_find_char);
        }
        break;
    case L'N':
        if (is->has_last_find) {
            if (is->last_find_forward) pane_find_char_backward(pane, is->last_find_char);
            else pane_find_char_forward(pane, is->last_find_char);
        }
        break;

    case L'y': {
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        if (sr != er) {
            /* Multi-row: yank full lines in line-mode */
            int count = er - sr + 1;
            wchar_t **lines = xmalloc(sizeof(wchar_t *) * count);
            int *lens = xmalloc(sizeof(int) * count);
            for (int i = 0; i < count; i++)
                lines[i] = buffer_copy_line(buf, sr + i, &lens[i]);
            clipboard_set(ed->clipboard, lines, lens, count, true);
            for (int i = 0; i < count; i++) free(lines[i]);
            free(lines);
            free(lens);
        } else {
            /* Single-row: char-mode yank */
            int *rlens; int rcount;
            wchar_t **range = buffer_get_range(buf, sr, sc, er, ec,
                                               &rlens, &rcount);
            clipboard_set(ed->clipboard, range, rlens, rcount, false);
            buffer_free_lines(range, rlens, rcount);
        }
        ed->mode = MODE_NORMAL;
        pane_clear_selection(pane);
        input_state_reset(is);
        break;
    }

    case L'd': case L'x': {
        if (ed->read_only) break;
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

    case L'>': case L'<': case L'=': {
        if (ed->read_only) break;
        int sr, sc, er, ec;
        pane_get_selection(pane, &sr, &sc, &er, &ec);
        if (ch == L'>') buffer_indent_range(buf, sr, er);
        else if (ch == L'<') buffer_unindent_range(buf, sr, er);
        else buffer_reindent_range(buf, sr, er);
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
