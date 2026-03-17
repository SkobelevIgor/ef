#include "editor.h"
#include "xalloc.h"
#include "autocomplete.h"

#include <stdlib.h>
#include <string.h>

static void dismiss_autocomplete(Editor *ed) {
    if (ed->input_state->autocomplete) {
        ac_state_free(ed->input_state->autocomplete);
        ed->input_state->autocomplete = NULL;
    }
}

void editor_trigger_autocomplete(Editor *ed) {
    Pane *pane = editor_active_pane(ed);
    Buffer *buf = pane->buffer;
    if (pane->cursor_col == 0) {
        dismiss_autocomplete(ed);
        return;
    }

    wchar_t *line = buf->lines[pane->cursor_row];
    int cursor_col = pane->cursor_col;
    if (cursor_col > buf->line_lens[pane->cursor_row])
        cursor_col = buf->line_lens[pane->cursor_row];

    /* Scan backwards for word start */
    int start_col = cursor_col;
    while (start_col > 0 && !ac_is_delimiter(line[start_col - 1])) start_col--;

    int prefix_len = cursor_col - start_col;
    if (prefix_len < 2) {
        dismiss_autocomplete(ed);
        return;
    }

    AutocompleteState *ac = ed->input_state->autocomplete;
    if (!ac) ac = ac_state_new();

    int word_count, *word_lens;
    wchar_t **words = ac_get_words(ac, buf->lines, buf->line_lens, buf->line_count,
                                   buf->mod_count, pane->cursor_row, start_col,
                                   &word_lens, &word_count);

    int sugg_count;
    Suggestion *suggs = ac_find_suggestions(words, word_lens, word_count,
                                            line + start_col, prefix_len,
                                            10, &sugg_count);

    if (sugg_count == 0) {
        ac_state_free(ac);
        ed->input_state->autocomplete = NULL;
        return;
    }

    /* Free old suggestions */
    ac_free_suggestions(ac->suggestions, ac->suggestion_count);
    free(ac->prefix);

    ac->active = true;
    ac->prefix = xmalloc(sizeof(wchar_t) * (prefix_len + 1));
    wmemcpy(ac->prefix, line + start_col, prefix_len);
    ac->prefix[prefix_len] = L'\0';
    ac->prefix_len = prefix_len;
    ac->prefix_col = start_col;
    ac->suggestions = suggs;
    ac->suggestion_count = sugg_count;
    ac->selected_idx = 0;
    ed->input_state->autocomplete = ac;
}

void editor_accept_autocomplete(Editor *ed) {
    AutocompleteState *ac = ed->input_state->autocomplete;
    if (!ac || ac->suggestion_count == 0) return;

    Suggestion *sel = ac_selected(ac);
    if (!sel) return;

    Buffer *buf = editor_active_buffer(ed);
    Pane *pane = editor_active_pane(ed);
    wchar_t *line = buf->lines[pane->cursor_row];
    int line_len = buf->line_lens[pane->cursor_row];

    int prefix_start = pane->cursor_col - ac->prefix_len;
    if (prefix_start < 0) prefix_start = 0;

    int new_line_len = prefix_start + sel->word_len + (line_len - pane->cursor_col);
    wchar_t *new_line = xmalloc(sizeof(wchar_t) * (new_line_len + 1));
    wmemcpy(new_line, line, prefix_start);
    wmemcpy(new_line + prefix_start, sel->word, sel->word_len);
    wmemcpy(new_line + prefix_start + sel->word_len,
            line + pane->cursor_col, line_len - pane->cursor_col);
    new_line[new_line_len] = L'\0';

    buffer_set_line(buf, pane->cursor_row, new_line, new_line_len);
    pane->cursor_col = prefix_start + sel->word_len;
    buf->modified = true;

    ac_state_free(ac);
    ed->input_state->autocomplete = NULL;
    editor_schedule_auto_save(ed);
}
