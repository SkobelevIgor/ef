#include "unity.h"
#include "syntax.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

void setUp(void) {}
void tearDown(void) {}

/* --- syntax_parse_color -------------------------------------------------- */

void test_parse_color_green(void) {
    TEST_ASSERT_EQUAL(2, syntax_parse_color("green")); /* COLOR_GREEN = 2 */
}

void test_parse_color_yellow(void) {
    TEST_ASSERT_EQUAL(3, syntax_parse_color("yellow"));
}

void test_parse_color_blue(void) {
    TEST_ASSERT_EQUAL(4, syntax_parse_color("blue"));
}

void test_parse_color_purple(void) {
    TEST_ASSERT_EQUAL(5, syntax_parse_color("purple")); /* COLOR_MAGENTA */
}

void test_parse_color_gray(void) {
    TEST_ASSERT_EQUAL(8, syntax_parse_color("gray"));
}

void test_parse_color_case_insensitive(void) {
    TEST_ASSERT_EQUAL(2, syntax_parse_color("GREEN"));
    TEST_ASSERT_EQUAL(2, syntax_parse_color("Green"));
}

void test_parse_color_null(void) {
    TEST_ASSERT_EQUAL(-1, syntax_parse_color(NULL));
}

void test_parse_color_empty(void) {
    TEST_ASSERT_EQUAL(-1, syntax_parse_color(""));
}

void test_parse_color_unknown(void) {
    TEST_ASSERT_EQUAL(-1, syntax_parse_color("neon"));
}

/* --- syntax_highlight_line ----------------------------------------------- */

void test_highlight_empty_line(void) {
    SyntaxRuleConfig rule = {"\\bfoo\\b", {"green", false}, 10};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, L"", 0, &count);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_null_highlighter(void) {
    int count;
    SyntaxToken *tokens = syntax_highlight_line(NULL, L"foo", 3, &count);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(tokens);
}

void test_highlight_no_rules(void) {
    SyntaxHighlighter *h = syntax_highlighter_new(NULL, 0);
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, L"foo", 3, &count);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_keyword(void) {
    SyntaxRuleConfig rule = {
        "\\b(if|else|for)\\b", {"yellow", false}, 10
    };
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"if (x) else y";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    /* Should have 2 tokens: "if" and "else" */
    TEST_ASSERT_EQUAL(2, count);

    TEST_ASSERT_EQUAL(0, tokens[0].start);
    TEST_ASSERT_EQUAL(2, tokens[0].end);

    TEST_ASSERT_EQUAL(7, tokens[1].start);
    TEST_ASSERT_EQUAL(11, tokens[1].end);

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_string_literal(void) {
    SyntaxRuleConfig rule = {
        "\"([^\"\\\\]|\\\\.)*\"", {"green", false}, 35
    };
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"x = \"hello world\"";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(4, tokens[0].start);   /* opening quote */
    TEST_ASSERT_EQUAL(17, tokens[0].end);     /* after closing quote */

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_priority(void) {
    SyntaxRuleConfig rules[2] = {
        {"\\w+\\(", {"yellow", false}, 8},      /* function call, low prio */
        {"\\b(if|for)\\b", {"blue", true}, 10}, /* keyword, high prio */
    };
    SyntaxHighlighter *h = syntax_highlighter_new(rules, 2);

    /* "if(" — "if" matches both keyword and function call.
       Keyword has higher priority, so "if" should be blue. */
    wchar_t *line = L"if(x)";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_GREATER_THAN(0, count);
    /* First token should cover "if" with keyword style (blue, bold) */
    TEST_ASSERT_EQUAL(0, tokens[0].start);
    TEST_ASSERT_EQUAL(4, syntax_parse_color("blue"));
    TEST_ASSERT_EQUAL(tokens[0].fg_color, syntax_parse_color("blue"));
    TEST_ASSERT_TRUE(tokens[0].bold);

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_adjacent_merge(void) {
    /* A pattern that matches multiple adjacent chars should merge into one token */
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"x = 12345";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(4, tokens[0].start);
    TEST_ASSERT_EQUAL(9, tokens[0].end);

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_multiple_matches(void) {
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"a = 1 + 20 + 300";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(4, tokens[0].start);
    TEST_ASSERT_EQUAL(5, tokens[0].end);

    TEST_ASSERT_EQUAL(8, tokens[1].start);
    TEST_ASSERT_EQUAL(10, tokens[1].end);

    TEST_ASSERT_EQUAL(13, tokens[2].start);
    TEST_ASSERT_EQUAL(16, tokens[2].end);

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_unicode(void) {
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"\x65E5\x672C = 42"; /* 日本 = 42 */
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(5, tokens[0].start); /* "42" starts at char 5 */
    TEST_ASSERT_EQUAL(7, tokens[0].end);

    free(tokens);
    syntax_highlighter_free(h);
}

void test_highlight_invalid_pattern(void) {
    SyntaxRuleConfig rule = {"[invalid", {"red", false}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);
    /* Should not crash, just skip the invalid pattern */
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, L"test", 4, &count);
    TEST_ASSERT_EQUAL(0, count);
    free(tokens);
    syntax_highlighter_free(h);
}

/* --- syntax_token_at ----------------------------------------------------- */

void test_token_at_found(void) {
    SyntaxToken tokens[] = {{0, 3, 2, false}, {5, 10, 4, true}};
    const SyntaxToken *t = syntax_token_at(tokens, 2, 6);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(5, t->start);
    TEST_ASSERT_EQUAL(10, t->end);
}

void test_token_at_not_found(void) {
    SyntaxToken tokens[] = {{0, 3, 2, false}, {5, 10, 4, true}};
    const SyntaxToken *t = syntax_token_at(tokens, 2, 4);
    TEST_ASSERT_NULL(t);
}

void test_token_at_empty(void) {
    const SyntaxToken *t = syntax_token_at(NULL, 0, 0);
    TEST_ASSERT_NULL(t);
}

void test_token_at_start_boundary(void) {
    SyntaxToken tokens[] = {{5, 10, 2, false}};
    const SyntaxToken *t = syntax_token_at(tokens, 1, 5);
    TEST_ASSERT_NOT_NULL(t);
}

void test_token_at_end_boundary(void) {
    SyntaxToken tokens[] = {{5, 10, 2, false}};
    const SyntaxToken *t = syntax_token_at(tokens, 1, 10);
    TEST_ASSERT_NULL(t); /* end is exclusive */
}

/* --- Highlight cache ----------------------------------------------------- */

void test_cache_miss_then_hit(void) {
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);
    HighlightCache *cache = highlight_cache_new(h);

    wchar_t *line = L"x = 42";
    int count1, count2;

    /* First call: cache miss */
    const SyntaxToken *t1 = highlight_cache_get_tokens(cache, 0, line,
                                                        wcslen(line), &count1);
    TEST_ASSERT_EQUAL(1, count1);
    TEST_ASSERT_NOT_NULL(t1);

    /* Second call: cache hit (same pointer) */
    const SyntaxToken *t2 = highlight_cache_get_tokens(cache, 0, line,
                                                        wcslen(line), &count2);
    TEST_ASSERT_EQUAL(count1, count2);
    TEST_ASSERT_EQUAL_PTR(t1, t2);

    highlight_cache_free(cache); /* Also frees the highlighter */
}

void test_cache_invalidation(void) {
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);
    HighlightCache *cache = highlight_cache_new(h);

    wchar_t *line = L"x = 42";
    int count;
    highlight_cache_get_tokens(cache, 0, line, wcslen(line), &count);
    TEST_ASSERT_EQUAL(1, count);

    /* Invalidate and re-query with different content */
    highlight_cache_invalidate(cache, 0);
    wchar_t *line2 = L"hello";
    const SyntaxToken *t = highlight_cache_get_tokens(cache, 0, line2,
                                                       wcslen(line2), &count);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(t);

    highlight_cache_free(cache);
}

void test_cache_null_highlighter(void) {
    HighlightCache *cache = highlight_cache_new(NULL);
    int count;
    const SyntaxToken *t = highlight_cache_get_tokens(cache, 0, L"test", 4,
                                                       &count);
    TEST_ASSERT_EQUAL(0, count);
    TEST_ASSERT_NULL(t);
    highlight_cache_free(cache);
}

void test_cache_multiple_lines(void) {
    SyntaxRuleConfig rule = {"\\d+", {"blue", true}, 5};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);
    HighlightCache *cache = highlight_cache_new(h);

    int c0, c1;
    highlight_cache_get_tokens(cache, 0, L"a = 1", 5, &c0);
    highlight_cache_get_tokens(cache, 1, L"b = 2", 5, &c1);
    TEST_ASSERT_EQUAL(1, c0);
    TEST_ASSERT_EQUAL(1, c1);

    highlight_cache_free(cache);
}

/* --- Comment pattern (realistic) ----------------------------------------- */

void test_highlight_comment(void) {
    SyntaxRuleConfig rule = {"//.*$", {"gray", false}, 30};
    SyntaxHighlighter *h = syntax_highlighter_new(&rule, 1);

    wchar_t *line = L"x = 1 // comment";
    int count;
    SyntaxToken *tokens = syntax_highlight_line(h, line, wcslen(line), &count);

    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(6, tokens[0].start);  /* "// comment" */
    TEST_ASSERT_EQUAL(16, tokens[0].end);

    free(tokens);
    syntax_highlighter_free(h);
}

/* --- Runner -------------------------------------------------------------- */

int main(void) {
    UNITY_BEGIN();

    /* parse_color */
    RUN_TEST(test_parse_color_green);
    RUN_TEST(test_parse_color_yellow);
    RUN_TEST(test_parse_color_blue);
    RUN_TEST(test_parse_color_purple);
    RUN_TEST(test_parse_color_gray);
    RUN_TEST(test_parse_color_case_insensitive);
    RUN_TEST(test_parse_color_null);
    RUN_TEST(test_parse_color_empty);
    RUN_TEST(test_parse_color_unknown);

    /* highlight_line */
    RUN_TEST(test_highlight_empty_line);
    RUN_TEST(test_highlight_null_highlighter);
    RUN_TEST(test_highlight_no_rules);
    RUN_TEST(test_highlight_keyword);
    RUN_TEST(test_highlight_string_literal);
    RUN_TEST(test_highlight_priority);
    RUN_TEST(test_highlight_adjacent_merge);
    RUN_TEST(test_highlight_multiple_matches);
    RUN_TEST(test_highlight_unicode);
    RUN_TEST(test_highlight_invalid_pattern);
    RUN_TEST(test_highlight_comment);

    /* token_at */
    RUN_TEST(test_token_at_found);
    RUN_TEST(test_token_at_not_found);
    RUN_TEST(test_token_at_empty);
    RUN_TEST(test_token_at_start_boundary);
    RUN_TEST(test_token_at_end_boundary);

    /* cache */
    RUN_TEST(test_cache_miss_then_hit);
    RUN_TEST(test_cache_invalidation);
    RUN_TEST(test_cache_null_highlighter);
    RUN_TEST(test_cache_multiple_lines);

    return UNITY_END();
}
