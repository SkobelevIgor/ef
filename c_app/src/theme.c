#include "theme.h"

#include <ncurses.h>

void theme_init(void) {
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    init_pair(PAIR_LINE_NUM,              COLOR_GREEN,   -1);
    init_pair(PAIR_WRAP_INDIC,            8,             -1); /* dark gray */
    init_pair(PAIR_SELECTION,             -1,            -1); /* reverse applied via attr */
    init_pair(PAIR_SEARCH_MATCH,          COLOR_BLACK,   COLOR_YELLOW);
    init_pair(PAIR_CURRENT_MATCH,         COLOR_BLACK,   COLOR_RED);
    init_pair(PAIR_NORMAL_LINENUM,        COLOR_WHITE,   COLOR_BLUE);
    init_pair(PAIR_INSERT_LINENUM,        COLOR_BLACK,   COLOR_GREEN);
    init_pair(PAIR_VISUAL_LINENUM,        COLOR_WHITE,   COLOR_MAGENTA);
    init_pair(PAIR_SEARCH_BAR,            COLOR_WHITE,   COLOR_BLUE);
    init_pair(PAIR_SEARCH_BAR_NOMATCH,    COLOR_RED,     COLOR_BLUE);
    init_pair(PAIR_REPLACE_BAR,           COLOR_WHITE,   COLOR_GREEN);
    init_pair(PAIR_REPLACE_BAR_NOMATCH,   COLOR_RED,     COLOR_GREEN);
    init_pair(PAIR_AUTOCOMPLETE_NORMAL,   COLOR_BLACK,   COLOR_MAGENTA);
    init_pair(PAIR_AUTOCOMPLETE_SELECTED, COLOR_WHITE,   COLOR_BLACK);
    init_pair(PAIR_SEPARATOR,             8,             -1);
}

int theme_current_linenum_pair(Mode m) {
    switch (m) {
    case MODE_INSERT: return PAIR_INSERT_LINENUM;
    case MODE_VISUAL: return PAIR_VISUAL_LINENUM;
    default:          return PAIR_NORMAL_LINENUM;
    }
}

/* --- Dynamic syntax color pair allocation -------------------------------- */

#define SYNTAX_PAIR_BASE  PAIR_COUNT
#define MAX_SYNTAX_PAIRS  64

static struct { short fg; short pair; } syntax_pair_cache[MAX_SYNTAX_PAIRS];
static int syntax_pair_count = 0;

short theme_syntax_pair(short fg_color) {
    for (int i = 0; i < syntax_pair_count; i++) {
        if (syntax_pair_cache[i].fg == fg_color)
            return syntax_pair_cache[i].pair;
    }
    if (syntax_pair_count >= MAX_SYNTAX_PAIRS) return 0;

    short idx = (short)(SYNTAX_PAIR_BASE + syntax_pair_count);
    init_pair(idx, fg_color, -1);
    syntax_pair_cache[syntax_pair_count].fg = fg_color;
    syntax_pair_cache[syntax_pair_count].pair = idx;
    syntax_pair_count++;
    return idx;
}
