#include "insert.h"
#include "editor.h"
#include "autocomplete.h"

#include <ncurses.h>

static void dismiss_ac(Editor *ed) {
    if (ed->input_state->autocomplete) {
        ac_state_free(ed->input_state->autocomplete);
        ed->input_state->autocomplete = NULL;
    }
}

static bool handle_ac_keys(Editor *ed, EditorEvent *ev) {
    AutocompleteState *ac = ed->input_state->autocomplete;
    if (!ac || !ac->active) return false;

    /* Character keys when AC active */
    if (ev->is_char) {
        if (ev->ch == 27) { /* Escape — dismiss popup, stay in insert */
            dismiss_ac(ed);
            return true;
        }
        if (ev->ch == L'\t') {
            editor_accept_autocomplete(ed);
            return true;
        }
        if (ev->ch == 14) { /* Ctrl+N */
            ac_next(ac);
            return true;
        }
        if (ev->ch == 16) { /* Ctrl+P */
            ac_prev(ac);
            return true;
        }
        if (ev->ch == L'\n' || ev->ch == L'\r') {
            Suggestion *sel = ac_selected(ac);
            if (sel && (sel->word_len != ac->prefix_len ||
                        wmemcmp(sel->word, ac->prefix,
                                ac->prefix_len) != 0)) {
                editor_accept_autocomplete(ed);
                return true;
            }
            dismiss_ac(ed);
            return false; /* fall through to newline */
        }
        return false; /* fall through to normal insert handling */
    }

    /* Special keys when AC active */
    switch (ev->key) {
    case KEY_DOWN:
        ac_next(ac);
        return true;
    case KEY_UP:
        ac_prev(ac);
        return true;
    }
    return false;
}

static void handle_special_keys(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int height = 0, width = 0;

    switch (ev->key) {
    case KEY_UP:
        dismiss_ac(ed);
        pane_move_up(pane);
        break;
    case KEY_DOWN:
        dismiss_ac(ed);
        pane_move_down(pane);
        break;
    case KEY_LEFT:
        dismiss_ac(ed);
        pane_move_left(pane);
        break;
    case KEY_RIGHT:
        dismiss_ac(ed);
        pane_move_right(pane);
        break;
    case KEY_BACKSPACE:
        buffer_delete_char(buf, pane->cursor_row, pane->cursor_col,
                           &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        editor_trigger_autocomplete(ed);
        break;
    case KEY_DC:
        dismiss_ac(ed);
        buffer_delete_char_forward(buf, pane->cursor_row, pane->cursor_col);
        editor_schedule_auto_save(ed);
        break;
    case KEY_BTAB:
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
        dismiss_ac(ed);
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_up(pane, height);
        break;
    case KEY_NPAGE:
        dismiss_ac(ed);
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_down(pane, height);
        break;
    }
}

static void handle_char_keys(Editor *ed, wchar_t ch) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    int height = 0, width = 0;

    if (ch == 27) { /* Escape */
        dismiss_ac(ed);
        ed->mode = MODE_NORMAL;
        history_commit_session(ed->history, buf->lines,
                               buf->line_lens, buf->line_count);
        input_state_reset(ed->input_state);
        if (pane->cursor_col > 0) pane->cursor_col--;
        return;
    }

    if (ch == 127 || ch == 8) { /* Backspace */
        buffer_delete_char(buf, pane->cursor_row, pane->cursor_col,
                           &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        editor_trigger_autocomplete(ed);
        return;
    }

    if (ch == L'\n' || ch == L'\r') { /* Enter */
        dismiss_ac(ed);
        buffer_insert_newline_with_indent(buf, pane->cursor_row, pane->cursor_col,
                                         &pane->cursor_row, &pane->cursor_col);
        editor_schedule_auto_save(ed);
        return;
    }

    if (ch == L'\t') { /* Tab (no AC active — AC tab handled earlier) */
        pane->cursor_col = buffer_insert_tab(buf, pane->cursor_row,
                                             pane->cursor_col);
        editor_schedule_auto_save(ed);
        editor_trigger_autocomplete(ed);
        return;
    }

    if (ch == 4) { /* Ctrl+D */
        dismiss_ac(ed);
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_down(pane, height);
        return;
    }

    if (ch == 21) { /* Ctrl+U */
        dismiss_ac(ed);
        ed->screen->get_size(ed->screen->impl, &width, &height);
        pane_page_up(pane, height);
        return;
    }

    /* Regular character */
    if (ch >= 32) {
        pane->cursor_col = buffer_insert_char(buf, pane->cursor_row,
                                              pane->cursor_col, ch);
        editor_schedule_auto_save(ed);
        editor_trigger_autocomplete(ed);
    }
}

bool handle_insert_mode(Editor *ed, EditorEvent *ev) {
    if (ev->type != EV_KEY) return false;

    /* Autocomplete navigation takes priority */
    if (handle_ac_keys(ed, ev)) return false;

    if (!ev->is_char)
        handle_special_keys(ed, ev);
    else
        handle_char_keys(ed, ev->ch);

    return false;
}
