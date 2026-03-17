#include "editor.h"
#include "xalloc.h"

#include <stdlib.h>
#include <ncurses.h>

static void ensure_session(Editor *ed, WidgetState *w) {
    Pane *pane = editor_active_pane(ed);
    switch (w->kind) {
    case WIDGET_SEARCH:
        if (!w->search_session)
            w->search_session = widget_session_new(pane->cursor_row, pane->cursor_col);
        break;
    case WIDGET_FIND_REPLACE:
        if (!w->find_replace_session)
            w->find_replace_session = widget_session_new(pane->cursor_row, pane->cursor_col);
        break;
    default: break;
    }
}

static void refresh_session_matches(Editor *ed, WidgetSession *s) {
    if (!s || s->query_len == 0) return;
    Buffer *buf = editor_active_buffer(ed);
    free(s->matches);
    s->matches = buffer_find_all_matches(buf, s->query, s->query_len, &s->match_count);
    s->no_matches = (s->match_count == 0);
    if (s->match_count == 0) { s->current_index = -1; return; }
    if (s->current_index >= s->match_count) {
        s->current_index = find_first_match_after_cursor(
            s->matches, s->match_count, s->cursor_row, s->cursor_col);
    }
}

static void switch_to_widget(Editor *ed, WidgetKind kind) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;

    /* Save current session's cursor */
    WidgetSession *cur = widget_current_session(w);
    if (cur) {
        cur->cursor_row = pane->cursor_row;
        cur->cursor_col = pane->cursor_col;
    }

    w->kind = kind;
    w->focus = FOCUS_FIND_BAR;
    ensure_session(ed, w);
    refresh_session_matches(ed, widget_current_session(w));

    WidgetSession *session = widget_current_session(w);
    if (session) {
        pane->cursor_row = session->cursor_row;
        pane->cursor_col = session->cursor_col;
    }
}

void editor_open_search_widget(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    if (!w) {
        pane->widget = widget_state_new(WIDGET_SEARCH, pane->cursor_row, pane->cursor_col);
        return;
    }
    if (w->kind == WIDGET_SEARCH) {
        /* Cycle focus: FindBar <-> Editor */
        w->focus = (w->focus == FOCUS_FIND_BAR) ? FOCUS_EDITOR : FOCUS_FIND_BAR;
        return;
    }
    switch_to_widget(ed, WIDGET_SEARCH);
}

void editor_open_find_replace_widget(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    if (!w) {
        pane->widget = widget_state_new(WIDGET_FIND_REPLACE, pane->cursor_row, pane->cursor_col);
        return;
    }
    if (w->kind == WIDGET_FIND_REPLACE) {
        w->focus = FOCUS_FIND_BAR;
        return;
    }
    switch_to_widget(ed, WIDGET_FIND_REPLACE);
}

void editor_close_widget(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    if (!w) return;
    if (!w->confirmed) {
        pane->cursor_row = w->anchor_row;
        pane->cursor_col = w->anchor_col;
    }
    widget_state_free(w);
    pane->widget = NULL;
}

static void widget_navigate_to_match(Editor *ed, int row, int col) {
    Pane *pane = editor_active_pane(ed);
    WidgetSession *s = widget_current_session(pane->widget);
    if (!s || s->match_count == 0) return;
    int idx = find_first_match_after_cursor(s->matches, s->match_count, row, col);
    s->current_index = idx;
    if (idx >= 0) {
        pane->cursor_row = s->matches[idx].row;
        pane->cursor_col = s->matches[idx].col;
    }
}

static void widget_navigate_to_current(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetSession *s = widget_current_session(pane->widget);
    if (!s || s->current_index < 0 || s->current_index >= s->match_count) return;
    pane->cursor_row = s->matches[s->current_index].row;
    pane->cursor_col = s->matches[s->current_index].col;
}

void editor_widget_next_match(Editor *ed) {
    WidgetSession *s = widget_current_session(editor_active_pane(ed)->widget);
    if (!s || s->match_count == 0) return;
    s->current_index = (s->current_index + 1) % s->match_count;
    widget_navigate_to_current(ed);
}

void editor_widget_prev_match(Editor *ed) {
    WidgetSession *s = widget_current_session(editor_active_pane(ed)->widget);
    if (!s || s->match_count == 0) return;
    s->current_index--;
    if (s->current_index < 0) s->current_index = s->match_count - 1;
    widget_navigate_to_current(ed);
}

void editor_update_widget_matches(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetSession *s = widget_current_session(pane->widget);
    if (!s) return;

    Buffer *buf = pane->buffer;
    if (s->query_len == 0) {
        free(s->matches); s->matches = NULL;
        s->match_count = 0; s->current_index = -1; s->no_matches = false;
        return;
    }

    free(s->matches);
    s->matches = buffer_find_all_matches(buf, s->query, s->query_len, &s->match_count);
    s->no_matches = (s->match_count == 0);

    if (s->match_count == 0) { s->current_index = -1; return; }

    /* Auto-navigate at 2+ chars */
    if (s->query_len >= 2)
        widget_navigate_to_match(ed, s->cursor_row, s->cursor_col);
}

void editor_widget_replace_current(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    WidgetSession *s = widget_current_session(w);
    if (!s || s->current_index < 0 || s->current_index >= s->match_count) return;

    Buffer *buf = pane->buffer;
    SearchMatch match = s->matches[s->current_index];

    /* Snapshot for undo */
    int *old_lens;
    wchar_t **old_lines = buffer_copy_lines(buf->lines, buf->line_lens,
                                            buf->line_count, &old_lens);
    int old_count = buf->line_count;

    /* Replace matched text on the line */
    wchar_t *line = buf->lines[match.row];
    int line_len = buf->line_lens[match.row];
    int end_col = match.col + match.length;
    if (end_col > line_len) end_col = line_len;

    int new_line_len = line_len - (end_col - match.col) + s->replace_len;
    wchar_t *new_line = xmalloc(sizeof(wchar_t) * (new_line_len + 1));
    wmemcpy(new_line, line, match.col);
    wmemcpy(new_line + match.col, s->replace_text, s->replace_len);
    wmemcpy(new_line + match.col + s->replace_len, line + end_col, line_len - end_col);
    new_line[new_line_len] = L'\0';
    buffer_set_line(buf, match.row, new_line, new_line_len);
    buf->modified = true;
    w->confirmed = true;

    /* Record undo */
    Change *c = change_new(CHANGE_REPLACE, buf, match.row, match.col);
    c->text = buffer_copy_lines(buf->lines, buf->line_lens, buf->line_count, &c->text_lens);
    c->text_count = buf->line_count;
    c->old_text = old_lines;
    c->old_text_lens = old_lens;
    c->old_text_count = old_count;
    history_push(ed->history, c);
    editor_schedule_auto_save(ed);

    int cursor_row = pane->cursor_row;
    int cursor_col = pane->cursor_col;

    /* Recalculate matches */
    free(s->matches);
    s->matches = buffer_find_all_matches(buf, s->query, s->query_len, &s->match_count);
    s->no_matches = (s->match_count == 0);

    if (s->match_count > 0) {
        widget_navigate_to_match(ed, cursor_row, cursor_col);
    } else {
        s->current_index = -1;
    }
}

static bool handle_widget_find_bar_key(Editor *ed, EditorEvent *ev) {
    WidgetState *w = editor_active_pane(ed)->widget;
    WidgetSession *s = widget_current_session(w);
    if (!s) return false;

    /* Enter */
    if (ev->key == '\n' || ev->key == '\r'
        || (!ev->is_char && ev->key == KEY_ENTER)) {
        if (w->kind == WIDGET_SEARCH) {
            w->focus = FOCUS_EDITOR;
            w->confirmed = true;
        } else if (w->kind == WIDGET_FIND_REPLACE) {
            w->focus = FOCUS_REPLACE_BAR;
        }
        return false;
    }

    /* Backspace */
    if (ev->key == KEY_DEL || ev->key == KEY_BS
        || (!ev->is_char && ev->key == KEY_BACKSPACE)) {
        widget_session_backspace_query(s);
        editor_update_widget_matches(ed);
        return false;
    }

    /* Printable characters only */
    if (ev->is_char && ev->ch >= 32) {
        widget_session_append_query(s, ev->ch);
        editor_update_widget_matches(ed);
    }
    return false;
}

static bool handle_widget_replace_bar_key(Editor *ed, EditorEvent *ev) {
    WidgetState *w = editor_active_pane(ed)->widget;
    WidgetSession *s = widget_current_session(w);
    if (!s) return false;

    /* Enter: move focus to editor */
    if (ev->key == '\n' || ev->key == '\r'
        || (!ev->is_char && ev->key == KEY_ENTER)) {
        w->focus = FOCUS_EDITOR;
        w->confirmed = true;
        return false;
    }

    /* Backspace */
    if (ev->key == KEY_DEL || ev->key == KEY_BS
        || (!ev->is_char && ev->key == KEY_BACKSPACE)) {
        widget_session_backspace_replace(s);
        return false;
    }

    /* Printable characters only */
    if (ev->is_char && ev->ch >= 32) {
        widget_session_append_replace(s, ev->ch);
    }
    return false;
}

static bool handle_widget_editor_key(Editor *ed, EditorEvent *ev) {
    WidgetState *w = editor_active_pane(ed)->widget;

    /* n/N navigation */
    if (ev->is_char) {
        switch (ev->ch) {
        case L'n': editor_widget_next_match(ed); return false;
        case L'N': editor_widget_prev_match(ed); return false;
        }
    }

    /* Enter: replace current match in FindReplace mode */
    if (ev->key == '\n' || ev->key == '\r'
        || (!ev->is_char && ev->key == KEY_ENTER)) {
        if (w->kind == WIDGET_FIND_REPLACE)
            editor_widget_replace_current(ed);
        return false;
    }
    return false;
}

bool editor_handle_widget_mode(Editor *ed, EditorEvent *ev) {
    WidgetState *w = editor_active_pane(ed)->widget;
    if (!w) return false;

    /* Escape: close widget (comes as is_char=true from ncurses) */
    if (ev->key == KEY_ESC) {
        editor_close_widget(ed);
        return false;
    }

    /* F3/F4: always special keys (is_char=false) */
    if (!ev->is_char) {
        if (ev->key == KEY_F(3)) {
            editor_open_find_replace_widget(ed);
            return false;
        }
        if (ev->key == KEY_F(4)) {
            editor_open_search_widget(ed);
            return false;
        }
    }

    /* Tab: toggle FindBar <-> ReplaceBar in FindReplace */
    if (ev->key == '\t' && w->kind == WIDGET_FIND_REPLACE) {
        w->focus = (w->focus == FOCUS_FIND_BAR)
            ? FOCUS_REPLACE_BAR : FOCUS_FIND_BAR;
        return false;
    }

    switch (w->focus) {
    case FOCUS_FIND_BAR:    return handle_widget_find_bar_key(ed, ev);
    case FOCUS_REPLACE_BAR: return handle_widget_replace_bar_key(ed, ev);
    case FOCUS_EDITOR:      return handle_widget_editor_key(ed, ev);
    }
    return false;
}
