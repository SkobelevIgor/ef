#include "editor.h"
#include "xalloc.h"
#include "normal.h"
#include "insert.h"
#include "visual.h"
#include "syntax.h"

#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

/* --- Lifecycle ----------------------------------------------------------- */

Editor *editor_new_with_deps(ScreenVTable *screen,
                             Buffer **buffers, Pane **panes, int pane_count,
                             SplitMode split_mode) {
    Editor *ed = xcalloc(1, sizeof(Editor));
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
                ? xstrdup(buf->filename) : NULL;
            ed->buffer_count++;
        }
    }
    ed->pane_count = pane_count;
    ed->watcher = file_watcher_new();
    for (int i = 0; i < ed->buffer_count; i++) {
        if (ed->buffer_registry[i]->filename)
            ed->watcher->watch(ed->watcher->impl,
                               ed->buffer_registry[i]->filename);
    }
    return ed;
}

static void setup_buffer_highlighting(EditorConfig *cfg, Buffer *buf,
                                      const char *forced_ft) {
    if (!cfg) return;

    const char *ft = forced_ft ? forced_ft
                               : config_detect_file_type(cfg, buf->filename);
    if (!ft) return;

    free(buf->file_type);
    buf->file_type = xstrdup(ft);

    const EditorFileTypeConfig *ftc = config_get_file_type(cfg, ft);
    if (!ftc) return;

    if (ftc->tab_stop > 0) buf->config.tab_stop = ftc->tab_stop;
    if (ftc->shift_width > 0) buf->config.shift_width = ftc->shift_width;
    buf->config.auto_indentation = ftc->auto_indentation;
    buf->config.expand_tab = ftc->expand_tab;

    highlight_cache_free(buf->highlight_cache);
    buf->highlight_cache = NULL;
    if (!ftc->syntax_highlighting || ftc->rule_count == 0) return;

    SyntaxHighlighter *h = syntax_highlighter_new(ftc->syntax_rules,
                                                   ftc->rule_count);
    buf->highlight_cache = highlight_cache_new(h);
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
    ed->owns_screen = true;

    /* Load config and set up syntax highlighting */
    ed->config = config_load(NULL);
    if (ed->config) {
        for (int i = 0; i < ed->buffer_count; i++)
            setup_buffer_highlighting(ed->config, ed->buffer_registry[i], NULL);
    }

    return ed;
}

void editor_apply_forced_highlighting(Editor *ed, const char *file_type) {
    if (!ed || !file_type) return;
    free(ed->forced_file_type);
    ed->forced_file_type = xstrdup(file_type);
    if (!ed->config) return;
    for (int i = 0; i < ed->buffer_count; i++)
        setup_buffer_highlighting(ed->config, ed->buffer_registry[i], file_type);
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
    free(ed->forced_file_type);
    file_watcher_free(ed->watcher);
    if (ed->owns_screen)
        ncurses_screen_free(ed->screen);
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
    if (ed->read_only) return;
    Pane *p = editor_active_pane(ed);
    Buffer *buf = p->buffer;
    history_start_session(ed->history, buf, p->cursor_row, p->cursor_col);
    ed->mode = MODE_INSERT;
}

/* --- Auto-save (simplified: immediate save) ------------------------------ */

void editor_schedule_auto_save(Editor *ed) {
    if (ed->read_only) return;
    editor_save_all_modified(ed);
}

void editor_save_all_modified(Editor *ed) {
    if (ed->read_only) return;
    for (int i = 0; i < ed->buffer_count; i++) {
        Buffer *buf = ed->buffer_registry[i];
        if (buf->modified) {
            buffer_save(buf);
            if (ed->watcher && buf->filename)
                ed->watcher->update_mod_time(
                    ed->watcher->impl, buf->filename);
        }
    }
}

/* --- Key handling -------------------------------------------------------- */

bool editor_handle_key(Editor *ed, EditorEvent *ev) {
    /* During paste, only insert mode should process characters */
    if (ev->is_paste && ed->mode != MODE_INSERT) {
        return false;
    }

    /* F10 = quit */
    if (ev->type == EV_KEY && !ev->is_char && ev->key == KEY_F(10)) {
        if (!ed->read_only) editor_save_all_modified(ed);
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

/* --- File watching ------------------------------------------------------- */

static Buffer *find_buffer_by_filename(Editor *ed, const char *fn) {
    for (int i = 0; i < ed->buffer_count; i++) {
        if (ed->buffer_registry[i]->filename
            && strcmp(ed->buffer_registry[i]->filename, fn) == 0)
            return ed->buffer_registry[i];
    }
    return NULL;
}

void editor_handle_file_change(Editor *ed, const char *filename) {
    if (!ed || !filename) return;
    Buffer *buf = find_buffer_by_filename(ed, filename);
    if (!buf) return;

    if (ed->mode == MODE_INSERT) {
        Buffer *ab = editor_active_buffer(ed);
        history_commit_session(ed->history,
                               ab->lines, ab->line_lens,
                               ab->line_count);
        ed->mode = MODE_NORMAL;
    }

    Pane *pane = editor_active_pane(ed);
    int *old_lens;
    wchar_t **old = buffer_copy_lines(
        buf->lines, buf->line_lens,
        buf->line_count, &old_lens);
    int old_count = buf->line_count;
    int row = pane->cursor_row, col = pane->cursor_col;

    if (buffer_load(buf) != 0) {
        buffer_free_lines(old, old_lens, old_count);
        return;
    }

    Change *c = change_new(CHANGE_REPLACE, buf, row, col);
    c->text = buffer_copy_lines(
        buf->lines, buf->line_lens,
        buf->line_count, &c->text_lens);
    c->text_count = buf->line_count;
    c->old_text = old;
    c->old_text_lens = old_lens;
    c->old_text_count = old_count;
    history_push(ed->history, c);

    for (int i = 0; i < ed->pane_count; i++) {
        if (ed->panes[i]->buffer == buf)
            pane_clamp_cursor(ed->panes[i]);
    }
}

void editor_check_file_changes(Editor *ed) {
    if (!ed->watcher) return;
    const char *changed =
        ed->watcher->check(ed->watcher->impl);
    if (changed)
        editor_handle_file_change(ed, changed);
}

/* --- Main loop ----------------------------------------------------------- */

int editor_run(Editor *ed) {
    while (1) {
        ed->screen->render(ed->screen->impl, ed->panes, ed->pane_count,
                           ed->active_pane_idx, ed->mode, ed->input_state,
                           ed->split_mode, ed->read_only);
        EditorEvent ev;
        ed->screen->poll_event(ed->screen->impl, &ev);

        if (ev.type == EV_NONE) {
            editor_check_file_changes(ed);
            continue;
        }
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
