#include "editor.h"
#include "normal.h"
#include "insert.h"
#include "visual.h"
#include "runes.h"
#include "autocomplete.h"
#include "syntax.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

/* --- Lifecycle ----------------------------------------------------------- */

Editor *editor_new_with_deps(ScreenVTable *screen,
                             Buffer **buffers, Pane **panes, int pane_count,
                             SplitMode split_mode) {
    Editor *ed = calloc(1, sizeof(Editor));
    if (!ed) return NULL;
    ed->screen = screen;
    ed->split_mode = split_mode;
    ed->mode = MODE_NORMAL;
    ed->input_state = input_state_new();
    ed->clipboard = clipboard_new();
    ed->history = history_new(DEFAULT_HISTORY_SIZE);

    for (int i = 0; i < pane_count && i < MAX_PANES; i++) {
        ed->panes[i] = panes[i];
        /* Register buffer if not already present */
        Buffer *buf = panes[i]->buffer;
        bool found = false;
        for (int j = 0; j < ed->buffer_count; j++) {
            if (ed->buffer_registry[j] == buf) { found = true; break; }
        }
        if (!found && ed->buffer_count < MAX_BUFFERS) {
            ed->buffer_registry[ed->buffer_count] = buf;
            ed->buffer_paths[ed->buffer_count] = buf->filename
                ? strdup(buf->filename) : NULL;
            ed->buffer_count++;
        }
    }
    ed->pane_count = pane_count;
    return ed;
}

static void setup_buffer_highlighting(EditorConfig *cfg, Buffer *buf) {
    if (!cfg || !buf->filename) return;

    const char *ft = config_detect_file_type(cfg, buf->filename);
    if (!ft) return;

    free(buf->file_type);
    buf->file_type = strdup(ft);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, ft);
    if (!ftc) return;

    if (ftc->tab_stop > 0) buf->config.tab_stop = ftc->tab_stop;
    if (ftc->shift_width > 0) buf->config.shift_width = ftc->shift_width;
    buf->config.auto_indentation = ftc->auto_indentation;
    buf->config.expand_tab = ftc->expand_tab;

    if (!ftc->syntax_highlighting || ftc->rule_count == 0) return;

    SyntaxHighlighter *h = syntax_highlighter_new(ftc->syntax_rules,
                                                   ftc->rule_count);
    buf->highlight_cache = highlight_cache_new(h);
    log_write("syntax: loaded %d rules for %s (%s)",
              ftc->rule_count, ft, buf->filename);
}

Editor *editor_new(FileInfo *files, int file_count, SplitMode split_mode) {
    ScreenVTable *scr = ncurses_screen_new();
    if (!scr) return NULL;

    Buffer *buffers[MAX_BUFFERS];
    Pane *panes[MAX_PANES];
    int buf_count = 0;
    int pane_count = 0;

    for (int i = 0; i < file_count && pane_count < MAX_PANES; i++) {
        /* Check if buffer exists */
        Buffer *buf = NULL;
        for (int j = 0; j < buf_count; j++) {
            if (buffers[j]->filename
                && strcmp(buffers[j]->filename, files[i].filename) == 0) {
                buf = buffers[j];
                break;
            }
        }
        if (!buf) {
            buf = buffer_new_from_file(files[i].filename);
            if (!buf) continue;
            buffers[buf_count++] = buf;
        }
        Pane *p;
        if (files[i].line > 0) {
            p = pane_new_at_line(buf, files[i].line);
        } else {
            p = pane_new(buf);
        }
        panes[pane_count++] = p;
    }

    Editor *ed = editor_new_with_deps(scr, buffers, panes, pane_count,
                                      split_mode);

    /* Load config and set up syntax highlighting */
    ed->config = config_load(NULL);
    if (ed->config) {
        log_write("config loaded, %d file type(s)", ed->config->file_type_count);
        for (int i = 0; i < ed->buffer_count; i++)
            setup_buffer_highlighting(ed->config, ed->buffer_registry[i]);
    }

    return ed;
}

void editor_free(Editor *ed) {
    if (!ed) return;
    for (int i = 0; i < ed->pane_count; i++) pane_free(ed->panes[i]);
    for (int i = 0; i < ed->buffer_count; i++) {
        buffer_free(ed->buffer_registry[i]);
        free(ed->buffer_paths[i]);
    }
    input_state_free(ed->input_state);
    clipboard_free(ed->clipboard);
    history_free(ed->history);
    config_free(ed->config);
    /* Screen freed by caller or here */
    free(ed);
}

/* --- Shortcuts ----------------------------------------------------------- */

Pane *editor_active_pane(Editor *ed) {
    return ed->panes[ed->active_pane_idx];
}

Buffer *editor_active_buffer(Editor *ed) {
    return editor_active_pane(ed)->buffer;
}

/* --- Insert mode entry --------------------------------------------------- */

void editor_enter_insert_mode(Editor *ed) {
    Pane *p = editor_active_pane(ed);
    Buffer *buf = p->buffer;
    history_start_session(ed->history, buf, p->cursor_row, p->cursor_col);
    ed->mode = MODE_INSERT;
}

/* --- Auto-save (simplified: immediate save) ------------------------------ */

void editor_schedule_auto_save(Editor *ed) {
    editor_save_all_modified(ed);
}

void editor_save_all_modified(Editor *ed) {
    for (int i = 0; i < ed->buffer_count; i++) {
        if (ed->buffer_registry[i]->modified) {
            buffer_save(ed->buffer_registry[i]);
        }
    }
}

/* --- Undo/Redo ----------------------------------------------------------- */

static void apply_remove_text(Buffer *buf, Change *c) {
    if (c->line_deletion) {
        int end = c->row + c->text_count;
        if (end <= buf->line_count) {
            buffer_splice_lines(buf, c->row, end - c->row, NULL, NULL, 0);
        }
        if (buf->line_count == 0) {
            buffer_ensure_lines(buf, 1);
            wchar_t *empty = malloc(sizeof(wchar_t));
            empty[0] = L'\0';
            buf->lines[0] = empty;
            buf->line_lens[0] = 0;
            buf->line_caps[0] = 1;
            buf->line_count = 1;
        }
        return;
    }
    if (c->text_count == 1) {
        int tl = c->text_lens[0];
        if (c->col + tl <= buf->line_lens[c->row]) {
            int new_len;
            wchar_t *nl = remove_runes(buf->lines[c->row], buf->line_lens[c->row],
                                       c->col, c->col + tl, &new_len);
            buffer_set_line(buf, c->row, nl, new_len);
        }
    } else {
        int end_row = c->row + c->text_count - 1;
        if (end_row < buf->line_count) {
            int last_text_len = c->text_lens[c->text_count - 1];
            int after_len = buf->line_lens[end_row] - last_text_len;
            if (after_len < 0) after_len = 0;
            int first_len = c->col;
            int merged_len = first_len + after_len;
            wchar_t *merged = malloc(sizeof(wchar_t) * (merged_len + 1));
            wmemcpy(merged, buf->lines[c->row], first_len);
            wmemcpy(merged + first_len,
                    buf->lines[end_row] + last_text_len, after_len);
            merged[merged_len] = L'\0';
            buffer_splice_lines(buf, c->row, end_row - c->row + 1,
                                &merged, &merged_len, 1);
        }
    }
}

static void apply_insert_text(Buffer *buf, Change *c) {
    if (c->line_deletion) {
        int *lens;
        wchar_t **lines = buffer_copy_lines(c->text, c->text_lens,
                                            c->text_count, &lens);
        buffer_splice_lines(buf, c->row, 0, lines, lens, c->text_count);
        free(lens);
        free(lines);
        return;
    }
    if (c->text_count == 1) {
        int new_len;
        wchar_t *nl = insert_runes(buf->lines[c->row], buf->line_lens[c->row],
                                   c->col, c->text[0], c->text_lens[0], &new_len);
        buffer_set_line(buf, c->row, nl, new_len);
    } else {
        int first_part_len = c->col;
        int rest_len = buf->line_lens[c->row] - c->col;
        if (rest_len < 0) rest_len = 0;

        int new_first_len = first_part_len + c->text_lens[0];
        wchar_t *new_first = malloc(sizeof(wchar_t) * (new_first_len + 1));
        wmemcpy(new_first, buf->lines[c->row], first_part_len);
        wmemcpy(new_first + first_part_len, c->text[0], c->text_lens[0]);
        new_first[new_first_len] = L'\0';

        int last_text_len = c->text_lens[c->text_count - 1];
        int new_last_len = last_text_len + rest_len;
        wchar_t *new_last = malloc(sizeof(wchar_t) * (new_last_len + 1));
        wmemcpy(new_last, c->text[c->text_count - 1], last_text_len);
        wmemcpy(new_last + last_text_len,
                buf->lines[c->row] + c->col, rest_len);
        new_last[new_last_len] = L'\0';

        int block_count = c->text_count;
        wchar_t **block = malloc(sizeof(wchar_t *) * block_count);
        int *block_lens = malloc(sizeof(int) * block_count);
        block[0] = new_first;
        block_lens[0] = new_first_len;
        for (int i = 1; i < block_count - 1; i++) {
            block[i] = malloc(sizeof(wchar_t) * (c->text_lens[i] + 1));
            wmemcpy(block[i], c->text[i], c->text_lens[i]);
            block[i][c->text_lens[i]] = L'\0';
            block_lens[i] = c->text_lens[i];
        }
        block[block_count - 1] = new_last;
        block_lens[block_count - 1] = new_last_len;

        buffer_splice_lines(buf, c->row, 1, block, block_lens, block_count);
        free(block);
        free(block_lens);
    }
}

void editor_undo(Editor *ed) {
    Change *c = history_undo(ed->history);
    if (!c) return;

    Buffer *buf = c->buffer ? c->buffer : editor_active_buffer(ed);
    Pane *pane = editor_active_pane(ed);

    switch (c->type) {
    case CHANGE_INSERT:
        apply_remove_text(buf, c);
        pane->cursor_row = c->row;
        pane->cursor_col = c->col;
        buf->modified = true;
        break;
    case CHANGE_DELETE:
        apply_insert_text(buf, c);
        pane->cursor_row = c->row;
        pane->cursor_col = c->col;
        buf->modified = true;
        break;
    case CHANGE_REPLACE:
        if (c->old_text) {
            int *lens;
            wchar_t **lines = buffer_copy_lines(c->old_text, c->old_text_lens,
                                                c->old_text_count, &lens);
            /* Replace all buffer lines */
            for (int i = 0; i < buf->line_count; i++) free(buf->lines[i]);
            buf->line_count = 0;
            buffer_ensure_lines(buf, c->old_text_count);
            for (int i = 0; i < c->old_text_count; i++) {
                buf->lines[i] = lines[i];
                buf->line_lens[i] = lens[i];
                buf->line_caps[i] = lens[i] + 1;
            }
            buf->line_count = c->old_text_count;
            free(lines);
            free(lens);
        }
        pane->cursor_row = c->row;
        pane->cursor_col = c->col;
        pane_clamp_cursor(pane);
        buf->modified = true;
        break;
    }

    pane_clamp_cursor_col(pane);
    editor_schedule_auto_save(ed);
}

void editor_redo(Editor *ed) {
    Change *c = history_redo(ed->history);
    if (!c) return;

    Buffer *buf = c->buffer ? c->buffer : editor_active_buffer(ed);
    Pane *pane = editor_active_pane(ed);

    switch (c->type) {
    case CHANGE_INSERT:
        apply_insert_text(buf, c);
        if (c->text_count == 1) {
            pane->cursor_col = c->col + c->text_lens[0];
        } else {
            pane->cursor_row = c->row + c->text_count - 1;
            pane->cursor_col = c->text_lens[c->text_count - 1];
        }
        buf->modified = true;
        break;
    case CHANGE_DELETE:
        apply_remove_text(buf, c);
        pane->cursor_row = c->row;
        pane->cursor_col = c->col;
        if (pane->cursor_row >= buf->line_count) {
            pane->cursor_row = buf->line_count - 1;
        }
        buf->modified = true;
        break;
    case CHANGE_REPLACE:
        if (c->text) {
            int *lens;
            wchar_t **lines = buffer_copy_lines(c->text, c->text_lens,
                                                c->text_count, &lens);
            for (int i = 0; i < buf->line_count; i++) free(buf->lines[i]);
            buf->line_count = 0;
            buffer_ensure_lines(buf, c->text_count);
            for (int i = 0; i < c->text_count; i++) {
                buf->lines[i] = lines[i];
                buf->line_lens[i] = lens[i];
                buf->line_caps[i] = lens[i] + 1;
            }
            buf->line_count = c->text_count;
            free(lines);
            free(lens);
        }
        pane->cursor_row = c->row;
        pane->cursor_col = c->col;
        pane_clamp_cursor(pane);
        buf->modified = true;
        break;
    }

    pane_clamp_cursor_col(pane);
    editor_schedule_auto_save(ed);
}

/* --- Paste --------------------------------------------------------------- */

void editor_paste_after(Editor *ed) {
    Clipboard *cb = ed->clipboard;
    if (cb->line_count == 0) return;

    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;

    int *old_lens;
    wchar_t **old_lines = buffer_copy_lines(buf->lines, buf->line_lens,
                                            buf->line_count, &old_lens);
    int old_count = buf->line_count;

    if (cb->is_line_mode) {
        for (int i = cb->line_count - 1; i >= 0; i--) {
            int len = cb->line_lens[i];
            wchar_t *line = malloc(sizeof(wchar_t) * (len + 1));
            wmemcpy(line, cb->lines[i], len);
            line[len] = L'\0';
            buffer_insert_line_after(buf, pane->cursor_row, line, len);
        }
        pane->cursor_row++;
        pane->cursor_col = 0;
    } else if (cb->line_count == 1) {
        int insert_pos = pane->cursor_col + 1;
        if (insert_pos > buf->line_lens[pane->cursor_row]) {
            insert_pos = buf->line_lens[pane->cursor_row];
        }
        int new_len;
        wchar_t *nl = insert_runes(buf->lines[pane->cursor_row],
                                   buf->line_lens[pane->cursor_row],
                                   insert_pos, cb->lines[0], cb->line_lens[0],
                                   &new_len);
        buffer_set_line(buf, pane->cursor_row, nl, new_len);
        pane->cursor_col = insert_pos + cb->line_lens[0] - 1;
        if (pane->cursor_col < 0) pane->cursor_col = 0;
        buf->modified = true;
    }

    /* Record for undo */
    Change *c = change_new(CHANGE_REPLACE, buf, pane->cursor_row, pane->cursor_col);
    c->text = buffer_copy_lines(buf->lines, buf->line_lens, buf->line_count, &c->text_lens);
    c->text_count = buf->line_count;
    c->old_text = old_lines;
    c->old_text_lens = old_lens;
    c->old_text_count = old_count;
    history_push(ed->history, c);
    editor_schedule_auto_save(ed);
}

void editor_paste_before(Editor *ed) {
    Clipboard *cb = ed->clipboard;
    if (cb->line_count == 0) return;

    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;

    int *old_lens;
    wchar_t **old_lines = buffer_copy_lines(buf->lines, buf->line_lens,
                                            buf->line_count, &old_lens);
    int old_count = buf->line_count;

    if (cb->is_line_mode) {
        for (int i = cb->line_count - 1; i >= 0; i--) {
            int len = cb->line_lens[i];
            wchar_t *line = malloc(sizeof(wchar_t) * (len + 1));
            wmemcpy(line, cb->lines[i], len);
            line[len] = L'\0';
            buffer_insert_line_before(buf, pane->cursor_row, line, len);
        }
        pane->cursor_col = 0;
    } else if (cb->line_count == 1) {
        int insert_pos = pane->cursor_col;
        int new_len;
        wchar_t *nl = insert_runes(buf->lines[pane->cursor_row],
                                   buf->line_lens[pane->cursor_row],
                                   insert_pos, cb->lines[0], cb->line_lens[0],
                                   &new_len);
        buffer_set_line(buf, pane->cursor_row, nl, new_len);
        pane->cursor_col = insert_pos + cb->line_lens[0] - 1;
        if (pane->cursor_col < 0) pane->cursor_col = 0;
        buf->modified = true;
    }

    Change *c = change_new(CHANGE_REPLACE, buf, pane->cursor_row, pane->cursor_col);
    c->text = buffer_copy_lines(buf->lines, buf->line_lens, buf->line_count, &c->text_lens);
    c->text_count = buf->line_count;
    c->old_text = old_lines;
    c->old_text_lens = old_lens;
    c->old_text_count = old_count;
    history_push(ed->history, c);
    editor_schedule_auto_save(ed);
}

/* --- Widget operations --------------------------------------------------- */

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
        /* Cycle focus: FindBar ↔ Editor */
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
    if (w->kind == WIDGET_FIND_REPLACE) return; /* Tab handles cycling */
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
    wchar_t *new_line = malloc(sizeof(wchar_t) * (new_line_len + 1));
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
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    WidgetSession *s = widget_current_session(w);
    if (!s) return false;

    if (ev->is_char) {
        widget_session_append_query(s, ev->ch);
        editor_update_widget_matches(ed);
        return false;
    }

    switch (ev->key) {
    case KEY_ENTER: case '\n': case '\r':
        if (w->kind == WIDGET_SEARCH) {
            w->focus = FOCUS_EDITOR;
            w->confirmed = true;
        }
        return false;
    case KEY_BACKSPACE: case 127:
        widget_session_backspace_query(s);
        editor_update_widget_matches(ed);
        return false;
    }
    return false;
}

static bool handle_widget_replace_bar_key(Editor *ed, EditorEvent *ev) {
    WidgetSession *s = widget_current_session(editor_active_pane(ed)->widget);
    if (!s) return false;

    if (ev->is_char) {
        widget_session_append_replace(s, ev->ch);
        return false;
    }

    switch (ev->key) {
    case KEY_BACKSPACE: case 127:
        widget_session_backspace_replace(s);
        return false;
    }
    return false;
}

static bool handle_widget_editor_key(Editor *ed, EditorEvent *ev) {
    WidgetState *w = editor_active_pane(ed)->widget;
    if (ev->is_char) {
        switch (ev->ch) {
        case L'n': editor_widget_next_match(ed); return false;
        case L'N': editor_widget_prev_match(ed); return false;
        }
    }
    if (!ev->is_char) {
        switch (ev->key) {
        case KEY_ENTER: case '\n': case '\r':
            if (w->kind == WIDGET_FIND_REPLACE)
                editor_widget_replace_current(ed);
            return false;
        }
    }
    return false;
}

bool editor_handle_widget_mode(Editor *ed, EditorEvent *ev) {
    Pane *pane = editor_active_pane(ed);
    WidgetState *w = pane->widget;
    if (!w) return false;

    /* Global widget keys */
    if (!ev->is_char) {
        switch (ev->key) {
        case 27: /* Escape */
            editor_close_widget(ed);
            return false;
        case KEY_F(3):
            editor_open_find_replace_widget(ed);
            return false;
        case KEY_F(4):
            editor_open_search_widget(ed);
            return false;
        case '\t':
            if (w->kind == WIDGET_FIND_REPLACE) {
                switch (w->focus) {
                case FOCUS_FIND_BAR:    w->focus = FOCUS_REPLACE_BAR; break;
                case FOCUS_REPLACE_BAR: w->focus = FOCUS_EDITOR; break;
                default:                w->focus = FOCUS_FIND_BAR; break;
                }
            }
            return false;
        }
    }

    switch (w->focus) {
    case FOCUS_FIND_BAR:    return handle_widget_find_bar_key(ed, ev);
    case FOCUS_REPLACE_BAR: return handle_widget_replace_bar_key(ed, ev);
    case FOCUS_EDITOR:      return handle_widget_editor_key(ed, ev);
    }
    return false;
}

/* --- Autocomplete -------------------------------------------------------- */

void editor_trigger_autocomplete(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    if (pane->cursor_col == 0) {
        if (ed->input_state->autocomplete) {
            ac_state_free(ed->input_state->autocomplete);
            ed->input_state->autocomplete = NULL;
        }
        return;
    }

    wchar_t *line = buf->lines[pane->cursor_row];
    int cursor_col = pane->cursor_col;
    if (cursor_col > buf->line_lens[pane->cursor_row])
        cursor_col = buf->line_lens[pane->cursor_row];

    /* Scan backwards for word start */
    int start_col = cursor_col;
    while (start_col > 0 && !ac_is_delimiter(line[start_col - 1])) start_col--;

    int prefix_len = cursor_col - start_col;
    if (prefix_len < 2) {
        if (ed->input_state->autocomplete) {
            ac_state_free(ed->input_state->autocomplete);
            ed->input_state->autocomplete = NULL;
        }
        return;
    }

    AutocompleteState *ac = ed->input_state->autocomplete;
    if (!ac) ac = ac_state_new();

    int word_count, *word_lens;
    wchar_t **words = ac_get_words(ac, buf->lines, buf->line_lens, buf->line_count,
                                   buf->mod_count, pane->cursor_row, start_col,
                                   &word_lens, &word_count);

    int sugg_count;
    Suggestion *suggs = ac_find_suggestions(words, word_lens, word_count,
                                            line + start_col, prefix_len,
                                            10, &sugg_count);

    if (sugg_count == 0) {
        ac_state_free(ac);
        ed->input_state->autocomplete = NULL;
        return;
    }

    /* Free old suggestions */
    ac_free_suggestions(ac->suggestions, ac->suggestion_count);
    free(ac->prefix);

    ac->active = true;
    ac->prefix = malloc(sizeof(wchar_t) * (prefix_len + 1));
    wmemcpy(ac->prefix, line + start_col, prefix_len);
    ac->prefix[prefix_len] = L'\0';
    ac->prefix_len = prefix_len;
    ac->prefix_col = start_col;
    ac->suggestions = suggs;
    ac->suggestion_count = sugg_count;
    ac->selected_idx = 0;
    ed->input_state->autocomplete = ac;
}

void editor_accept_autocomplete(Editor *ed) {
    AutocompleteState *ac = ed->input_state->autocomplete;
    if (!ac || ac->suggestion_count == 0) return;

    Suggestion *sel = ac_selected(ac);
    if (!sel) return;

    Buffer *buf = editor_active_buffer(ed);
    Pane *pane = editor_active_pane(ed);
    wchar_t *line = buf->lines[pane->cursor_row];
    int line_len = buf->line_lens[pane->cursor_row];

    int prefix_start = pane->cursor_col - ac->prefix_len;
    if (prefix_start < 0) prefix_start = 0;

    int new_line_len = prefix_start + sel->word_len + (line_len - pane->cursor_col);
    wchar_t *new_line = malloc(sizeof(wchar_t) * (new_line_len + 1));
    wmemcpy(new_line, line, prefix_start);
    wmemcpy(new_line + prefix_start, sel->word, sel->word_len);
    wmemcpy(new_line + prefix_start + sel->word_len,
            line + pane->cursor_col, line_len - pane->cursor_col);
    new_line[new_line_len] = L'\0';

    buffer_set_line(buf, pane->cursor_row, new_line, new_line_len);
    pane->cursor_col = prefix_start + sel->word_len;
    buf->modified = true;

    ac_state_free(ac);
    ed->input_state->autocomplete = NULL;
    editor_schedule_auto_save(ed);
}

/* --- Key handling -------------------------------------------------------- */

bool editor_handle_key(Editor *ed, EditorEvent *ev) {
    /* F10 = quit */
    if (ev->type == EV_KEY && ev->key == KEY_F(10)) {
        editor_save_all_modified(ed);
        return true;
    }

    /* F3/F4 open widgets from any mode */
    if (ev->type == EV_KEY && !ev->is_char) {
        if (ev->key == KEY_F(3)) {
            editor_open_find_replace_widget(ed);
            return false;
        }
        if (ev->key == KEY_F(4)) {
            editor_open_search_widget(ed);
            return false;
        }
    }

    /* Widget mode takes priority when active */
    if (pane_has_active_widget(editor_active_pane(ed))) {
        return editor_handle_widget_mode(ed, ev);
    }

    switch (ed->mode) {
    case MODE_NORMAL:
        return handle_normal_mode(ed, ev);
    case MODE_INSERT:
        return handle_insert_mode(ed, ev);
    case MODE_VISUAL:
        return handle_visual_mode(ed, ev);
    }
    return false;
}

/* --- Main loop ----------------------------------------------------------- */

int editor_run(Editor *ed) {
    while (1) {
        ed->screen->render(ed->screen->impl, ed->panes, ed->pane_count,
                           ed->active_pane_idx, ed->mode, ed->input_state,
                           ed->split_mode);
        EditorEvent ev;
        ed->screen->poll_event(ed->screen->impl, &ev);

        if (ev.type == EV_RESIZE) {
            ed->screen->sync(ed->screen->impl);
            continue;
        }
        if (editor_handle_key(ed, &ev)) {
            break;
        }
    }
    ed->screen->close(ed->screen->impl);
    return 0;
}
