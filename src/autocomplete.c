#include "autocomplete.h"
#include "xalloc.h"

#include <stdlib.h>
#include <string.h>
#include <wctype.h>

/* --- Delimiter check ----------------------------------------------------- */

static const wchar_t delimiters[] = L"()[]{}.,;:!?<>=+-*/%&|^~\"'` \t\n\r";

bool ac_is_delimiter(wchar_t ch) {
    if (iswspace(ch)) return true;
    for (const wchar_t *p = delimiters; *p; p++) {
        if (*p == ch) return true;
    }
    return false;
}

/* --- Word extraction ----------------------------------------------------- */

/* Simple hash set for deduplication */
typedef struct WordNode {
    wchar_t *word;
    int len;
    struct WordNode *next;
} WordNode;

#define HASH_BUCKETS 256

static unsigned int wcs_hash(const wchar_t *s, int len) {
    unsigned int h = 0;
    for (int i = 0; i < len; i++) h = h * 31 + (unsigned int)s[i];
    return h % HASH_BUCKETS;
}

wchar_t **ac_extract_words(wchar_t **lines, const int *line_lens, int line_count,
                           int exclude_row, int exclude_col,
                           int **out_lens, int *out_count) {
    WordNode *buckets[HASH_BUCKETS] = {0};
    int total = 0;

    for (int row = 0; row < line_count; row++) {
        int word_start = -1;
        for (int col = 0; col <= line_lens[row]; col++) {
            bool is_delim = (col == line_lens[row]) || ac_is_delimiter(lines[row][col]);
            if (is_delim) {
                if (word_start >= 0) {
                    int wlen = col - word_start;
                    if (wlen >= 2 && !(row == exclude_row && word_start == exclude_col)) {
                        unsigned int h = wcs_hash(lines[row] + word_start, wlen);
                        /* Check duplicate */
                        bool dup = false;
                        for (WordNode *n = buckets[h]; n; n = n->next) {
                            if (n->len == wlen && wmemcmp(n->word, lines[row] + word_start, wlen) == 0) {
                                dup = true;
                                break;
                            }
                        }
                        if (!dup) {
                            WordNode *n = xmalloc(sizeof(WordNode));
                            n->word = xmalloc(sizeof(wchar_t) * (wlen + 1));
                            wmemcpy(n->word, lines[row] + word_start, wlen);
                            n->word[wlen] = L'\0';
                            n->len = wlen;
                            n->next = buckets[h];
                            buckets[h] = n;
                            total++;
                        }
                    }
                    word_start = -1;
                }
            } else {
                if (word_start < 0) word_start = col;
            }
        }
    }

    wchar_t **words = xmalloc(sizeof(wchar_t *) * (total > 0 ? total : 1));
    int *lens = xmalloc(sizeof(int) * (total > 0 ? total : 1));
    int idx = 0;

    for (int b = 0; b < HASH_BUCKETS; b++) {
        WordNode *n = buckets[b];
        while (n) {
            words[idx] = n->word;
            lens[idx] = n->len;
            idx++;
            WordNode *next = n->next;
            free(n);
            n = next;
        }
    }

    *out_lens = lens;
    *out_count = total;
    return words;
}

void ac_free_words(wchar_t **words, int *lens, int count) {
    if (words) {
        for (int i = 0; i < count; i++) free(words[i]);
        free(words);
    }
    free(lens);
}

/* --- Subsequence matching ------------------------------------------------ */

bool ac_match_subsequence(const wchar_t *word, int word_len,
                          const wchar_t *query, int query_len) {
    int wi = 0;
    for (int qi = 0; qi < query_len; qi++) {
        wchar_t qc = towlower(query[qi]);
        bool found = false;
        while (wi < word_len) {
            if (towlower(word[wi++]) == qc) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

/* --- Scoring ------------------------------------------------------------- */

int ac_score_suggestion(const wchar_t *word, int word_len,
                        const wchar_t *query, int query_len) {
    int score = 0;

    /* Prefix match bonus */
    if (word_len >= query_len) {
        bool prefix = true;
        for (int i = 0; i < query_len; i++) {
            if (towlower(word[i]) != towlower(query[i])) {
                prefix = false;
                break;
            }
        }
        if (prefix) score += 1000;
    }

    /* Length penalty */
    score -= word_len;

    return score;
}

/* --- Sort comparison ----------------------------------------------------- */

static int suggestion_cmp(const void *a, const void *b) {
    const Suggestion *sa = a;
    const Suggestion *sb = b;
    if (sa->score != sb->score) return sb->score - sa->score; /* descending */
    /* Alphabetical tiebreak */
    int min_len = sa->word_len < sb->word_len ? sa->word_len : sb->word_len;
    for (int i = 0; i < min_len; i++) {
        wchar_t ca = towlower(sa->word[i]);
        wchar_t cb = towlower(sb->word[i]);
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    return sa->word_len - sb->word_len;
}

/* --- Find suggestions ---------------------------------------------------- */

Suggestion *ac_find_suggestions(wchar_t **words, const int *word_lens, int word_count,
                                const wchar_t *prefix, int prefix_len,
                                int max_results, int *out_count) {
    int cap = 32;
    Suggestion *suggs = xmalloc(sizeof(Suggestion) * cap);
    int count = 0;

    for (int i = 0; i < word_count; i++) {
        /* Skip exact matches — they offer nothing to complete */
        if (word_lens[i] == prefix_len &&
            wmemcmp(words[i], prefix, prefix_len) == 0)
            continue;
        if (ac_match_subsequence(words[i], word_lens[i], prefix, prefix_len)) {
            if (count >= cap) { cap *= 2; suggs = xrealloc(suggs, sizeof(Suggestion) * cap); }
            suggs[count].word = xmalloc(sizeof(wchar_t) * (word_lens[i] + 1));
            wmemcpy(suggs[count].word, words[i], word_lens[i]);
            suggs[count].word[word_lens[i]] = L'\0';
            suggs[count].word_len = word_lens[i];
            suggs[count].score = ac_score_suggestion(words[i], word_lens[i], prefix, prefix_len);
            count++;
        }
    }

    qsort(suggs, count, sizeof(Suggestion), suggestion_cmp);

    /* Trim to max_results */
    if (count > max_results) {
        for (int i = max_results; i < count; i++) free(suggs[i].word);
        count = max_results;
    }

    *out_count = count;
    if (count == 0) { free(suggs); return NULL; }
    return suggs;
}

void ac_free_suggestions(Suggestion *suggestions, int count) {
    if (!suggestions) return;
    for (int i = 0; i < count; i++) free(suggestions[i].word);
    free(suggestions);
}

/* --- AutocompleteState --------------------------------------------------- */

AutocompleteState *ac_state_new(void) {
    return xcalloc(1, sizeof(AutocompleteState));
}

void ac_state_free(AutocompleteState *ac) {
    if (!ac) return;
    free(ac->prefix);
    ac_free_suggestions(ac->suggestions, ac->suggestion_count);
    ac_free_words(ac->cached_words, ac->cached_word_lens, ac->cached_word_count);
    free(ac);
}

Suggestion *ac_selected(AutocompleteState *ac) {
    if (!ac || ac->suggestion_count == 0) return NULL;
    if (ac->selected_idx < 0 || ac->selected_idx >= ac->suggestion_count) return NULL;
    return &ac->suggestions[ac->selected_idx];
}

void ac_next(AutocompleteState *ac) {
    if (!ac || ac->suggestion_count == 0) return;
    ac->selected_idx = (ac->selected_idx + 1) % ac->suggestion_count;
}

void ac_prev(AutocompleteState *ac) {
    if (!ac || ac->suggestion_count == 0) return;
    ac->selected_idx--;
    if (ac->selected_idx < 0) ac->selected_idx = ac->suggestion_count - 1;
}

wchar_t **ac_get_words(AutocompleteState *ac, const void *source, wchar_t **lines,
                       const int *line_lens, int line_count,
                       uint64_t mod_count, int exclude_row, int exclude_col,
                       int **out_lens, int *out_count) {
    if (ac->cached_words && ac->cached_source == source
        && ac->cached_mod_count == mod_count) {
        *out_lens = ac->cached_word_lens;
        *out_count = ac->cached_word_count;
        return ac->cached_words;
    }
    /* Free old cache */
    ac_free_words(ac->cached_words, ac->cached_word_lens, ac->cached_word_count);

    ac->cached_words = ac_extract_words(lines, line_lens, line_count,
                                        exclude_row, exclude_col,
                                        &ac->cached_word_lens, &ac->cached_word_count);
    ac->cached_mod_count = mod_count;
    ac->cached_source = source;

    *out_lens = ac->cached_word_lens;
    *out_count = ac->cached_word_count;
    return ac->cached_words;
}
