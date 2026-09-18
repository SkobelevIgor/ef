#include "buffer.h"
#include "xalloc.h"
#include "syntax.h"
#include "runes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <locale.h>
#include <unistd.h>

/* --- Internal helpers ---------------------------------------------------- */

static void buffer_shift_arrays(Buffer *buf, int dst, int src, int count) {
    memmove(buf->lines + dst, buf->lines + src, sizeof(wchar_t *) * count);
    memmove(buf->line_lens + dst, buf->line_lens + src, sizeof(int) * count);
    memmove(buf->line_caps + dst, buf->line_caps + src, sizeof(int) * count);
}

static void get_leading_whitespace(const wchar_t *line, int len,
                                   wchar_t **out, int *out_len) {
    int i = 0;
    while (i < len && is_whitespace(line[i])) i++;
    *out_len = i;
    if (i == 0) { *out = NULL; return; }
    *out = xwcsdup(line, i);
}

/* --- Lifecycle ----------------------------------------------------------- */

Buffer *buffer_new(void) {
    Buffer *buf = xcalloc(1, sizeof(Buffer));
    buf->config.tab_stop = DEFAULT_TAB_STOP;
    buf->config.shift_width = DEFAULT_TAB_STOP;
    buf->trailing_newline = true;
    /* Start with one empty line */
    buffer_ensure_lines(buf, 1);
    buf->lines[0] = xwcsdup(L"", 0);
    buf->line_lens[0] = 0;
    buf->line_caps[0] = 1;
    buf->line_count = 1;
    return buf;
}

Buffer *buffer_new_from_file(const char *filename) {
    Buffer *buf = buffer_new();
    buf->filename = xstrdup(filename);
    struct stat st;
    if (stat(filename, &st) == 0) {
        if (buffer_load(buf) != 0) {
            buffer_free(buf);
            return NULL;
        }
    }
    return buf;
}

int buffer_check_writable(const char *filename) {
    if (!filename) return -1;
    if (access(filename, F_OK) != 0) return 0;  /* file doesn't exist – OK */
    return access(filename, W_OK) == 0 ? 0 : -1;
}

void buffer_free(Buffer *buf) {
    if (!buf) return;
    for (int i = 0; i < buf->line_count; i++) {
        free(buf->lines[i]);
    }
    free(buf->lines);
    free(buf->line_lens);
    free(buf->line_caps);
    free(buf->filename);
    free(buf->file_type);
    highlight_cache_free(buf->highlight_cache);
    free(buf);
}

/* --- Capacity ------------------------------------------------------------ */

void buffer_ensure_lines(Buffer *buf, int n) {
    if (n <= buf->line_alloc) return;
    int new_alloc = buf->line_alloc == 0 ? 16 : buf->line_alloc;
    while (new_alloc < n) new_alloc *= 2;
    buf->lines = xrealloc(buf->lines, sizeof(wchar_t *) * new_alloc);
    buf->line_lens = xrealloc(buf->line_lens, sizeof(int) * new_alloc);
    buf->line_caps = xrealloc(buf->line_caps, sizeof(int) * new_alloc);
    for (int i = buf->line_alloc; i < new_alloc; i++) {
        buf->lines[i] = NULL;
        buf->line_lens[i] = 0;
        buf->line_caps[i] = 0;
    }
    buf->line_alloc = new_alloc;
}

void buffer_set_line(Buffer *buf, int row, wchar_t *line, int len) {
    free(buf->lines[row]);
    buf->lines[row] = line;
    buf->line_lens[row] = len;
    buf->line_caps[row] = len + 1;
}

void buffer_replace_all(Buffer *buf, wchar_t **src, int *src_lens, int count) {
    for (int i = 0; i < buf->line_count; i++) free(buf->lines[i]);
    buf->line_count = 0;
    if (count > 0) {
        buffer_ensure_lines(buf, count);
        for (int i = 0; i < count; i++) {
            buf->lines[i] = src[i];
            buf->line_lens[i] = src_lens[i];
            buf->line_caps[i] = src_lens[i] + 1;
        }
    }
    buf->line_count = count;
}

/* --- Splice -------------------------------------------------------------- */

void buffer_splice_lines(Buffer *buf, int start, int delete_count,
                         wchar_t **insert, int *insert_lens, int insert_count) {
    /* Free deleted lines */
    for (int i = start; i < start + delete_count && i < buf->line_count; i++) {
        free(buf->lines[i]);
    }

    int new_count = buf->line_count - delete_count + insert_count;
    buffer_ensure_lines(buf, new_count);

    /* Shift lines after deleted region */
    int tail_start = start + delete_count;
    int tail_count = buf->line_count - tail_start;
    if (tail_count > 0) {
        buffer_shift_arrays(buf, start + insert_count, tail_start, tail_count);
    }

    /* Insert new lines */
    for (int i = 0; i < insert_count; i++) {
        buf->lines[start + i] = insert[i];
        buf->line_lens[start + i] = insert_lens[i];
        buf->line_caps[start + i] = insert_lens[i] + 1;
    }

    buf->line_count = new_count;
}

/* --- File I/O ------------------------------------------------------------ */

/* Convert one NUL-terminated line to wide chars. Returns NULL if the
   bytes cannot be decoded or contain an embedded NUL. */
static wchar_t *decode_line(const char *mb_buf, size_t mb_len, int *wlen_out) {
    size_t wlen = mbstowcs(NULL, mb_buf, 0);
    if (wlen == (size_t)-1 || strlen(mb_buf) != mb_len) return NULL;
    wchar_t *wline = xmalloc(sizeof(wchar_t) * (wlen + 1));
    if (wlen > 0) {
        mbstowcs(wline, mb_buf, wlen + 1);
    }
    wline[wlen] = L'\0';
    *wlen_out = (int)wlen;
    return wline;
}

static int read_file_lines(FILE *f, wchar_t ***lines_out, int **lens_out,
                           bool *trailing_newline) {
    wchar_t **lines = NULL;
    int *lens = NULL;
    int count = 0, alloc = 0;

    char *mb_buf = NULL;
    size_t mb_cap = 0;
    ssize_t nread;
    *trailing_newline = true;
    while ((nread = getline(&mb_buf, &mb_cap, f)) != -1) {
        /* Strip trailing newline */
        size_t mb_len = (size_t)nread;
        *trailing_newline = (mb_len > 0 && mb_buf[mb_len - 1] == '\n');
        if (*trailing_newline) {
            mb_buf[--mb_len] = '\0';
        }
        if (mb_len > 0 && mb_buf[mb_len - 1] == '\r') {
            mb_buf[--mb_len] = '\0';
        }

        int wlen;
        wchar_t *wline = decode_line(mb_buf, mb_len, &wlen);
        if (!wline) {
            free(mb_buf);
            buffer_free_lines(lines, lens, count);
            return -1;
        }

        if (count >= alloc) {
            alloc = alloc == 0 ? 16 : alloc * 2;
            lines = xrealloc(lines, sizeof(wchar_t *) * alloc);
            lens = xrealloc(lens, sizeof(int) * alloc);
        }
        lines[count] = wline;
        lens[count] = wlen;
        count++;
    }
    free(mb_buf);

    if (ferror(f)) {
        buffer_free_lines(lines, lens, count);
        return -1;
    }
    *lines_out = lines;
    *lens_out = lens;
    return count;
}

int buffer_load(Buffer *buf) {
    FILE *f = fopen(buf->filename, "r");
    if (!f) return -1;

    wchar_t **lines;
    int *lens;
    bool trailing_newline;
    int count = read_file_lines(f, &lines, &lens, &trailing_newline);
    fclose(f);
    if (count < 0) return -1;

    buffer_replace_all(buf, lines, lens, count);
    free(lines);
    free(lens);
    buf->trailing_newline = trailing_newline;

    if (buf->line_count == 0) {
        buffer_ensure_lines(buf, 1);
        buf->lines[0] = xwcsdup(L"", 0);
        buf->line_lens[0] = 0;
        buf->line_caps[0] = 1;
        buf->line_count = 1;
    }

    struct stat st;
    if (stat(buf->filename, &st) == 0) {
        buf->last_mod_time = st.st_mtime;
    }

    buf->modified = false;
    return 0;
}

/* Size of the largest encoded line plus NUL, or 0 if any line cannot
   be encoded in the current locale. */
static size_t encoded_line_cap(const Buffer *buf) {
    size_t cap = 1;
    for (int i = 0; i < buf->line_count; i++) {
        size_t n = wcstombs(NULL, buf->lines[i], 0);
        if (n == (size_t)-1) return 0;
        if (n + 1 > cap) cap = n + 1;
    }
    return cap;
}

int buffer_save(Buffer *buf) {
    if (!buf->filename) return -1;
    size_t mb_cap = encoded_line_cap(buf);
    if (mb_cap == 0) return -1;
    FILE *f = fopen(buf->filename, "w");
    if (!f) return -1;

    char *mb_buf = xmalloc(mb_cap);
    for (int i = 0; i < buf->line_count; i++) {
        wcstombs(mb_buf, buf->lines[i], mb_cap);
        if (fputs(mb_buf, f) == EOF) { free(mb_buf); fclose(f); return -1; }
        if (i < buf->line_count - 1 || buf->trailing_newline) {
            if (fputc('\n', f) == EOF) { free(mb_buf); fclose(f); return -1; }
        }
    }
    free(mb_buf);

    fclose(f);

    struct stat st;
    if (stat(buf->filename, &st) == 0) {
        buf->last_mod_time = st.st_mtime;
    }

    buf->modified = false;
    return 0;
}

/* --- Mutations ----------------------------------------------------------- */

int buffer_insert_char(Buffer *buf, int row, int col, wchar_t ch) {
    int new_len;
    wchar_t *new_line = insert_runes(buf->lines[row], buf->line_lens[row],
                                     col, &ch, 1, &new_len);
    buffer_set_line(buf, row, new_line, new_len);
    buf->modified = true;
    buf->mod_count++;
    return col + 1;
}

void buffer_delete_char(Buffer *buf, int row, int col,
                        int *new_row, int *new_col) {
    if (col > 0) {
        int new_len;
        wchar_t *new_line = remove_runes(buf->lines[row], buf->line_lens[row],
                                         col - 1, col, &new_len);
        buffer_set_line(buf, row, new_line, new_len);
        buf->modified = true;
        buf->mod_count++;
        *new_row = row;
        *new_col = col - 1;
    } else if (row > 0) {
        /* Join with previous line */
        int prev_len = buf->line_lens[row - 1];
        int curr_len = buf->line_lens[row];
        int merged_len = prev_len + curr_len;
        wchar_t *merged = xmalloc(sizeof(wchar_t) * (merged_len + 1));
        wmemcpy(merged, buf->lines[row - 1], prev_len);
        wmemcpy(merged + prev_len, buf->lines[row], curr_len);
        merged[merged_len] = L'\0';
        buffer_set_line(buf, row - 1, merged, merged_len);
        /* Remove current line */
        free(buf->lines[row]);
        buffer_shift_arrays(buf, row, row + 1, buf->line_count - row - 1);
        buf->line_count--;
        buf->modified = true;
        buf->mod_count++;
        *new_row = row - 1;
        *new_col = prev_len;
    } else {
        *new_row = row;
        *new_col = col;
    }
}

void buffer_delete_char_forward(Buffer *buf, int row, int col) {
    if (col < buf->line_lens[row]) {
        int new_len;
        wchar_t *new_line = remove_runes(buf->lines[row], buf->line_lens[row],
                                         col, col + 1, &new_len);
        buffer_set_line(buf, row, new_line, new_len);
        buf->modified = true;
        buf->mod_count++;
    } else if (row < buf->line_count - 1) {
        /* Join with next line */
        int curr_len = buf->line_lens[row];
        int next_len = buf->line_lens[row + 1];
        int merged_len = curr_len + next_len;
        wchar_t *merged = xmalloc(sizeof(wchar_t) * (merged_len + 1));
        wmemcpy(merged, buf->lines[row], curr_len);
        wmemcpy(merged + curr_len, buf->lines[row + 1], next_len);
        merged[merged_len] = L'\0';
        buffer_set_line(buf, row, merged, merged_len);
        /* Remove next line */
        free(buf->lines[row + 1]);
        buffer_shift_arrays(buf, row + 1, row + 2, buf->line_count - row - 2);
        buf->line_count--;
        buf->modified = true;
        buf->mod_count++;
    }
}

wchar_t buffer_delete_char_at(Buffer *buf, int row, int col) {
    if (col >= buf->line_lens[row]) return 0;
    wchar_t deleted = buf->lines[row][col];
    int new_len;
    wchar_t *new_line = remove_runes(buf->lines[row], buf->line_lens[row],
                                     col, col + 1, &new_len);
    buffer_set_line(buf, row, new_line, new_len);
    buf->modified = true;
    buf->mod_count++;
    return deleted;
}

void buffer_insert_newline(Buffer *buf, int row, int col,
                           int *new_row, int *new_col) {
    wchar_t *line = buf->lines[row];
    int line_len = buf->line_lens[row];

    wchar_t *left = xwcsdup(line, col);
    int right_len = line_len - col;
    wchar_t *right = xwcsdup(line + col, right_len);

    buffer_set_line(buf, row, left, col);

    /* Insert right part as new line after row */
    buffer_splice_lines(buf, row + 1, 0, &right, &right_len, 1);

    buf->modified = true;
    buf->mod_count++;
    *new_row = row + 1;
    *new_col = 0;
}

void buffer_insert_newline_with_indent(Buffer *buf, int row, int col,
                                       int *new_row, int *new_col) {
    wchar_t *indent = NULL;
    int indent_len = 0;
    if (buf->config.auto_indentation) {
        get_leading_whitespace(buf->lines[row], buf->line_lens[row],
                               &indent, &indent_len);
    }

    buffer_insert_newline(buf, row, col, new_row, new_col);

    if (indent_len > 0 && indent) {
        wchar_t *new_line_content = buf->lines[*new_row];
        int new_line_len = buf->line_lens[*new_row];
        /* When cursor was within the indentation area, the right part
           already contains leftover whitespace — skip it to avoid
           duplicating indentation. */
        int skip = 0;
        if (col < indent_len) {
            skip = indent_len - col;
            if (skip > new_line_len) skip = new_line_len;
        }
        int content_len = new_line_len - skip;
        int total = indent_len + content_len;
        wchar_t *indented = xmalloc(sizeof(wchar_t) * (total + 1));
        wmemcpy(indented, indent, indent_len);
        wmemcpy(indented + indent_len, new_line_content + skip, content_len);
        indented[total] = L'\0';
        buffer_set_line(buf, *new_row, indented, total);
        *new_col = indent_len;
        free(indent);
    }
}

int buffer_insert_tab(Buffer *buf, int row, int col) {
    if (buf->config.expand_tab) {
        int tab_stop = buf->config.tab_stop;
        if (tab_stop <= 0) tab_stop = DEFAULT_TAB_STOP;
        int vis = buffer_get_visual_column(buf, buf->lines[row],
                                           buf->line_lens[row], col);
        int spaces = tab_stop - (vis % tab_stop);
        for (int i = 0; i < spaces; i++) {
            col = buffer_insert_char(buf, row, col, L' ');
        }
    } else {
        col = buffer_insert_char(buf, row, col, L'\t');
    }
    return col;
}

void buffer_open_line_below(Buffer *buf, int row,
                            int *new_row, int *new_col) {
    wchar_t *indent = NULL;
    int indent_len = 0;
    if (buf->config.auto_indentation) {
        get_leading_whitespace(buf->lines[row], buf->line_lens[row],
                               &indent, &indent_len);
    }

    wchar_t *new_line;
    if (indent_len > 0 && indent) {
        new_line = xwcsdup(indent, indent_len);
        free(indent);
    } else {
        new_line = xwcsdup(L"", 0);
        indent_len = 0;
    }

    buffer_insert_line_after(buf, row, new_line, indent_len);
    *new_row = row + 1;
    *new_col = indent_len;
}

void buffer_open_line_above(Buffer *buf, int row,
                            int *new_row, int *new_col) {
    wchar_t *indent = NULL;
    int indent_len = 0;
    if (buf->config.auto_indentation) {
        get_leading_whitespace(buf->lines[row], buf->line_lens[row],
                               &indent, &indent_len);
    }

    wchar_t *new_line;
    if (indent_len > 0 && indent) {
        new_line = xwcsdup(indent, indent_len);
        free(indent);
    } else {
        new_line = xwcsdup(L"", 0);
        indent_len = 0;
    }

    buffer_insert_line_before(buf, row, new_line, indent_len);
    *new_row = row;
    *new_col = indent_len;
}

wchar_t *buffer_delete_line(Buffer *buf, int row, int *deleted_len) {
    if (row < 0 || row >= buf->line_count) {
        if (deleted_len) *deleted_len = 0;
        return NULL;
    }

    wchar_t *deleted = xwcsdup(buf->lines[row], buf->line_lens[row]);
    if (deleted_len) *deleted_len = buf->line_lens[row];

    if (buf->line_count == 1) {
        free(buf->lines[0]);
        buf->lines[0] = xwcsdup(L"", 0);
        buf->line_lens[0] = 0;
        buf->line_caps[0] = 1;
    } else {
        free(buf->lines[row]);
        buffer_shift_arrays(buf, row, row + 1, buf->line_count - row - 1);
        buf->line_count--;
    }

    buf->modified = true;
    buf->mod_count++;
    return deleted;
}

wchar_t *buffer_copy_line(Buffer *buf, int row, int *copy_len) {
    if (row < 0 || row >= buf->line_count) {
        if (copy_len) *copy_len = 0;
        return NULL;
    }
    if (copy_len) *copy_len = buf->line_lens[row];
    return xwcsdup(buf->lines[row], buf->line_lens[row]);
}

void buffer_insert_line_after(Buffer *buf, int row, wchar_t *line, int line_len) {
    if (row < 0) row = 0;
    if (row >= buf->line_count) row = buf->line_count - 1;
    buffer_splice_lines(buf, row + 1, 0, &line, &line_len, 1);
    buf->modified = true;
    buf->mod_count++;
}

void buffer_insert_line_before(Buffer *buf, int row, wchar_t *line, int line_len) {
    if (row < 0) row = 0;
    if (row > buf->line_count) row = buf->line_count;
    buffer_splice_lines(buf, row, 0, &line, &line_len, 1);
    buf->modified = true;
    buf->mod_count++;
}

/* --- Range operations ---------------------------------------------------- */

static void clamp_range(const Buffer *buf, int *sr, int *sc, int *er, int *ec) {
    if (*sr < 0) *sr = 0;
    if (*er < 0) *er = 0;
    if (*sr >= buf->line_count) *sr = buf->line_count - 1;
    if (*er >= buf->line_count) *er = buf->line_count - 1;
    if (*sc < 0) *sc = 0;
    if (*ec < 0) *ec = 0;
    if (*sc > buf->line_lens[*sr]) *sc = buf->line_lens[*sr];
    if (*ec > buf->line_lens[*er]) *ec = buf->line_lens[*er];
}

wchar_t **buffer_delete_range(Buffer *buf,
                              int sr, int sc, int er, int ec,
                              int **deleted_lens, int *deleted_count) {
    normalize_range(&sr, &sc, &er, &ec);
    clamp_range(buf, &sr, &sc, &er, &ec);

    /* Same line */
    if (sr == er) {
        int del_len = ec - sc;
        wchar_t *del = xwcsdup(buf->lines[sr] + sc, del_len);
        int new_len;
        wchar_t *new_line = remove_runes(buf->lines[sr], buf->line_lens[sr],
                                         sc, ec, &new_len);
        buffer_set_line(buf, sr, new_line, new_len);
        buf->modified = true;
        buf->mod_count++;

        *deleted_count = 1;
        wchar_t **result = xmalloc(sizeof(wchar_t *));
        result[0] = del;
        *deleted_lens = xmalloc(sizeof(int));
        (*deleted_lens)[0] = del_len;
        return result;
    }

    /* Multi-line */
    int count = er - sr + 1;
    wchar_t **deleted = xmalloc(sizeof(wchar_t *) * count);
    int *dlens = xmalloc(sizeof(int) * count);

    /* First line: from sc to end */
    int first_del = buf->line_lens[sr] - sc;
    deleted[0] = xwcsdup(buf->lines[sr] + sc, first_del);
    dlens[0] = first_del;

    /* Middle lines */
    for (int i = sr + 1; i < er; i++) {
        deleted[i - sr] = xwcsdup(buf->lines[i], buf->line_lens[i]);
        dlens[i - sr] = buf->line_lens[i];
    }

    /* Last line: from start to ec */
    deleted[count - 1] = xwcsdup(buf->lines[er], ec);
    dlens[count - 1] = ec;

    /* Build merged line */
    int merged_len = sc + buf->line_lens[er] - ec;
    wchar_t *merged = xmalloc(sizeof(wchar_t) * (merged_len + 1));
    wmemcpy(merged, buf->lines[sr], sc);
    wmemcpy(merged + sc, buf->lines[er] + ec, buf->line_lens[er] - ec);
    merged[merged_len] = L'\0';

    buffer_splice_lines(buf, sr, count, &merged, &merged_len, 1);
    buf->modified = true;
    buf->mod_count++;

    *deleted_count = count;
    *deleted_lens = dlens;
    return deleted;
}

wchar_t **buffer_get_range(Buffer *buf,
                           int sr, int sc, int er, int ec,
                           int **result_lens, int *result_count) {
    normalize_range(&sr, &sc, &er, &ec);
    clamp_range(buf, &sr, &sc, &er, &ec);
    /* get_range uses inclusive ec, clamp one further */
    int ll = buf->line_lens[er];
    if (ec >= ll) ec = ll - 1;

    /* Same line */
    if (sr == er) {
        if (sc > ec || ec < 0) {
            *result_count = 1;
            wchar_t **r = xmalloc(sizeof(wchar_t *));
            r[0] = xwcsdup(L"", 0);
            *result_lens = xmalloc(sizeof(int));
            (*result_lens)[0] = 0;
            return r;
        }
        int len = ec - sc + 1;
        *result_count = 1;
        wchar_t **r = xmalloc(sizeof(wchar_t *));
        r[0] = xwcsdup(buf->lines[sr] + sc, len);
        *result_lens = xmalloc(sizeof(int));
        (*result_lens)[0] = len;
        return r;
    }

    /* Multi-line */
    int count = er - sr + 1;
    wchar_t **result = xmalloc(sizeof(wchar_t *) * count);
    int *rlens = xmalloc(sizeof(int) * count);

    /* First line: from sc to end */
    int first_len = buf->line_lens[sr] - sc;
    result[0] = xwcsdup(buf->lines[sr] + sc, first_len);
    rlens[0] = first_len;

    /* Middle lines */
    for (int i = sr + 1; i < er; i++) {
        result[i - sr] = xwcsdup(buf->lines[i], buf->line_lens[i]);
        rlens[i - sr] = buf->line_lens[i];
    }

    /* Last line: from start to ec (inclusive) */
    if (ec >= 0 && ec < buf->line_lens[er]) {
        int last_len = ec + 1;
        result[count - 1] = xwcsdup(buf->lines[er], last_len);
        rlens[count - 1] = last_len;
    } else {
        result[count - 1] = xwcsdup(L"", 0);
        rlens[count - 1] = 0;
    }

    *result_count = count;
    *result_lens = rlens;
    return result;
}

/* --- Indent/Unindent ----------------------------------------------------- */

void buffer_indent_range(Buffer *buf, int start_row, int end_row) {
    if (start_row > end_row) { int t = start_row; start_row = end_row; end_row = t; }
    if (start_row < 0) start_row = 0;
    if (end_row >= buf->line_count) end_row = buf->line_count - 1;

    for (int row = start_row; row <= end_row; row++) {
        if (buf->config.expand_tab) {
            int sw = buffer_get_shift_width(buf);
            int new_len = sw + buf->line_lens[row];
            wchar_t *nl = xmalloc(sizeof(wchar_t) * (new_len + 1));
            for (int i = 0; i < sw; i++) nl[i] = L' ';
            wmemcpy(nl + sw, buf->lines[row], buf->line_lens[row]);
            nl[new_len] = L'\0';
            buffer_set_line(buf, row, nl, new_len);
        } else {
            wchar_t tab = L'\t';
            int new_len;
            wchar_t *nl = insert_runes(buf->lines[row], buf->line_lens[row],
                                       0, &tab, 1, &new_len);
            buffer_set_line(buf, row, nl, new_len);
        }
    }
    buf->modified = true;
    buf->mod_count++;
}

void buffer_unindent_range(Buffer *buf, int start_row, int end_row) {
    if (start_row > end_row) { int t = start_row; start_row = end_row; end_row = t; }
    if (start_row < 0) start_row = 0;
    if (end_row >= buf->line_count) end_row = buf->line_count - 1;

    int sw = buffer_get_shift_width(buf);

    for (int row = start_row; row <= end_row; row++) {
        if (buf->line_lens[row] == 0) continue;
        if (buf->lines[row][0] == L'\t') {
            int new_len;
            wchar_t *nl = remove_runes(buf->lines[row], buf->line_lens[row],
                                       0, 1, &new_len);
            buffer_set_line(buf, row, nl, new_len);
        } else {
            int spaces = 0;
            for (int i = 0; i < buf->line_lens[row] && i < sw
                     && buf->lines[row][i] == L' '; i++) {
                spaces++;
            }
            if (spaces > 0) {
                int new_len;
                wchar_t *nl = remove_runes(buf->lines[row], buf->line_lens[row],
                                           0, spaces, &new_len);
                buffer_set_line(buf, row, nl, new_len);
            }
        }
    }
    buf->modified = true;
    buf->mod_count++;
}

void buffer_reindent_range(Buffer *buf, int start_row, int end_row) {
    if (start_row > end_row) { int t = start_row; start_row = end_row; end_row = t; }
    if (start_row < 0) start_row = 0;
    if (end_row >= buf->line_count) end_row = buf->line_count - 1;

    for (int row = start_row; row <= end_row; row++) {
        wchar_t *prev_indent = NULL;
        int prev_indent_len = 0;
        if (row > 0 && buf->line_lens[row - 1] > 0) {
            get_leading_whitespace(buf->lines[row - 1], buf->line_lens[row - 1],
                                   &prev_indent, &prev_indent_len);
        }

        /* Strip leading whitespace from current line */
        int stripped_start = 0;
        while (stripped_start < buf->line_lens[row]
               && is_whitespace(buf->lines[row][stripped_start])) {
            stripped_start++;
        }
        int content_len = buf->line_lens[row] - stripped_start;
        int new_len = prev_indent_len + content_len;
        wchar_t *nl = xmalloc(sizeof(wchar_t) * (new_len + 1));
        if (prev_indent_len > 0 && prev_indent) {
            wmemcpy(nl, prev_indent, prev_indent_len);
        }
        wmemcpy(nl + prev_indent_len,
                buf->lines[row] + stripped_start, content_len);
        nl[new_len] = L'\0';
        buffer_set_line(buf, row, nl, new_len);
        free(prev_indent);
    }
    buf->modified = true;
    buf->mod_count++;
}

/* --- Query --------------------------------------------------------------- */

int buffer_get_shift_width(const Buffer *buf) {
    if (buf->config.shift_width > 0) return buf->config.shift_width;
    return DEFAULT_TAB_STOP;
}

int buffer_get_visual_column(const Buffer *buf, const wchar_t *line,
                             int line_len, int char_col) {
    return visual_column(line, line_len, char_col, buf->config.tab_stop);
}

int buffer_get_visual_line_width(const Buffer *buf, const wchar_t *line,
                                 int line_len) {
    return visual_line_width(line, line_len, buf->config.tab_stop);
}

/* --- Line utilities ------------------------------------------------------ */

void buffer_free_lines(wchar_t **lines, int *lens, int count) {
    if (!lines) return;
    for (int i = 0; i < count; i++) free(lines[i]);
    free(lines);
    free(lens);
}

wchar_t **buffer_copy_lines(wchar_t **lines, const int *lens, int count,
                            int **out_lens) {
    if (!lines || count == 0) {
        if (out_lens) *out_lens = NULL;
        return NULL;
    }
    wchar_t **result = xmalloc(sizeof(wchar_t *) * count);
    int *rlens = xmalloc(sizeof(int) * count);
    for (int i = 0; i < count; i++) {
        result[i] = xwcsdup(lines[i], lens[i]);
        rlens[i] = lens[i];
    }
    if (out_lens) *out_lens = rlens;
    return result;
}
