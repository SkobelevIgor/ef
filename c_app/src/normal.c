#include "normal.h"
#include "editor.h"
#include "runes.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for sub-handlers */
static bool handle_goto_line_input(Editor *ed, EditorEvent *ev);
static bool handle_find_char_input(Editor *ed, EditorEvent *ev);
static bool handle_normal_rune(Editor *ed, wchar_t r);
static void handle_pending_operator(Editor *ed, wchar_t r);

bool handle_normal_mode(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    InputState *is = ed->input_state;
    int height = 0, width = 0;
    ed->screen->get_size(ed->screen->impl, &width, &height);

    /* Pending goto line */
    if (is->pending_goto_line) {
        return handle_goto_line_input(ed, ev);
    }

    /* Pending find char */
    if (is->pending_find_forward || is->pending_find_backward) {
        return handle_find_char_input(ed, ev);
    }

    if (ev->type != EV_KEY) return false;

    /* Special keys */
    if (!ev->is_char) {
        switch (ev->key) {
        case 27: /* Escape */
            input_state_reset(is);
            return false;
        case KEY_UP:
            pane_move_up_n(pane, input_state_get_count(is));
            break;
        case KEY_DOWN:
            pane_move_down_n(pane, input_state_get_count(is));
            break;
        case KEY_LEFT:
            pane_move_left_n(pane, input_state_get_count(is));
            break;
        case KEY_RIGHT:
            pane_move_right_n(pane, input_state_get_count(is));
            break;
        case KEY_PPAGE: /* Ctrl+U equivalent */
            pane_page_up(pane, height);
            break;
        case KEY_NPAGE: /* Ctrl+D equivalent */
            pane_page_down(pane, height);
            break;
        case KEY_BTAB: /* Shift+Tab: switch pane */
            if (ed->pane_count > 1) {
                ed->active_pane_idx = (ed->active_pane_idx + 1) % ed->pane_count;
            }
            break;
        }
        input_state_reset(is);
        return false;
    }

    /* Rune keys */
    wchar_t ch = ev->ch;

    /* Ctrl keys as low chars */
    if (ch == 4) { /* Ctrl+D */
        pane_page_down(pane, height);
        input_state_reset(is);
        return false;
    }
    if (ch == 21) { /* Ctrl+U */
        pane_page_up(pane, height);
        input_state_reset(is);
        return false;
    }
    if (ch == 18) { /* Ctrl+R */
        editor_redo(ed);
        input_state_reset(is);
        return false;
    }
    if (ch == 27) { /* Escape */
        input_state_reset(is);
        return false;
    }

    bool skip_reset = handle_normal_rune(ed, ch);
    if (!skip_reset) input_state_reset(is);
    return false;
}

static bool handle_normal_rune(Editor *ed, wchar_t r) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    InputState *is = ed->input_state;
    int count = input_state_get_count(is);

    /* Handle pending operator */
    if (is->pending_operator != 0) {
        handle_pending_operator(ed, r);
        return false;
    }

    switch (r) {
    /* Numeric prefix */
    case L'1': case L'2': case L'3': case L'4': case L'5':
    case L'6': case L'7': case L'8': case L'9':
        input_state_add_digit(is, (int)(r - L'0'));
        return true;
    case L'0':
        if (is->has_count) {
            input_state_add_digit(is, 0);
            return true;
        }
        pane_move_to_line_start(pane);
        return false;

    /* Navigation */
    case L'h': pane_move_left_n(pane, count); break;
    case L'j': pane_move_down_n(pane, count); break;
    case L'k': pane_move_up_n(pane, count); break;
    case L'l': pane_move_right_n(pane, count); break;
    case L'$': pane_move_to_line_end(pane); break;

    /* Mode switching */
    case L'i': editor_enter_insert_mode(ed); break;
    case L'a':
        if (pane->cursor_col < buf->line_lens[pane->cursor_row]) {
            pane->cursor_col++;
        }
        editor_enter_insert_mode(ed);
        break;
    case L'A':
        pane_move_to_line_end(pane);
        editor_enter_insert_mode(ed);
        break;
    case L'I':
        pane_move_to_line_start(pane);
        editor_enter_insert_mode(ed);
        break;
    case L'o':
        editor_enter_insert_mode(ed);
        buffer_open_line_below(buf, pane->cursor_row,
                               &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        break;
    case L'O':
        editor_enter_insert_mode(ed);
        buffer_open_line_above(buf, pane->cursor_row,
                               &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        break;
    case L'v':
        ed->mode = MODE_VISUAL;
        pane_start_selection(pane);
        break;

    /* Find char */
    case L'f':
        is->pending_find_forward = true;
        return true;
    case L'F':
        is->pending_find_backward = true;
        return true;
    case L';':
        if (is->has_last_find) {
            for (int i = 0; i < count; i++) {
                if (is->last_find_forward)
                    pane_find_char_forward(pane, is->last_find_char);
                else
                    pane_find_char_backward(pane, is->last_find_char);
            }
        }
        break;
    case L',':
        if (is->has_last_find) {
            for (int i = 0; i < count; i++) {
                if (is->last_find_forward)
                    pane_find_char_backward(pane, is->last_find_char);
                else
                    pane_find_char_forward(pane, is->last_find_char);
            }
        }
        break;

    /* Goto line */
    case L':':
        is->pending_goto_line = true;
        is->goto_line_buffer[0] = '\0';
        is->goto_line_buf_len = 0;
        return true;

    /* Page navigation */
    case L'G':
        if (is->has_count)
            pane_goto_line(pane, count);
        else
            pane_goto_line(pane, buf->line_count);
        break;
    case L'g':
        pane_goto_line(pane, 1);
        break;

    /* Word navigation */
    case L'w':
        for (int i = 0; i < count; i++) pane_move_to_next_word(pane);
        break;
    case L'b':
        for (int i = 0; i < count; i++) pane_move_to_prev_word(pane);
        break;

    /* Operators */
    case L'd':
        is->pending_operator = L'd';
        return true;
    case L'y':
        is->pending_operator = L'y';
        return true;

    /* Delete char under cursor */
    case L'x': {
        int start_col = pane->cursor_col;
        wchar_t *deleted_chars = malloc(sizeof(wchar_t) * count);
        int del_count = 0;
        for (int i = 0; i < count; i++) {
            if (pane->cursor_col < buf->line_lens[pane->cursor_row]) {
                wchar_t d = buffer_delete_char_at(buf, pane->cursor_row,
                                                  pane->cursor_col);
                if (d != 0) deleted_chars[del_count++] = d;
            }
        }
        if (del_count > 0) {
            clipboard_set(ed->clipboard, &deleted_chars, &del_count, 1, false);
            history_record_delete(ed->history, buf, pane->cursor_row,
                                  start_col, &deleted_chars, &del_count, 1);
        }
        free(deleted_chars);
        pane_clamp_cursor_col(pane);
        editor_schedule_auto_save(ed);
        break;
    }

    /* Paste */
    case L'p':
        if (ed->clipboard->line_count > 0)
            editor_paste_after(ed);
        break;
    case L'P':
        if (ed->clipboard->line_count > 0)
            editor_paste_before(ed);
        break;

    /* Undo */
    case L'u':
        editor_undo(ed);
        break;
    }

    return false;
}

static void handle_pending_operator(Editor *ed, wchar_t r) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    InputState *is = ed->input_state;
    wchar_t op = is->pending_operator;
    int count = input_state_get_count(is);

    if (op == L'd' && r == L'd') {
        /* dd: delete lines */
        int start_row = pane->cursor_row;
        wchar_t **deleted = malloc(sizeof(wchar_t *) * count);
        int *del_lens = malloc(sizeof(int) * count);
        int actual = 0;
        for (int i = 0; i < count && pane->cursor_row < buf->line_count; i++) {
            deleted[i] = buffer_delete_line(buf, pane->cursor_row, &del_lens[i]);
            actual++;
        }
        clipboard_set(ed->clipboard, deleted, del_lens, actual, true);
        history_record_delete_lines(ed->history, buf, start_row,
                                    deleted, del_lens, actual);
        for (int i = 0; i < actual; i++) free(deleted[i]);
        free(deleted);
        free(del_lens);
        pane_clamp_cursor(pane);
        editor_schedule_auto_save(ed);
    } else if (op == L'y' && r == L'y') {
        /* yy: yank lines */
        wchar_t **yanked = malloc(sizeof(wchar_t *) * count);
        int *yank_lens = malloc(sizeof(int) * count);
        int actual = 0;
        for (int i = 0; i < count
                 && pane->cursor_row + i < buf->line_count; i++) {
            yanked[i] = buffer_copy_line(buf, pane->cursor_row + i,
                                         &yank_lens[i]);
            actual++;
        }
        clipboard_set(ed->clipboard, yanked, yank_lens, actual, true);
        for (int i = 0; i < actual; i++) free(yanked[i]);
        free(yanked);
        free(yank_lens);
    } else if (op == L'd' && r == L'w') {
        /* dw: delete word */
        int sr = pane->cursor_row, sc = pane->cursor_col;
        for (int i = 0; i < count; i++) pane_move_to_next_word(pane);
        int er = pane->cursor_row, ec = pane->cursor_col;
        pane->cursor_row = sr;
        pane->cursor_col = sc;
        int *del_lens;
        int del_count;
        wchar_t **deleted = buffer_delete_range(buf, sr, sc, er, ec,
                                                &del_lens, &del_count);
        clipboard_set(ed->clipboard, deleted, del_lens, del_count, false);
        history_record_delete(ed->history, buf, sr, sc,
                              deleted, del_lens, del_count);
        buffer_free_lines(deleted, del_lens, del_count);
        editor_schedule_auto_save(ed);
    } else if (op == L'd' && r == L'b') {
        /* db: delete word backward */
        int er = pane->cursor_row, ec = pane->cursor_col;
        for (int i = 0; i < count; i++) pane_move_to_prev_word(pane);
        int sr = pane->cursor_row, sc = pane->cursor_col;
        int *del_lens;
        int del_count;
        wchar_t **deleted = buffer_delete_range(buf, sr, sc, er, ec,
                                                &del_lens, &del_count);
        clipboard_set(ed->clipboard, deleted, del_lens, del_count, false);
        history_record_delete(ed->history, buf, sr, sc,
                              deleted, del_lens, del_count);
        buffer_free_lines(deleted, del_lens, del_count);
        editor_schedule_auto_save(ed);
    } else if (op == L'd' && r == L'$') {
        /* d$: delete to end of line */
        int line_len = buf->line_lens[pane->cursor_row];
        if (pane->cursor_col < line_len) {
            int del_len = line_len - pane->cursor_col;
            wchar_t *del = malloc(sizeof(wchar_t) * (del_len + 1));
            wmemcpy(del, buf->lines[pane->cursor_row] + pane->cursor_col, del_len);
            del[del_len] = L'\0';
            /* Truncate line */
            int new_len;
            wchar_t *nl = remove_runes(buf->lines[pane->cursor_row], line_len,
                                       pane->cursor_col, line_len, &new_len);
            buffer_set_line(buf, pane->cursor_row, nl, new_len);
            clipboard_set(ed->clipboard, &del, &del_len, 1, false);
            history_record_delete(ed->history, buf, pane->cursor_row,
                                  pane->cursor_col, &del, &del_len, 1);
            free(del);
            buf->modified = true;
            editor_schedule_auto_save(ed);
        }
    } else if (op == L'd' && r == L'0') {
        /* d0: delete to beginning of line */
        if (pane->cursor_col > 0) {
            int del_len = pane->cursor_col;
            wchar_t *del = malloc(sizeof(wchar_t) * (del_len + 1));
            wmemcpy(del, buf->lines[pane->cursor_row], del_len);
            del[del_len] = L'\0';
            int new_len;
            wchar_t *nl = remove_runes(buf->lines[pane->cursor_row],
                                       buf->line_lens[pane->cursor_row],
                                       0, del_len, &new_len);
            buffer_set_line(buf, pane->cursor_row, nl, new_len);
            clipboard_set(ed->clipboard, &del, &del_len, 1, false);
            history_record_delete(ed->history, buf, pane->cursor_row, 0,
                                  &del, &del_len, 1);
            free(del);
            pane->cursor_col = 0;
            buf->modified = true;
            editor_schedule_auto_save(ed);
        }
    }
}

static bool handle_find_char_input(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    InputState *is = ed->input_state;
    int count = input_state_get_count(is);

    if (ev->is_char && ev->ch == 27) { /* Escape */
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

static bool handle_goto_line_input(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    InputState *is = ed->input_state;

    if (ev->is_char && ev->ch == 27) { /* Escape */
        input_state_reset(is);
        return false;
    }

    if (ev->is_char && (ev->ch == L'\n' || ev->ch == L'\r')) { /* Enter */
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

    /* Backspace */
    if (ev->is_char && (ev->ch == 127 || ev->ch == 8)) {
        if (is->goto_line_buf_len > 0) {
            is->goto_line_buffer[--is->goto_line_buf_len] = '\0';
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
