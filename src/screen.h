#ifndef EF_SCREEN_H
#define EF_SCREEN_H

#include "mode.h"
#include "input.h"
#include "pane.h"

/* SplitMode determines pane arrangement. */
typedef enum {
    SPLIT_HORIZONTAL = 0,
    SPLIT_VERTICAL
} SplitMode;

/* EditorEvent from screen poll. */
typedef enum {
    EV_KEY = 0,
    EV_RESIZE,
    EV_NONE
} EventType;

typedef struct {
    EventType type;
    int       key;       /* ncurses key code or char */
    wchar_t   ch;        /* wide character if printable */
    bool      is_char;   /* true if ch is valid printable */
    bool      is_paste;  /* true when inside bracketed paste */
} EditorEvent;

/* PaneLayout represents computed layout for a single pane viewport. */
typedef struct {
    int pane_index;
    int start_x;
    int start_y;
    int width;
    int height;
} PaneLayout;

/* ScreenVTable abstracts terminal screen operations (dependency inversion). */
typedef struct {
    void (*render)(void *self, Pane **panes, int npanes, int active,
                   Mode mode, InputState *input, SplitMode split);
    int  (*poll_event)(void *self, EditorEvent *ev);
    void (*get_size)(void *self, int *w, int *h);
    void (*sync)(void *self);
    void (*close)(void *self);
    void (*suspend)(void *self);
    void (*resume)(void *self);
    void *impl;
} ScreenVTable;

/* NcursesScreen is the real terminal implementation. */
ScreenVTable *ncurses_screen_new(void);
void          ncurses_screen_free(ScreenVTable *scr);

/* Layout calculation (used by both real screen and tests). */
PaneLayout *calculate_pane_layout_horizontal(int num_panes, int width,
                                             int height, int start_y,
                                             int *out_count);
PaneLayout *calculate_pane_layout_vertical(int num_panes, int width,
                                           int height, int start_y,
                                           int *out_count);

/* Line number width calculation. */
int line_number_width(int total_lines);

/* Cursor screen position calculation. */
void calc_cursor_screen_pos(wchar_t **lines, const int *line_lens,
                            int cursor_row, int cursor_col,
                            int scroll_offset, int pane_width,
                            int line_num_width, int tab_stop,
                            int *screen_x, int *screen_y);

#endif /* EF_SCREEN_H */
