#include "unity.h"
#include "autocomplete.h"
#include <stdlib.h>
#include <locale.h>

void setUp(void) {}
void tearDown(void) {}

/* --- isDelimiter --------------------------------------------------------- */

void test_is_delimiter_paren(void) { TEST_ASSERT_TRUE(ac_is_delimiter(L'(')); }
void test_is_delimiter_space(void) { TEST_ASSERT_TRUE(ac_is_delimiter(L' ')); }
void test_is_delimiter_tab(void) { TEST_ASSERT_TRUE(ac_is_delimiter(L'\t')); }
void test_is_delimiter_dot(void) { TEST_ASSERT_TRUE(ac_is_delimiter(L'.')); }
void test_is_delimiter_alpha(void) { TEST_ASSERT_FALSE(ac_is_delimiter(L'a')); }
void test_is_delimiter_digit(void) { TEST_ASSERT_FALSE(ac_is_delimiter(L'0')); }
void test_is_delimiter_underscore(void) { TEST_ASSERT_FALSE(ac_is_delimiter(L'_')); }

/* --- ExtractWords -------------------------------------------------------- */

void test_extract_words_simple(void) {
    wchar_t *lines[] = {(wchar_t *)L"hello world"};
    int lens[] = {11};
    int count;
    int *wlens;
    wchar_t **words = ac_extract_words(lines, lens, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    ac_free_words(words, wlens, count);
}

void test_extract_words_excludes_position(void) {
    wchar_t *lines[] = {(wchar_t *)L"hello world"};
    int lens[] = {11};
    int count;
    int *wlens;
    wchar_t **words = ac_extract_words(lines, lens, 1, 0, 6, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(1, count); /* "world" excluded at col 6 */
    ac_free_words(words, wlens, count);
}

void test_extract_words_skips_short(void) {
    wchar_t *lines[] = {(wchar_t *)L"a bb ccc"};
    int lens[] = {8};
    int count;
    int *wlens;
    wchar_t **words = ac_extract_words(lines, lens, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(2, count); /* "bb" and "ccc" */
    ac_free_words(words, wlens, count);
}

void test_extract_words_deduplicates(void) {
    wchar_t *lines[] = {(wchar_t *)L"foo foo bar"};
    int lens[] = {11};
    int count;
    int *wlens;
    wchar_t **words = ac_extract_words(lines, lens, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    ac_free_words(words, wlens, count);
}

void test_extract_words_empty(void) {
    wchar_t *lines[] = {(wchar_t *)L""};
    int lens[] = {0};
    int count;
    int *wlens;
    wchar_t **words = ac_extract_words(lines, lens, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    ac_free_words(words, wlens, count);
}

/* --- MatchSubsequence ---------------------------------------------------- */

void test_match_subsequence_full(void) {
    TEST_ASSERT_TRUE(ac_match_subsequence(L"hello", 5, L"hello", 5));
}

void test_match_subsequence_partial(void) {
    TEST_ASSERT_TRUE(ac_match_subsequence(L"hello", 5, L"hlo", 3));
}

void test_match_subsequence_case_insensitive(void) {
    TEST_ASSERT_TRUE(ac_match_subsequence(L"Hello", 5, L"hello", 5));
}

void test_match_subsequence_no_match(void) {
    TEST_ASSERT_FALSE(ac_match_subsequence(L"hello", 5, L"xyz", 3));
}

void test_match_subsequence_query_longer(void) {
    TEST_ASSERT_FALSE(ac_match_subsequence(L"abc", 3, L"abcd", 4));
}

void test_match_subsequence_empty_query(void) {
    TEST_ASSERT_TRUE(ac_match_subsequence(L"abc", 3, L"", 0));
}

void test_match_subsequence_empty_word(void) {
    TEST_ASSERT_FALSE(ac_match_subsequence(L"", 0, L"a", 1));
}

/* --- ScoreSuggestion ----------------------------------------------------- */

void test_score_prefix_higher(void) {
    int prefix_score = ac_score_suggestion(L"hello", 5, L"hel", 3);
    int subseq_score = ac_score_suggestion(L"help", 4, L"hlp", 3);
    TEST_ASSERT_TRUE(prefix_score > subseq_score);
}

void test_score_shorter_preferred(void) {
    int short_score = ac_score_suggestion(L"foo", 3, L"fo", 2);
    int long_score = ac_score_suggestion(L"foobar", 6, L"fo", 2);
    TEST_ASSERT_TRUE(short_score > long_score);
}

/* --- FindSuggestions ----------------------------------------------------- */

void test_find_suggestions_prefix(void) {
    wchar_t *words[] = {(wchar_t *)L"apple", (wchar_t *)L"application",
                        (wchar_t *)L"banana", (wchar_t *)L"apply", (wchar_t *)L"ape"};
    int lens[] = {5, 11, 6, 5, 3};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 5, L"app", 3, 10, &count);
    TEST_ASSERT_EQUAL_INT(3, count); /* apple, application, apply */
    ac_free_suggestions(s, count);
}

void test_find_suggestions_max_limit(void) {
    wchar_t *words[] = {(wchar_t *)L"apple", (wchar_t *)L"application",
                        (wchar_t *)L"apply", (wchar_t *)L"ape"};
    int lens[] = {5, 11, 5, 3};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 4, L"a", 1, 2, &count);
    TEST_ASSERT_EQUAL_INT(2, count);
    ac_free_suggestions(s, count);
}

void test_find_suggestions_no_match(void) {
    wchar_t *words[] = {(wchar_t *)L"apple", (wchar_t *)L"banana"};
    int lens[] = {5, 6};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 2, L"xyz", 3, 10, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(s);
}

void test_find_suggestions_excludes_exact_match(void) {
    /* If prefix exactly matches a word, that word should be excluded */
    wchar_t *words[] = {(wchar_t *)L"import"};
    int lens[] = {6};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 1, L"import", 6, 10, &count);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(s);
}

void test_find_suggestions_exact_match_keeps_others(void) {
    /* Exact match excluded, but other longer matches survive */
    wchar_t *words[] = {(wchar_t *)L"import", (wchar_t *)L"important"};
    int lens[] = {6, 9};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 2, L"import", 6, 10, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(9, s[0].word_len); /* "important" only */
    ac_free_suggestions(s, count);
}

void test_find_suggestions_case_different_not_excluded(void) {
    /* Different casing is NOT an exact match — suggestion kept */
    wchar_t *words[] = {(wchar_t *)L"Import"};
    int lens[] = {6};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 1, L"import", 6, 10, &count);
    TEST_ASSERT_EQUAL_INT(1, count);
    ac_free_suggestions(s, count);
}

void test_find_suggestions_subsequence(void) {
    wchar_t *words[] = {(wchar_t *)L"apple", (wchar_t *)L"ape"};
    int lens[] = {5, 3};
    int count;
    Suggestion *s = ac_find_suggestions(words, lens, 2, L"ae", 2, 10, &count);
    TEST_ASSERT_TRUE(count > 0);
    ac_free_suggestions(s, count);
}

/* --- AutocompleteState --------------------------------------------------- */

void test_ac_state_new(void) {
    AutocompleteState *ac = ac_state_new();
    TEST_ASSERT_NOT_NULL(ac);
    TEST_ASSERT_FALSE(ac->active);
    TEST_ASSERT_EQUAL_INT(0, ac->selected_idx);
    ac_state_free(ac);
}

void test_ac_selected_nil(void) {
    TEST_ASSERT_NULL(ac_selected(NULL));
}

void test_ac_selected_empty(void) {
    AutocompleteState ac = {0};
    TEST_ASSERT_NULL(ac_selected(&ac));
}

void test_ac_next_prev(void) {
    AutocompleteState ac = {0};
    Suggestion suggs[3] = {
        {(wchar_t *)L"hello", 5, 100},
        {(wchar_t *)L"help", 4, 90},
        {(wchar_t *)L"hero", 4, 80},
    };
    ac.suggestions = suggs;
    ac.suggestion_count = 3;
    ac.selected_idx = 0;

    ac_next(&ac);
    TEST_ASSERT_EQUAL_INT(1, ac.selected_idx);
    ac_next(&ac);
    TEST_ASSERT_EQUAL_INT(2, ac.selected_idx);
    ac_next(&ac); /* wrap */
    TEST_ASSERT_EQUAL_INT(0, ac.selected_idx);

    ac_prev(&ac); /* wrap to end */
    TEST_ASSERT_EQUAL_INT(2, ac.selected_idx);
    ac_prev(&ac);
    TEST_ASSERT_EQUAL_INT(1, ac.selected_idx);
}

void test_ac_next_nil(void) {
    /* Should not crash */
    ac_next(NULL);
    ac_prev(NULL);
}

void test_ac_get_words_caching(void) {
    AutocompleteState *ac = ac_state_new();
    wchar_t *lines[] = {(wchar_t *)L"hello world foo"};
    int lens[] = {15};
    int count;
    int *wlens;

    wchar_t **words1 = ac_get_words(ac, lines, lens, 1, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_TRUE(count >= 2);
    int count1 = count;

    /* Same mod_count → cached */
    wchar_t **words2 = ac_get_words(ac, lines, lens, 1, 1, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_PTR(words1, words2);
    TEST_ASSERT_EQUAL_INT(count1, count);

    /* Different mod_count → recalculate */
    ac_get_words(ac, lines, lens, 1, 2, -1, -1, &wlens, &count);
    TEST_ASSERT_EQUAL_INT(2, (int)ac->cached_mod_count);

    ac_state_free(ac);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();
    /* Delimiter */
    RUN_TEST(test_is_delimiter_paren);
    RUN_TEST(test_is_delimiter_space);
    RUN_TEST(test_is_delimiter_tab);
    RUN_TEST(test_is_delimiter_dot);
    RUN_TEST(test_is_delimiter_alpha);
    RUN_TEST(test_is_delimiter_digit);
    RUN_TEST(test_is_delimiter_underscore);
    /* ExtractWords */
    RUN_TEST(test_extract_words_simple);
    RUN_TEST(test_extract_words_excludes_position);
    RUN_TEST(test_extract_words_skips_short);
    RUN_TEST(test_extract_words_deduplicates);
    RUN_TEST(test_extract_words_empty);
    /* MatchSubsequence */
    RUN_TEST(test_match_subsequence_full);
    RUN_TEST(test_match_subsequence_partial);
    RUN_TEST(test_match_subsequence_case_insensitive);
    RUN_TEST(test_match_subsequence_no_match);
    RUN_TEST(test_match_subsequence_query_longer);
    RUN_TEST(test_match_subsequence_empty_query);
    RUN_TEST(test_match_subsequence_empty_word);
    /* Score */
    RUN_TEST(test_score_prefix_higher);
    RUN_TEST(test_score_shorter_preferred);
    /* FindSuggestions */
    RUN_TEST(test_find_suggestions_prefix);
    RUN_TEST(test_find_suggestions_max_limit);
    RUN_TEST(test_find_suggestions_no_match);
    RUN_TEST(test_find_suggestions_excludes_exact_match);
    RUN_TEST(test_find_suggestions_exact_match_keeps_others);
    RUN_TEST(test_find_suggestions_case_different_not_excluded);
    RUN_TEST(test_find_suggestions_subsequence);
    /* AutocompleteState */
    RUN_TEST(test_ac_state_new);
    RUN_TEST(test_ac_selected_nil);
    RUN_TEST(test_ac_selected_empty);
    RUN_TEST(test_ac_next_prev);
    RUN_TEST(test_ac_next_nil);
    RUN_TEST(test_ac_get_words_caching);
    return UNITY_END();
}
