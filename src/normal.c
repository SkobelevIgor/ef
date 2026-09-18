#include "normal.h"
#include "xalloc.h"
#include "editor.h"
#include "runes.h"

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for sub-handlers */
static bool handle_goto_line_input(Editor *ed, EditorEvent *ev);
static bool handle_find_char_input(Editor *ed, EditorEvent *ev);
static bool handle_mark_input(Editor *ed, EditorEvent *ev);
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

    /* Pending mark set/jump */
    if (is->pending_mark || is->pending_jump_to_mark) {
        return handle_mark_input(ed, ev);
    }

    if (ev->type != EV_KEY) return false;

    /* Special keys */
    if (!ev->is_char) {
        switch (ev->key) {
        case KEY_ESC: /* Escape */
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
    if (ch == CTRL_D) { /* Ctrl+D */
        pane_page_down(pane, height);
        input_state_reset(is);
        return false;
    }
    if (ch == CTRL_U) { /* Ctrl+U */
        pane_page_up(pane, height);
        input_state_reset(is);
        return false;
    }
    if (ch == CTRL_R) { /* Ctrl+R */
        if (!ed->read_only) editor_redo(ed);
        input_state_reset(is);
        return false;
    }
    if (ch == KEY_ESC) { /* Escape */
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
    case L'i':
        editor_enter_insert_mode(ed);
        break;
    case L'a':
        if (!ed->read_only) {
            if (pane->cursor_col < buf->line_lens[pane->cursor_row])
                pane->cursor_col++;
            editor_enter_insert_mode(ed);
        }
        break;
    case L'A':
        if (!ed->read_only) {
            pane_move_to_line_end(pane);
            editor_enter_insert_mode(ed);
        }
        break;
    case L'I':
        if (!ed->read_only) {
            pane_move_to_line_start(pane);
            editor_enter_insert_mode(ed);
        }
        break;
    case L'o':
        if (!ed->read_only) {
            editor_enter_insert_mode(ed);
            buffer_open_line_below(buf, pane->cursor_row,
                                   &pane->cursor_row, &pane->cursor_col);
            editor_schedule_auto_save(ed);
        }
        break;
    case L'O':
        if (!ed->read_only) {
            editor_enter_insert_mode(ed);
            buffer_open_line_above(buf, pane->cursor_row,
                                   &pane->cursor_row, &pane->cursor_col);
            editor_schedule_auto_save(ed);
        }
        break;
    case L'v':
        if (!ed->read_only) {
            ed->mode = MODE_VISUAL;
            pane_start_selection(pane);
        }
        break;

    /* Find char */
    case L'f':
        is->pending_find_forward = true;
        return true;
    case L'F':
        is->pending_find_backward = true;
        return true;
    case L'n':
        if (is->has_last_find) {
            for (int i = 0; i < count; i++) {
                if (is->last_find_forward)
                    pane_find_char_forward(pane, is->last_find_char);
                else
                    pane_find_char_backward(pane, is->last_find_char);
            }
        }
        break;
    case L'N':
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
        if (ed->read_only) break;
        is->pending_operator = L'd';
        return true;
    case L'y':
        is->pending_operator = L'y';
        return true;

    /* Delete char under cursor */
    case L'x': {
        if (ed->read_only) break;
        int start_col = pane->cursor_col;
        int remaining = buf->line_lens[pane->cursor_row] - start_col;
        if (count > remaining) count = remaining;
        if (count <= 0) break;
        wchar_t *deleted_chars = xmalloc(sizeof(wchar_t) * count);
        int del_count = 0;
        for (int i = 0; i < count; i++) {
            wchar_t d = buffer_delete_char_at(buf, pane->cursor_row,
                                              pane->cursor_col);
            if (d != 0) deleted_chars[del_count++] = d;
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
        if (!ed->read_only && ed->clipboard->line_count > 0)
            editor_paste_after(ed);
        break;
    case L'P':
        if (!ed->read_only && ed->clipboard->line_count > 0)
            editor_paste_before(ed);
        break;

    /* Undo */
    case L'u':
        if (!ed->read_only) editor_undo(ed);
        break;

    /* Marks */
    case L'm':
        is->pending_mark = true;
        return true;
    case L'`':
        is->pending_jump_to_mark = true;
        return true;
    }

    return false;
}

/* --- Operator helpers ---------------------------------------------------- */

static void handle_op_dd(Editor *ed, int count) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int start_row = pane->cursor_row;
    int remaining = buf->line_count - start_row;
    if (count > remaining) count = remaining;
    wchar_t **deleted = xmalloc(sizeof(wchar_t *) * count);
    int *del_lens = xmalloc(sizeof(int) * count);
    int actual = 0;
    for (int i = 0; i < count; i++) {
        deleted[i] = buffer_delete_line(buf, pane->cursor_row, &del_lens[i]);
        actual++;
    }
    clipboard_set(ed->clipboard, deleted, del_lens, actual, true);
    if (start_row == 0 && actual == remaining) {
        Change *c = change_new(CHANGE_REPLACE, buf, 0, 0);
        c->old_text = buffer_copy_lines(deleted, del_lens, actual,
                                        &c->old_text_lens);
        c->old_text_count = actual;
        c->text = buffer_copy_lines(buf->lines, buf->line_lens, 1,
                                    &c->text_lens);
        c->text_count = 1;
        history_push(ed->history, c);
    } else {
        history_record_delete_lines(ed->history, buf, start_row,
                                    deleted, del_lens, actual);
    }
    for (int i = 0; i < actual; i++) free(deleted[i]);
    free(deleted);
    free(del_lens);
    pane_clamp_cursor(pane);
    editor_schedule_auto_save(ed);
}

static void handle_op_yy(Editor *ed, int count) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int remaining = buf->line_count - pane->cursor_row;
    if (count > remaining) count = remaining;
    wchar_t **yanked = xmalloc(sizeof(wchar_t *) * count);
    int *yank_lens = xmalloc(sizeof(int) * count);
    int actual = 0;
    for (int i = 0; i < count; i++) {
        yanked[i] = buffer_copy_line(buf, pane->cursor_row + i,
                                     &yank_lens[i]);
        actual++;
    }
    clipboard_set(ed->clipboard, yanked, yank_lens, actual, true);
    for (int i = 0; i < actual; i++) free(yanked[i]);
    free(yanked);
    free(yank_lens);
}

static void normal_delete_range(Editor *ed, int sr, int sc, int er, int ec) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
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
}

static void handle_op_dw(Editor *ed, int count) {
    Pane *pane = editor_active_pane(ed);
    int sr = pane->cursor_row, sc = pane->cursor_col;
    for (int i = 0; i < count; i++) pane_move_to_next_word(pane);
    int er = pane->cursor_row, ec = pane->cursor_col;
    normal_delete_range(ed, sr, sc, er, ec);
}

static void handle_op_db(Editor *ed, int count) {
    Pane *pane = editor_active_pane(ed);
    int er = pane->cursor_row, ec = pane->cursor_col;
    for (int i = 0; i < count; i++) pane_move_to_prev_word(pane);
    int sr = pane->cursor_row, sc = pane->cursor_col;
    normal_delete_range(ed, sr, sc, er, ec);
}

static void normal_delete_to_col(Editor *ed, int from, int to) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int del_len = to - from;
    wchar_t *del = xmalloc(sizeof(wchar_t) * (del_len + 1));
    wmemcpy(del, buf->lines[pane->cursor_row] + from, del_len);
    del[del_len] = L'\0';
    int new_len;
    wchar_t *nl = remove_runes(buf->lines[pane->cursor_row],
                               buf->line_lens[pane->cursor_row],
                               from, to, &new_len);
    buffer_set_line(buf, pane->cursor_row, nl, new_len);
    clipboard_set(ed->clipboard, &del, &del_len, 1, false);
    history_record_delete(ed->history, buf, pane->cursor_row,
                          from, &del, &del_len, 1);
    free(del);
    pane->cursor_col = from;
    buf->modified = true;
    editor_schedule_auto_save(ed);
}

static void handle_op_d_dollar(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int line_len = buf->line_lens[pane->cursor_row];
    if (pane->cursor_col < line_len)
        normal_delete_to_col(ed, pane->cursor_col, line_len);
}

static void handle_op_d_zero(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    if (pane->cursor_col > 0)
        normal_delete_to_col(ed, 0, pane->cursor_col);
}

static void handle_pending_operator(Editor *ed, wchar_t r) {
    InputState *is = ed->input_state;
    wchar_t op = is->pending_operator;
    int count = input_state_get_count(is);

    if (op == L'd' && r == L'd')      handle_op_dd(ed, count);
    else if (op == L'y' && r == L'y') handle_op_yy(ed, count);
    else if (op == L'd' && r == L'w') handle_op_dw(ed, count);
    else if (op == L'd' && r == L'b') handle_op_db(ed, count);
    else if (op == L'd' && r == L'$') handle_op_d_dollar(ed);
    else if (op == L'd' && r == L'0') handle_op_d_zero(ed);
}

static bool is_valid_mark_id(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z')
        || (ch >= L'A' && ch <= L'Z')
        || (ch >= L'0' && ch <= L'9');
}

static bool handle_mark_input(Editor *ed, EditorEvent *ev) {
    InputState *is = ed->input_state;

    if (ev->is_char && ev->ch == KEY_ESC) { /* Escape */
        is->pending_mark = false;
        is->pending_jump_to_mark = false;
        input_state_reset(is);
        return false;
    }
    if (!ev->is_char) {
        is->pending_mark = false;
        is->pending_jump_to_mark = false;
        input_state_reset(is);
        return false;
    }

    wchar_t ch = ev->ch;
    if (!is_valid_mark_id(ch)) {
        is->pending_mark = false;
        is->pending_jump_to_mark = false;
        input_state_reset(is);
        return false;
    }

    int idx = (int)ch; /* ASCII value as index */
    if (is->pending_mark) {
        Pane *pane = editor_active_pane(ed);
        ed->marks[idx].set = true;
        ed->marks[idx].buffer = pane->buffer;
        ed->marks[idx].row = pane->cursor_row;
        ed->marks[idx].col = pane->cursor_col;
    } else if (is->pending_jump_to_mark) {
        GlobalMark *m = &ed->marks[idx];
        if (m->set) {
            /* Find pane with this buffer */
            for (int i = 0; i < ed->pane_count; i++) {
                if (ed->panes[i]->buffer == m->buffer) {
                    ed->active_pane_idx = i;
                    Pane *p = ed->panes[i];
                    p->cursor_row = m->row;
                    p->cursor_col = m->col;
                    pane_clamp_cursor(p);
                    break;
                }
            }
        }
    }
    is->pending_mark = false;
    is->pending_jump_to_mark = false;
    input_state_reset(is);
    return false;
}

static bool handle_find_char_input(Editor *ed, EditorEvent *ev) {
    return input_handle_find_char(ed->input_state, editor_active_pane(ed),
                                  ev, input_state_get_count(ed->input_state));
}

static bool handle_goto_line_input(Editor *ed, EditorEvent *ev) {
    return input_handle_goto_line(ed->input_state, editor_active_pane(ed), ev);
}
