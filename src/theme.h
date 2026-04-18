#ifndef EF_THEME_H
#define EF_THEME_H

#include "mode.h"
#include <stdbool.h>

/* Color pair indices for ncurses */
enum {
    PAIR_DEFAULT = 0,
    PAIR_LINE_NUM,
    PAIR_WRAP_INDIC,
    PAIR_SELECTION,
    PAIR_SEARCH_MATCH,
    PAIR_CURRENT_MATCH,
    PAIR_NORMAL_LINENUM,
    PAIR_INSERT_LINENUM,
    PAIR_VISUAL_LINENUM,
    PAIR_SEARCH_BAR,
    PAIR_SEARCH_BAR_NOMATCH,
    PAIR_REPLACE_BAR,
    PAIR_REPLACE_BAR_NOMATCH,
    PAIR_AUTOCOMPLETE_NORMAL,
    PAIR_AUTOCOMPLETE_SELECTED,
    PAIR_SEPARATOR,
    PAIR_READONLY_LINENUM,
    PAIR_COUNT
};

/* Initialize color pairs. Call after ncurses init. */
void theme_init(void);

/* Returns the color pair for the current-line gutter in a pane.
   Active panes get mode-specific highlighting; inactive panes get plain. */
int theme_gutter_current_line_pair(bool is_active, Mode m);

/* Allocate/return a color pair for a syntax foreground color on default bg.
   Safe to call multiple times with the same fg — caches internally. */
short theme_syntax_pair(short fg_color);

#endif /* EF_THEME_H */
