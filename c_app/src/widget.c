#include "widget.h"
#include <stdlib.h>
#include <string.h>

/* --- WidgetSession ------------------------------------------------------- */

WidgetSession *widget_session_new(int row, int col) {
    WidgetSession *s = calloc(1, sizeof(WidgetSession));
    s->current_index = -1;
    s->cursor_row = row;
    s->cursor_col = col;
    s->query_cap = 64;
    s->query = calloc(s->query_cap, sizeof(wchar_t));
    s->replace_cap = 64;
    s->replace_text = calloc(s->replace_cap, sizeof(wchar_t));
    return s;
}

void widget_session_free(WidgetSession *s) {
    if (!s) return;
    free(s->query);
    free(s->replace_text);
    free(s->matches);
    free(s);
}

void widget_session_append_query(WidgetSession *s, wchar_t ch) {
    if (!s) return;
    if (s->query_len + 1 >= s->query_cap) {
        s->query_cap *= 2;
        s->query = realloc(s->query, sizeof(wchar_t) * s->query_cap);
    }
    s->query[s->query_len++] = ch;
    s->query[s->query_len] = L'\0';
}

void widget_session_backspace_query(WidgetSession *s) {
    if (!s || s->query_len == 0) return;
    s->query_len--;
    s->query[s->query_len] = L'\0';
}

void widget_session_append_replace(WidgetSession *s, wchar_t ch) {
    if (!s) return;
    if (s->replace_len + 1 >= s->replace_cap) {
        s->replace_cap *= 2;
        s->replace_text = realloc(s->replace_text, sizeof(wchar_t) * s->replace_cap);
    }
    s->replace_text[s->replace_len++] = ch;
    s->replace_text[s->replace_len] = L'\0';
}

void widget_session_backspace_replace(WidgetSession *s) {
    if (!s || s->replace_len == 0) return;
    s->replace_len--;
    s->replace_text[s->replace_len] = L'\0';
}

/* --- WidgetState --------------------------------------------------------- */

WidgetState *widget_state_new(WidgetKind kind, int anchor_row, int anchor_col) {
    WidgetState *w = calloc(1, sizeof(WidgetState));
    w->active = true;
    w->kind = kind;
    w->focus = FOCUS_FIND_BAR;
    w->anchor_row = anchor_row;
    w->anchor_col = anchor_col;

    switch (kind) {
    case WIDGET_SEARCH:
        w->search_session = widget_session_new(anchor_row, anchor_col);
        break;
    case WIDGET_FIND_REPLACE:
        w->find_replace_session = widget_session_new(anchor_row, anchor_col);
        break;
    default:
        break;
    }
    return w;
}

void widget_state_free(WidgetState *w) {
    if (!w) return;
    widget_session_free(w->search_session);
    widget_session_free(w->find_replace_session);
    free(w);
}

WidgetSession *widget_current_session(WidgetState *w) {
    if (!w) return NULL;
    switch (w->kind) {
    case WIDGET_SEARCH:      return w->search_session;
    case WIDGET_FIND_REPLACE: return w->find_replace_session;
    default:                  return NULL;
    }
}

/* --- Bar height calculation ---------------------------------------------- */

int calculate_bar_rows(const wchar_t *text, int text_len, int width) {
    if (width <= 0) return 1;
    if (text_len <= width) return 1;
    return (text_len + width - 1) / width;
}

int widget_bar_height(const WidgetState *w, int screen_width) {
    if (!w) return 0;
    switch (w->kind) {
    case WIDGET_SEARCH:
        if (!w->search_session) return 1;
        return calculate_bar_rows(w->search_session->query,
                                  w->search_session->query_len, screen_width);
    case WIDGET_FIND_REPLACE:
        if (!w->find_replace_session) return 2;
        {
            int find_rows = calculate_bar_rows(
                w->find_replace_session->query,
                w->find_replace_session->query_len, screen_width);
            int replace_rows = calculate_bar_rows(
                w->find_replace_session->replace_text,
                w->find_replace_session->replace_len, screen_width);
            return find_rows + replace_rows;
        }
    default:
        return 0;
    }
}
