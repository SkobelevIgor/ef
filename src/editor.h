#ifndef EF_EDITOR_H
#define EF_EDITOR_H

#include "buffer.h"
#include "pane.h"
#include "screen.h"
#include "mode.h"
#include "input.h"
#include "history.h"
#include "clipboard.h"
#include "widget.h"
#include "search.h"
#include "config.h"
#include "file_watcher.h"

#include <stdbool.h>
#include <time.h>

#define MAX_PANES    16
#define MAX_BUFFERS  64
#define MAX_MARKS    128
_Static_assert(MAX_MARKS >= 123, "MAX_MARKS must cover ASCII 'z' for mark indexing");

typedef struct {
    bool    set;
    Buffer *buffer;
    int     row;
    int     col;
} GlobalMark;

typedef struct {
    char *filename;
    int   line; /* 1-based, 0 = not specified */
} FileInfo;

typedef struct Editor {
    Buffer *buffer_registry[MAX_BUFFERS];
    char   *buffer_paths[MAX_BUFFERS]; /* abs paths for dedup */
    int     buffer_count;

    Pane *panes[MAX_PANES];
    int   pane_count;
    int   active_pane_idx;

    ScreenVTable *screen;
    bool          owns_screen; /* true when screen is heap-allocated */
    SplitMode     split_mode;
    Mode          mode;
    InputState   *input_state;
    Clipboard    *clipboard;
    History      *history;
    GlobalMark    marks[MAX_MARKS];

    struct timespec last_shift_time;
    bool            has_last_shift;

    EditorConfig       *config;
    FileWatcherVTable  *watcher;

    bool  read_only;        /* set via -r flag; blocks all mutations */
    char *forced_file_type; /* set via -e/-ext flag; overrides extension detection */
} Editor;

/* Create editor for testing (no screen init, mock-friendly). */
Editor *editor_new_with_deps(ScreenVTable *screen,
                             Buffer **buffers, Pane **panes, int pane_count,
                             SplitMode split_mode);

/* Create editor from file arguments (full init with ncurses). */
Editor *editor_new(FileInfo *files, int file_count, SplitMode split_mode);

void editor_free(Editor *ed);

/* Main event loop. Returns 0 on clean exit. */
int editor_run(Editor *ed);

/* Active pane/buffer shortcuts. */
Pane   *editor_active_pane(Editor *ed);
Buffer *editor_active_buffer(Editor *ed);

/* Handle a single key event. Returns true if should quit. */
bool editor_handle_key(Editor *ed, EditorEvent *ev);

/* Enter insert mode (start session, set mode). */
void editor_enter_insert_mode(Editor *ed);

/* Schedule auto-save. */
void editor_schedule_auto_save(Editor *ed);

/* Save all modified buffers. */
void editor_save_all_modified(Editor *ed);

/* Undo/redo. */
void editor_undo(Editor *ed);
void editor_redo(Editor *ed);

/* Paste operations. */
void editor_paste_after(Editor *ed);
void editor_paste_before(Editor *ed);

/* Widget operations. */
void editor_open_search_widget(Editor *ed);
void editor_open_find_replace_widget(Editor *ed);
void editor_close_widget(Editor *ed);
bool editor_handle_widget_mode(Editor *ed, EditorEvent *ev);

/* Widget internal: match navigation */
void editor_widget_next_match(Editor *ed);
void editor_widget_prev_match(Editor *ed);
void editor_widget_replace_current(Editor *ed);
void editor_update_widget_matches(Editor *ed);

/* Autocomplete operations. */
void editor_trigger_autocomplete(Editor *ed);
void editor_accept_autocomplete(Editor *ed);

/* Force syntax highlighting for all buffers regardless of filename extension. */
void editor_apply_forced_highlighting(Editor *ed, const char *file_type);

/* File watching. */
void editor_check_file_changes(Editor *ed);
void editor_handle_file_change(Editor *ed, const char *filename);

#endif /* EF_EDITOR_H */
