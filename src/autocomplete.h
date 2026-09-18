#ifndef EF_AUTOCOMPLETE_H
#define EF_AUTOCOMPLETE_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

/* Suggestion represents a single autocomplete candidate */
typedef struct {
    wchar_t *word;
    int      word_len;
    int      score;
} Suggestion;

/* AutocompleteState represents an active autocomplete session */
typedef struct AutocompleteState {
    bool        active;
    wchar_t    *prefix;
    int         prefix_len;
    int         prefix_col;
    Suggestion *suggestions;
    int         suggestion_count;
    int         selected_idx;
    bool        show_above;

    /* Word cache */
    wchar_t **cached_words;
    int       *cached_word_lens;
    int        cached_word_count;
    uint64_t   cached_mod_count;
    const void *cached_source;
} AutocompleteState;

/* Check if a character is a word delimiter */
bool ac_is_delimiter(wchar_t ch);

/* Extract unique words from buffer lines (min length 2).
   Caller must free result with ac_free_words(). */
wchar_t **ac_extract_words(wchar_t **lines, const int *line_lens, int line_count,
                           int exclude_row, int exclude_col,
                           int **out_lens, int *out_count);
void ac_free_words(wchar_t **words, int *lens, int count);

/* Check if query chars appear in order within word (case-insensitive) */
bool ac_match_subsequence(const wchar_t *word, int word_len,
                          const wchar_t *query, int query_len);

/* Calculate relevance score (higher = better) */
int ac_score_suggestion(const wchar_t *word, int word_len,
                        const wchar_t *query, int query_len);

/* Find sorted, limited suggestions. Caller must free with ac_free_suggestions(). */
Suggestion *ac_find_suggestions(wchar_t **words, const int *word_lens, int word_count,
                                const wchar_t *prefix, int prefix_len,
                                int max_results, int *out_count);
void ac_free_suggestions(Suggestion *suggestions, int count);

/* AutocompleteState lifecycle */
AutocompleteState *ac_state_new(void);
void ac_state_free(AutocompleteState *ac);

/* Get currently selected suggestion (NULL if none) */
Suggestion *ac_selected(AutocompleteState *ac);

/* Navigate suggestions */
void ac_next(AutocompleteState *ac);
void ac_prev(AutocompleteState *ac);

/* Get cached words, rebuilding if buffer changed (source identifies the buffer) */
wchar_t **ac_get_words(AutocompleteState *ac, const void *source, wchar_t **lines,
                       const int *line_lens, int line_count,
                       uint64_t mod_count, int exclude_row, int exclude_col,
                       int **out_lens, int *out_count);

#endif /* EF_AUTOCOMPLETE_H */
