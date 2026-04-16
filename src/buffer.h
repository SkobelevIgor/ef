#ifndef EF_BUFFER_H
#define EF_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wchar.h>

#include "constants.h"

typedef struct HighlightCache HighlightCache;

/* FileTypeConfig holds per-filetype editing configuration. */
typedef struct {
    int  tab_stop;
    int  shift_width;
    bool auto_indentation;
    bool expand_tab;
} FileTypeConfig;

/* SearchMatch represents a single search match position. */
typedef struct {
    int row;
    int col;
    int length;
} SearchMatch;

/* Buffer represents the text content being edited.
   Buffer is a pure text container - cursor, scroll, and selection state live in Pane. */
typedef struct Buffer {
    wchar_t **lines;      /* Array of wide-char lines */
    int      *line_lens;  /* Length of each line */
    int      *line_caps;  /* Allocated capacity of each line */
    int       line_count; /* Number of lines */
    int       line_alloc; /* Allocated capacity for lines array */

    char    *filename;
    bool     modified;
    time_t   last_mod_time;

    char    *file_type;
    uint64_t mod_count;

    FileTypeConfig config;

    HighlightCache *highlight_cache;
} Buffer;

/* Create a new empty buffer (no file loading). */
Buffer *buffer_new(void);

/* Create a new buffer for the given filename. Loads file if it exists. */
Buffer *buffer_new_from_file(const char *filename);

/* Check whether filename can be opened for editing.
   Returns 0 if the file does not exist (will be created) or if it is writable.
   Returns -1 if the file exists but the user lacks write permission. */
int buffer_check_writable(const char *filename);

/* Free a buffer and all its lines. */
void buffer_free(Buffer *buf);

/* Load file content into the buffer. Returns 0 on success, -1 on error. */
int buffer_load(Buffer *buf);

/* Save buffer content to file. Returns 0 on success, -1 on error. */
int buffer_save(Buffer *buf);

/* InsertChar inserts a character at the given position. Returns new col. */
int buffer_insert_char(Buffer *buf, int row, int col, wchar_t ch);

/* DeleteChar deletes the character before the given position (backspace).
   Returns new row/col via pointers. */
void buffer_delete_char(Buffer *buf, int row, int col, int *new_row, int *new_col);

/* DeleteCharForward deletes the character at the given position (delete key). */
void buffer_delete_char_forward(Buffer *buf, int row, int col);

/* DeleteCharAt deletes the character at (row, col), returns the deleted wchar. */
wchar_t buffer_delete_char_at(Buffer *buf, int row, int col);

/* InsertNewline splits the line at the given position.
   Returns new row/col via pointers. */
void buffer_insert_newline(Buffer *buf, int row, int col, int *new_row, int *new_col);

/* InsertNewlineWithIndent splits the line and applies auto-indentation.
   Returns new row/col via pointers. */
void buffer_insert_newline_with_indent(Buffer *buf, int row, int col,
                                       int *new_row, int *new_col);

/* InsertTab inserts a tab or spaces. Returns new col. */
int buffer_insert_tab(Buffer *buf, int row, int col);

/* OpenLineBelow opens a new line below the given row with auto-indentation.
   Returns new row/col via pointers. */
void buffer_open_line_below(Buffer *buf, int row, int *new_row, int *new_col);

/* OpenLineAbove opens a new line above the given row with auto-indentation.
   Returns new row/col via pointers. */
void buffer_open_line_above(Buffer *buf, int row, int *new_row, int *new_col);

/* DeleteLine deletes the specified row. Returns deleted line (caller owns) and its length. */
wchar_t *buffer_delete_line(Buffer *buf, int row, int *deleted_len);

/* CopyLine returns a copy of the specified row (caller owns). */
wchar_t *buffer_copy_line(Buffer *buf, int row, int *copy_len);

/* InsertLineAfter inserts a line after the specified row. Takes ownership of line. */
void buffer_insert_line_after(Buffer *buf, int row, wchar_t *line, int line_len);

/* InsertLineBefore inserts a line before the specified row. Takes ownership of line. */
void buffer_insert_line_before(Buffer *buf, int row, wchar_t *line, int line_len);

/* DeleteRange deletes text from (startRow, startCol) to (endRow, endCol) exclusive.
   Returns the deleted text as lines. Caller must free result with buffer_free_lines(). */
wchar_t **buffer_delete_range(Buffer *buf,
                              int start_row, int start_col,
                              int end_row, int end_col,
                              int **deleted_lens, int *deleted_count);

/* GetRange returns text from (startRow, startCol) to (endRow, endCol) inclusive.
   Caller must free result with buffer_free_lines(). */
wchar_t **buffer_get_range(Buffer *buf,
                           int start_row, int start_col,
                           int end_row, int end_col,
                           int **result_lens, int *result_count);

/* IndentRange prepends indentation to each line in the range. */
void buffer_indent_range(Buffer *buf, int start_row, int end_row);

/* UnindentRange removes leading indentation from each line in the range. */
void buffer_unindent_range(Buffer *buf, int start_row, int end_row);

/* ReindentRange reindents lines by matching each line's indent to the line above. */
void buffer_reindent_range(Buffer *buf, int start_row, int end_row);

/* GetShiftWidth returns the shift width for indentation. */
int buffer_get_shift_width(const Buffer *buf);

/* GetVisualColumn calculates the visual column accounting for tabs. */
int buffer_get_visual_column(const Buffer *buf, const wchar_t *line, int line_len, int char_col);

/* GetVisualLineWidth calculates the visual width of a line accounting for tabs. */
int buffer_get_visual_line_width(const Buffer *buf, const wchar_t *line, int line_len);

/* Free an array of lines returned by buffer_delete_range / buffer_get_range. */
void buffer_free_lines(wchar_t **lines, int *lens, int count);

/* Deep-copy an array of lines. Caller must free with buffer_free_lines(). */
wchar_t **buffer_copy_lines(wchar_t **lines, const int *lens, int count, int **out_lens);

/* Replace all buffer lines with src. Takes ownership of src entries.
   Caller frees the outer src/src_lens arrays. */
void buffer_replace_all(Buffer *buf, wchar_t **src, int *src_lens, int count);

/* Internal: set a line at a given row (takes ownership). */
void buffer_set_line(Buffer *buf, int row, wchar_t *line, int len);

/* Internal: ensure capacity for at least n lines. */
void buffer_ensure_lines(Buffer *buf, int n);

/* Internal: splice lines (remove deleteCount at start, insert new lines). */
void buffer_splice_lines(Buffer *buf, int start, int delete_count,
                         wchar_t **insert, int *insert_lens, int insert_count);

#endif /* EF_BUFFER_H */
