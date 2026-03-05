#ifndef EF_HISTORY_H
#define EF_HISTORY_H

#include <stdbool.h>
#include "buffer.h"

typedef enum {
    CHANGE_INSERT = 0,
    CHANGE_DELETE,
    CHANGE_REPLACE
} ChangeType;

typedef struct Change {
    ChangeType type;
    Buffer    *buffer;
    int        row;
    int        col;
    wchar_t  **text;
    int       *text_lens;
    int        text_count;
    wchar_t  **old_text;
    int       *old_text_lens;
    int        old_text_count;
    bool       line_deletion;
} Change;

typedef struct History {
    Change **undo_stack;
    int      undo_count;
    int      undo_cap;

    Change **redo_stack;
    int      redo_count;
    int      redo_cap;

    int max_size;

    Change  *pending;
    bool     in_session;

    Buffer  *session_buffer;
    int      session_start_row;
    int      session_start_col;
    wchar_t **session_lines;
    int      *session_line_lens;
    int       session_line_count;
} History;

History *history_new(int max_size);
void     history_free(History *h);

void     history_push(History *h, Change *c);
Change  *history_undo(History *h);
Change  *history_redo(History *h);
bool     history_can_undo(const History *h);
bool     history_can_redo(const History *h);

void history_start_session(History *h, Buffer *buf, int row, int col);
void history_commit_session(History *h, wchar_t **current_lines,
                            const int *current_lens, int current_count);

void history_record_insert(History *h, Buffer *buf, int row, int col,
                           wchar_t **text, const int *text_lens, int text_count);
void history_record_delete(History *h, Buffer *buf, int row, int col,
                           wchar_t **text, const int *text_lens, int text_count);
void history_record_delete_lines(History *h, Buffer *buf, int row,
                                 wchar_t **lines, const int *lens, int count);

Change *change_new(ChangeType type, Buffer *buf, int row, int col);
void    change_free(Change *c);

#endif /* EF_HISTORY_H */
