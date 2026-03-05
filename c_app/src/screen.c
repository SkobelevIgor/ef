#include "screen.h"
#include "theme.h"
#include "runes.h"
#include "syntax.h"

#include <locale.h>
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

/* --- Layout calculation -------------------------------------------------- */

int line_number_width(int total_lines) {
    int w = 1;
    while (total_lines >= 10) { total_lines /= 10; w++; }
    if (w < 3) w = 3;
    return w + 1; /* +1 for space after number */
}

PaneLayout *calculate_pane_layout_horizontal(int num_panes, int width,
                                             int height, int start_y,
                                             int *out_count) {
    if (num_panes == 0) { *out_count = 0; return NULL; }

    int visible = num_panes;
    int seps = visible - 1;
    int avail = height - seps;
    int base_h = avail / visible;
    int rem = avail % visible;

    while (base_h < MIN_PANE_HEIGHT_HORIZONTAL && visible > 1) {
        visible--;
        seps = visible - 1;
        avail = height - seps;
        base_h = avail / visible;
        rem = avail % visible;
    }

    PaneLayout *layouts = malloc(sizeof(PaneLayout) * visible);
    int cur_y = start_y;
    for (int i = 0; i < visible; i++) {
        int h = base_h + (i >= visible - rem ? 1 : 0);
        layouts[i] = (PaneLayout){i, 0, cur_y, width, h};
        cur_y += h + 1;
    }
    *out_count = visible;
    return layouts;
}

PaneLayout *calculate_pane_layout_vertical(int num_panes, int width,
                                           int height, int start_y,
                                           int *out_count) {
    if (num_panes == 0) { *out_count = 0; return NULL; }

    int visible = num_panes;
    int seps = visible - 1;
    int avail = width - seps;
    int base_w = avail / visible;
    int rem = avail % visible;

    while (base_w < MIN_PANE_WIDTH_VERTICAL && visible > 1) {
        visible--;
        seps = visible - 1;
        avail = width - seps;
        base_w = avail / visible;
        rem = avail % visible;
    }

    PaneLayout *layouts = malloc(sizeof(PaneLayout) * visible);
    int cur_x = 0;
    for (int i = 0; i < visible; i++) {
        int w = base_w + (i >= visible - rem ? 1 : 0);
        layouts[i] = (PaneLayout){i, cur_x, start_y, w, height};
        cur_x += w + 1;
    }
    *out_count = visible;
    return layouts;
}

/* --- Cursor position ----------------------------------------------------- */

void calc_cursor_screen_pos(wchar_t **lines, const int *line_lens,
                            int cursor_row, int cursor_col,
                            int scroll_offset, int pane_width,
                            int ln_width, int tab_stop,
                            int *sx, int *sy) {
    int text_width = pane_width - ln_width;
    if (text_width < 1) text_width = 1;
    if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;

    *sy = 0;
    for (int i = scroll_offset; i < cursor_row; i++) {
        int vw = visual_line_width(lines[i], line_lens[i], tab_stop);
        *sy += (vw == 0) ? 1 : (vw + text_width - 1) / text_width;
    }

    if (line_lens[cursor_row] == 0 || cursor_col == 0) {
        *sx = ln_width;
    } else {
        int vc = visual_column(lines[cursor_row], line_lens[cursor_row],
                               cursor_col, tab_stop);
        *sy += vc / text_width;
        *sx = ln_width + (vc % text_width);
    }
}

/* --- NcursesScreen implementation ---------------------------------------- */

typedef struct {
    int dummy; /* ncurses uses global state */
} NcursesScreen;

static void ncurses_render(void *self, Pane **panes, int npanes, int active,
                           Mode mode, InputState *input, SplitMode split) {
    (void)self;
    erase();
    int width, height;
    getmaxyx(stdscr, height, width);

    int content_start_y = 0;
    int layout_count = 0;
    PaneLayout *layouts;
    if (split == SPLIT_VERTICAL) {
        layouts = calculate_pane_layout_vertical(npanes, width, height,
                                                content_start_y, &layout_count);
    } else {
        layouts = calculate_pane_layout_horizontal(npanes, width, height,
                                                  content_start_y, &layout_count);
    }

    if (active >= layout_count) active = layout_count - 1;
    if (active < 0) active = 0;

    for (int i = 0; i < layout_count; i++) {
        PaneLayout *lay = &layouts[i];
        if (lay->pane_index >= npanes) continue;
        Pane *pane = panes[lay->pane_index];
        Buffer *buf = pane->buffer;

        int ln_w = line_number_width(buf->line_count);
        int text_w = lay->width - ln_w;
        if (text_w < 1) text_w = 1;

        Mode pane_mode = (i == active) ? mode : MODE_NORMAL;
        int cur_ln_pair = theme_current_linenum_pair(pane_mode);

        pane_adjust_scroll(pane, text_w, lay->height);

        int screen_row = 0;
        for (int line_idx = pane->scroll_offset;
             line_idx < buf->line_count && screen_row < lay->height;
             line_idx++) {
            bool is_current = (line_idx == pane->cursor_row);
            int line_num;
            int ln_pair;
            if (is_current) {
                line_num = line_idx + 1;
                ln_pair = cur_ln_pair;
            } else {
                line_num = line_idx - pane->cursor_row;
                if (line_num < 0) line_num = -line_num;
                ln_pair = PAIR_LINE_NUM;
            }

            /* Draw line number */
            char num_buf[16];
            snprintf(num_buf, sizeof(num_buf), "%*d ", ln_w - 1, line_num);
            attron(COLOR_PAIR(ln_pair) | (is_current ? A_BOLD : 0));
            mvaddstr(lay->start_y + screen_row, lay->start_x, num_buf);
            attroff(COLOR_PAIR(ln_pair) | (is_current ? A_BOLD : 0));

            if (buf->line_lens[line_idx] == 0) {
                screen_row++;
                continue;
            }

            /* Get syntax tokens for this line */
            const SyntaxToken *tokens = NULL;
            int token_count = 0;
            if (buf->highlight_cache) {
                tokens = highlight_cache_get_tokens(
                    buf->highlight_cache, line_idx,
                    buf->lines[line_idx], buf->line_lens[line_idx],
                    &token_count);
            }

            /* Draw text with wrapping */
            int char_idx = 0;
            bool first_wrap = true;
            while (char_idx < buf->line_lens[line_idx]
                   && screen_row < lay->height) {
                if (!first_wrap) {
                    /* Wrap indicator */
                    char wrap_buf[16];
                    snprintf(wrap_buf, sizeof(wrap_buf), "%*s ",
                             ln_w - 1, "\xe2\x86\xaa"); /* ↪ in UTF-8 */
                    int wp = is_current ? cur_ln_pair : PAIR_WRAP_INDIC;
                    attron(COLOR_PAIR(wp));
                    mvaddstr(lay->start_y + screen_row, lay->start_x, wrap_buf);
                    attroff(COLOR_PAIR(wp));
                }
                first_wrap = false;

                int tab_stop = buf->config.tab_stop;
                if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;

                int col = 0;
                int text_x = lay->start_x + ln_w;
                while (col < text_w
                       && char_idx < buf->line_lens[line_idx]) {
                    wchar_t ch = buf->lines[line_idx][char_idx];

                    attr_t char_attr = 0;
                    short char_pair = 0;
                    if (pane->selection_active
                        && pane_is_in_selection(pane, line_idx, char_idx)) {
                        char_attr = A_REVERSE;
                    } else if (tokens) {
                        const SyntaxToken *tok =
                            syntax_token_at(tokens, token_count, char_idx);
                        if (tok) {
                            if (tok->fg_color >= 0)
                                char_pair = theme_syntax_pair(tok->fg_color);
                            if (tok->bold)
                                char_attr |= A_BOLD;
                        }
                    }

                    if (ch == L'\t') {
                        int spaces = tab_stop - (col % tab_stop);
                        for (int s = 0; s < spaces && col < text_w; s++) {
                            mvaddch(lay->start_y + screen_row,
                                    text_x + col,
                                    ' ' | char_attr | COLOR_PAIR(char_pair));
                            col++;
                        }
                    } else {
                        cchar_t cc;
                        wchar_t wch[2] = {ch, L'\0'};
                        setcchar(&cc, wch, char_attr, char_pair, NULL);
                        mvadd_wch(lay->start_y + screen_row,
                                  text_x + col, &cc);
                        col++;
                    }
                    char_idx++;
                }
                screen_row++;
            }
        }

        /* Draw separator */
        if (i < layout_count - 1) {
            attron(COLOR_PAIR(PAIR_SEPARATOR));
            if (split == SPLIT_VERTICAL) {
                for (int y = lay->start_y; y < lay->start_y + lay->height; y++) {
                    mvaddstr(y, lay->start_x + lay->width, "\xe2\x94\x82");
                }
            } else {
                for (int x = 0; x < width; x++) {
                    mvaddstr(lay->start_y + lay->height, x, "\xe2\x94\x80");
                }
            }
            attroff(COLOR_PAIR(PAIR_SEPARATOR));
        }
    }

    /* Position cursor */
    if (active < layout_count && active < npanes) {
        PaneLayout *al = &layouts[active];
        Pane *ap = panes[active];
        int ln_w = line_number_width(ap->buffer->line_count);
        int cx, cy;
        calc_cursor_screen_pos(ap->buffer->lines, ap->buffer->line_lens,
                               ap->cursor_row, ap->cursor_col,
                               ap->scroll_offset, al->width, ln_w,
                               ap->buffer->config.tab_stop, &cx, &cy);
        move(al->start_y + cy, al->start_x + cx);
        curs_set(1);
    }

    refresh();
    free(layouts);
}

static int ncurses_poll_event(void *self, EditorEvent *ev) {
    (void)self;
    wget_wch(stdscr, (wint_t *)&ev->key);

    if (ev->key == KEY_RESIZE) {
        ev->type = EV_RESIZE;
        ev->is_char = false;
    } else if (ev->key >= KEY_MIN) {
        ev->type = EV_KEY;
        ev->ch = 0;
        ev->is_char = false;
    } else {
        ev->type = EV_KEY;
        ev->ch = (wchar_t)ev->key;
        ev->is_char = true;
    }
    return 0;
}

static void ncurses_get_size(void *self, int *w, int *h) {
    (void)self;
    getmaxyx(stdscr, *h, *w);
}

static void ncurses_sync(void *self) {
    (void)self;
    refresh();
}

static void ncurses_close(void *self) {
    (void)self;
    endwin();
}

static void ncurses_suspend(void *self) {
    (void)self;
    endwin();
    kill(getpid(), SIGTSTP);
}

static void ncurses_resume(void *self) {
    (void)self;
    refresh();
}

ScreenVTable *ncurses_screen_new(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    theme_init();

    NcursesScreen *ns = calloc(1, sizeof(NcursesScreen));
    ScreenVTable *vt = calloc(1, sizeof(ScreenVTable));
    vt->render = ncurses_render;
    vt->poll_event = ncurses_poll_event;
    vt->get_size = ncurses_get_size;
    vt->sync = ncurses_sync;
    vt->close = ncurses_close;
    vt->suspend = ncurses_suspend;
    vt->resume = ncurses_resume;
    vt->impl = ns;
    return vt;
}

void ncurses_screen_free(ScreenVTable *scr) {
    if (!scr) return;
    free(scr->impl);
    free(scr);
}
