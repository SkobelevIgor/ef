#ifndef EF_WIDGET_H
#define EF_WIDGET_H

#include "buffer.h"
#include <stdbool.h>

/* WidgetKind identifies which widget is active */
typedef enum {
    WIDGET_NONE = 0,
    WIDGET_SEARCH,         /* F4 */
    WIDGET_FIND_REPLACE    /* F3 */
} WidgetKind;

/* FocusTarget identifies which part of the widget has focus */
typedef enum {
    FOCUS_FIND_BAR = 0,
    FOCUS_REPLACE_BAR,
    FOCUS_EDITOR
} FocusTarget;

/* WidgetSession holds state for a single search or find-replace session */
typedef struct {
    wchar_t *query;           /* Search query (wide chars) */
    int      query_len;
    int      query_cap;

    wchar_t *replace_text;    /* Replacement text */
    int      replace_len;
    int      replace_cap;

    SearchMatch *matches;
    int          match_count;
    int          current_index;
    bool         no_matches;

    int cursor_row;  /* Editor cursor when this session was last active */
    int cursor_col;
} WidgetSession;

/* WidgetState holds the full widget state shared across sessions */
typedef struct WidgetState {
    bool         active;
    WidgetKind   kind;
    FocusTarget  focus;
    int          anchor_row;
    int          anchor_col;
    bool         confirmed;  /* true when cursor should persist on close */

    WidgetSession *search_session;        /* F4 state */
    WidgetSession *find_replace_session;  /* F3 state */
} WidgetState;

/* Create/free session */
WidgetSession *widget_session_new(int row, int col);
void           widget_session_free(WidgetSession *s);

/* Create/free widget state */
WidgetState *widget_state_new(WidgetKind kind, int anchor_row, int anchor_col);
void         widget_state_free(WidgetState *w);

/* Get current session for active kind */
WidgetSession *widget_current_session(WidgetState *w);

/* Append/remove character from query */
void widget_session_append_query(WidgetSession *s, wchar_t ch);
void widget_session_backspace_query(WidgetSession *s);

/* Append/remove character from replace text */
void widget_session_append_replace(WidgetSession *s, wchar_t ch);
void widget_session_backspace_replace(WidgetSession *s);

/* Calculate bar rows for text wrapping */
int calculate_bar_rows(const wchar_t *text, int text_len, int width);

/* Total screen rows consumed by the widget */
int widget_bar_height(const WidgetState *w, int screen_width);

#endif /* EF_WIDGET_H */
