#include "screen.h"
#include "xalloc.h"
#include "theme.h"
#include "runes.h"
#include "syntax.h"
#include "widget.h"
#include "autocomplete.h"

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

    PaneLayout *layouts = xmalloc(sizeof(PaneLayout) * visible);
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

    PaneLayout *layouts = xmalloc(sizeof(PaneLayout) * visible);
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
                            int scroll_offset, int scroll_wrap,
                            int pane_width, int ln_width, int tab_stop,
                            int *sx, int *sy) {
    int text_width = pane_width - ln_width;
    if (text_width < 1) text_width = 1;
    if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;

    *sy = -scroll_wrap;
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

/* --- Cell helpers -------------------------------------------------------- */

static void set_cell(cchar_t *cc, wchar_t ch, attr_t attr, short pair) {
    wchar_t wch[2] = {ch, L'\0'};
    *cc = (cchar_t){0};
    if (setcchar(cc, wch, attr, pair, NULL) == ERR) {
        wch[0] = L'?';
        setcchar(cc, wch, attr, pair, NULL);
    }
}

/* --- Widget bar rendering ------------------------------------------------ */

static int render_widget_bar(const wchar_t *text, int text_len,
                             int start_x, int start_y, int width,
                             int max_rows, int color_pair) {
    int rows = calculate_bar_rows(text, text_len, width);
    if (rows > max_rows) rows = max_rows;
    attron(COLOR_PAIR(color_pair));
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < width; c++)
            mvaddch(start_y + r, start_x + c, ' ');

    int x = 0, y = 0;
    for (int i = 0; i < text_len; i++) {
        if (x >= width) { x = 0; y++; }
        if (y < rows) {
            cchar_t cc;
            set_cell(&cc, text[i], A_NORMAL, color_pair);
            mvadd_wch(start_y + y, start_x + x, &cc);
            x++;
        }
    }
    attroff(COLOR_PAIR(color_pair));
    return rows;
}

static int render_widget_in_pane(const WidgetState *w,
                                 int start_x, int start_y, int width,
                                 int max_rows) {
    if (!w || !w->active) return 0;
    WidgetSession *s = widget_current_session((WidgetState *)w);
    int total = 0;

    if (w->kind == WIDGET_SEARCH && s) {
        int pair = s->no_matches ? PAIR_SEARCH_BAR_NOMATCH : PAIR_SEARCH_BAR;
        total += render_widget_bar(s->query, s->query_len,
                                   start_x, start_y, width, max_rows, pair);
    } else if (w->kind == WIDGET_FIND_REPLACE && s) {
        int fp = s->no_matches ? PAIR_SEARCH_BAR_NOMATCH : PAIR_SEARCH_BAR;
        total += render_widget_bar(s->query, s->query_len,
                                   start_x, start_y, width, max_rows, fp);
        int rp = s->no_matches ? PAIR_REPLACE_BAR_NOMATCH : PAIR_REPLACE_BAR;
        total += render_widget_bar(s->replace_text, s->replace_len,
                                   start_x, start_y + total, width,
                                   max_rows - total, rp);
    }
    return total;
}

static bool is_in_match(const SearchMatch *matches, int count,
                        int current_index, int row, int col, int qlen,
                        bool *is_current) {
    for (int i = 0; i < count; i++) {
        if (matches[i].row == row
            && col >= matches[i].col && col < matches[i].col + qlen) {
            *is_current = (i == current_index);
            return true;
        }
    }
    return false;
}

/* --- Autocomplete dropdown ----------------------------------------------- */

static void render_autocomplete(AutocompleteState *ac,
                                 int cursor_x, int cursor_y,
                                 int min_y, int max_y) {
    if (!ac || !ac->active || ac->suggestion_count == 0) return;

    int max_w = 0;
    for (int i = 0; i < ac->suggestion_count; i++)
        if (ac->suggestions[i].word_len > max_w)
            max_w = ac->suggestions[i].word_len;

    int pad = 2;
    int drop_w = max_w + pad * 2;
    int drop_h = ac->suggestion_count;
    int space_below = max_y - cursor_y - 1;
    int space_above = cursor_y - min_y;
    int start_y;

    if (space_below >= drop_h) {
        start_y = cursor_y + 1;
    } else if (space_above >= drop_h) {
        start_y = cursor_y - drop_h;
    } else if (space_below > space_above) {
        start_y = cursor_y + 1;
        drop_h = space_below;
    } else {
        drop_h = space_above;
        start_y = cursor_y - drop_h;
    }
    if (drop_h <= 0) return;

    int display = drop_h < ac->suggestion_count ? drop_h : ac->suggestion_count;
    int offset = ac->selected_idx - display + 1;
    if (offset < 0) offset = 0;

    for (int i = 0; i < display; i++) {
        int y = start_y + i;
        bool selected = (i + offset == ac->selected_idx);
        int pair = selected ? PAIR_AUTOCOMPLETE_SELECTED
                            : PAIR_AUTOCOMPLETE_NORMAL;
        attron(COLOR_PAIR(pair));

        /* Left padding */
        for (int p = 0; p < pad; p++)
            mvaddch(y, cursor_x + p, ' ');

        /* Word */
        const Suggestion *sug = &ac->suggestions[i + offset];
        int x = cursor_x + pad;
        for (int c = 0; c < sug->word_len; c++) {
            cchar_t cc;
            set_cell(&cc, sug->word[c], A_NORMAL, pair);
            mvadd_wch(y, x + c, &cc);
        }

        /* Right padding */
        int filled = pad + sug->word_len;
        for (int p = filled; p < drop_w; p++)
            mvaddch(y, cursor_x + p, ' ');

        attroff(COLOR_PAIR(pair));
    }
}

/* --- NcursesScreen implementation ---------------------------------------- */

/* Custom key codes for bracketed paste mode (above KEY_MAX to avoid collision) */
#define KEY_PASTE_START (KEY_MAX + 1)
#define KEY_PASTE_END   (KEY_MAX + 2)

typedef struct {
    bool pasting; /* true between paste start/end */
} NcursesScreen;

static void ncurses_render(void *self, Pane **panes, int npanes, int active,
                           Mode mode, InputState *input, SplitMode split,
                           bool read_only) {
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

    int active_bar_h = 0;

    for (int i = 0; i < layout_count; i++) {
        PaneLayout *lay = &layouts[i];
        if (lay->pane_index >= npanes) continue;
        Pane *pane = panes[lay->pane_index];
        Buffer *buf = pane->buffer;

        int pane_y = lay->start_y;
        int pane_h = lay->height;

        /* Render widget bar at top of pane */
        int bar_h = 0;
        if (pane_has_active_widget(pane)) {
            bar_h = render_widget_in_pane(pane->widget,
                                          lay->start_x, pane_y, lay->width,
                                          lay->height - 1);
            pane_y += bar_h;
            pane_h -= bar_h;
            if (pane_h < 1) pane_h = 1;
        }
        if (i == active) active_bar_h = bar_h;

        /* Get widget match info for highlighting */
        const SearchMatch *matches = NULL;
        int match_count = 0, current_idx = -1, qlen = 0;
        if (pane_has_active_widget(pane)) {
            WidgetSession *ws = widget_current_session(pane->widget);
            if (ws && ws->match_count > 0) {
                matches = ws->matches;
                match_count = ws->match_count;
                current_idx = ws->current_index;
                qlen = ws->query_len;
            }
        }

        int ln_w = line_number_width(buf->line_count);
        /* Widen gutter if goto-line buffer needs more space */
        if (i == active && input && input->pending_goto_line) {
            int goto_w = input->goto_line_buf_len + 2; /* ':' + digits + space */
            if (goto_w > ln_w) ln_w = goto_w;
        }
        int text_w = lay->width - ln_w;
        if (text_w < 1) text_w = 1;

        int cur_ln_pair = (read_only && i == active)
            ? PAIR_READONLY_LINENUM
            : theme_gutter_current_line_pair(i == active, mode);

        pane_adjust_scroll(pane, text_w, pane_h);

        int screen_row = 0;
        for (int line_idx = pane->scroll_offset;
             line_idx < buf->line_count && screen_row < pane_h;
             line_idx++) {
            bool is_current = (line_idx == pane->cursor_row);
            int skip_rows = (line_idx == pane->scroll_offset)
                ? pane->scroll_wrap : 0;
            bool goto_prompt = is_current && i == active && input
                && input->pending_goto_line;
            bool show_num = skip_rows == 0 || goto_prompt;
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

            /* Draw line number (or goto-line indicator) */
            char num_buf[128];
            if (goto_prompt) {
                snprintf(num_buf, sizeof(num_buf), ":%-*s",
                         ln_w - 1, input->goto_line_buffer);
            } else {
                snprintf(num_buf, sizeof(num_buf), "%*d ", ln_w - 1, line_num);
            }
            if (show_num) {
                attron(COLOR_PAIR(ln_pair) | (is_current ? A_BOLD : 0));
                mvaddstr(pane_y + screen_row, lay->start_x, num_buf);
                attroff(COLOR_PAIR(ln_pair) | (is_current ? A_BOLD : 0));
            }

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
            int vis = 0;
            bool first_wrap = show_num;
            int tab_stop = buf->config.tab_stop;
            if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;
            while (char_idx < buf->line_lens[line_idx]
                   && vis / text_w < skip_rows) {
                wchar_t ch = buf->lines[line_idx][char_idx];
                vis += (ch == L'\t') ? tab_stop - (vis % tab_stop)
                                     : rune_width(ch);
                char_idx++;
            }
            while (char_idx < buf->line_lens[line_idx]
                   && screen_row < pane_h) {
                if (!first_wrap) {
                    /* Wrap indicator */
                    char wrap_buf[80];
                    snprintf(wrap_buf, sizeof(wrap_buf), "%*s\xe2\x86\xaa ",
                             ln_w - 2, ""); /* ↪ in UTF-8 */
                    int wp = is_current ? cur_ln_pair : PAIR_WRAP_INDIC;
                    attron(COLOR_PAIR(wp));
                    mvaddstr(pane_y + screen_row, lay->start_x, wrap_buf);
                    attroff(COLOR_PAIR(wp));
                }
                first_wrap = false;

                int wrap_row = vis / text_w;
                int text_x = lay->start_x + ln_w;
                while (vis / text_w == wrap_row
                       && char_idx < buf->line_lens[line_idx]) {
                    wchar_t ch = buf->lines[line_idx][char_idx];

                    attr_t char_attr = 0;
                    short char_pair = 0;
                    if (pane->selection_active
                        && pane_is_in_selection(pane, line_idx, char_idx)) {
                        char_attr = A_REVERSE;
                    } else {
                        /* Check widget match highlighting */
                        bool is_cur_match = false;
                        if (matches && is_in_match(matches, match_count,
                                current_idx, line_idx, char_idx,
                                qlen, &is_cur_match)) {
                            char_pair = is_cur_match
                                ? PAIR_CURRENT_MATCH : PAIR_SEARCH_MATCH;
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
                    }

                    if (ch == L'\t') {
                        int spaces = tab_stop - (vis % tab_stop);
                        int s;
                        for (s = 0; s < spaces && vis / text_w == wrap_row; s++) {
                            mvaddch(pane_y + screen_row,
                                    text_x + vis % text_w,
                                    ' ' | char_attr | COLOR_PAIR(char_pair));
                            vis++;
                        }
                        if (s < spaces) break;
                    } else {
                        cchar_t cc;
                        set_cell(&cc, ch, char_attr, char_pair);
                        mvadd_wch(pane_y + screen_row,
                                  text_x + vis % text_w, &cc);
                        vis += rune_width(ch);
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

        if (pane_has_active_widget(ap)
            && ap->widget->focus != FOCUS_EDITOR) {
            /* Cursor in widget bar */
            WidgetSession *ws = widget_current_session(ap->widget);
            if (ws) {
                int row_off = 0;
                int tlen = 0;
                if (ap->widget->focus == FOCUS_REPLACE_BAR) {
                    row_off = calculate_bar_rows(ws->query, ws->query_len,
                                                 al->width);
                    tlen = ws->replace_len;
                } else {
                    tlen = ws->query_len;
                }
                int cx = tlen % al->width;
                int cy = tlen / al->width + row_off;
                if (cy >= active_bar_h) cy = active_bar_h - 1;
                if (cy < 0) cy = 0;
                move(al->start_y + cy, al->start_x + cx);
            }
            curs_set(1);
        } else {
            int ln_w = line_number_width(ap->buffer->line_count);
            if (input && input->pending_goto_line) {
                int goto_w = input->goto_line_buf_len + 2;
                if (goto_w > ln_w) ln_w = goto_w;
            }
            int cx, cy;
            calc_cursor_screen_pos(ap->buffer->lines, ap->buffer->line_lens,
                                   ap->cursor_row, ap->cursor_col,
                                   ap->scroll_offset, ap->scroll_wrap,
                                   al->width, ln_w,
                                   ap->buffer->config.tab_stop, &cx, &cy);
            move(al->start_y + active_bar_h + cy, al->start_x + cx);
            curs_set(1);

            /* Render autocomplete dropdown */
            if (input && input->autocomplete && input->autocomplete->active) {
                int pane_max_y = al->start_y + al->height;
                render_autocomplete(input->autocomplete,
                                    al->start_x + cx,
                                    al->start_y + active_bar_h + cy,
                                    al->start_y + active_bar_h, pane_max_y);
                /* Restore cursor after overlay */
                move(al->start_y + active_bar_h + cy, al->start_x + cx);
            }
        }
    }

    refresh();
    free(layouts);
}

static int ncurses_poll_event(void *self, EditorEvent *ev) {
    NcursesScreen *ns = (NcursesScreen *)self;
    timeout(1000);
    wint_t wch = 0;
    int rc = wget_wch(stdscr, &wch);
    ev->is_paste = false;

    if (rc == ERR) {
        if (ns->pasting) ns->pasting = false; /* Reset stuck paste state */
        ev->key = 0;
        ev->type = EV_NONE;
        ev->is_char = false;
        return 0;
    }
    ev->key = (int)wch;

    if (rc == KEY_CODE_YES && ev->key == KEY_RESIZE) {
        ev->type = EV_RESIZE;
        ev->is_char = false;
        return 0;
    }

    /* Handle bracketed paste start/end as invisible events */
    if (rc == KEY_CODE_YES && ev->key == KEY_PASTE_START) {
        ns->pasting = true;
        ev->type = EV_NONE;
        ev->is_char = false;
        return 0;
    }
    if (rc == KEY_CODE_YES && ev->key == KEY_PASTE_END) {
        ns->pasting = false;
        ev->type = EV_NONE;
        ev->is_char = false;
        return 0;
    }

    if (rc == KEY_CODE_YES) {
        /* Special key (function key, arrow, etc.) */
        ev->type = EV_KEY;
        ev->ch = 0;
        ev->is_char = false;
    } else {
        /* Regular wide character (OK) — includes Cyrillic, CJK, etc. */
        ev->type = EV_KEY;
        ev->ch = (wchar_t)wch;
        ev->is_char = true;
    }

    ev->is_paste = ns->pasting;
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
    /* Disable bracketed paste mode before closing */
    printf("\033[?2004l");
    fflush(stdout);
    endwin();
}

static void ncurses_suspend(void *self) {
    NcursesScreen *ns = (NcursesScreen *)self;
    printf("\033[?2004l");
    fflush(stdout);
    ns->pasting = false;
    endwin();
    kill(getpid(), SIGTSTP);
}

static void ncurses_resume(void *self) {
    (void)self;
    refresh();
    printf("\033[?2004h");
    fflush(stdout);
}

ScreenVTable *ncurses_screen_new(void) {
    setlocale(LC_ALL, "");
    set_escdelay(25);
    initscr();
    cbreak();
    noecho();
    nonl();
    keypad(stdscr, TRUE);
    theme_init();

    /* Enable bracketed paste mode and register custom key codes */
    printf("\033[?2004h");
    fflush(stdout);
    define_key("\033[200~", KEY_PASTE_START);
    define_key("\033[201~", KEY_PASTE_END);

    NcursesScreen *ns = xcalloc(1, sizeof(NcursesScreen));
    ScreenVTable *vt = xcalloc(1, sizeof(ScreenVTable));
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
