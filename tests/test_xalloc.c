#include "unity.h"
#include "xalloc.h"

#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

void setUp(void) {}
void tearDown(void) {}

/* --- xmalloc tests ------------------------------------------------------- */

void test_xmalloc_returns_valid_pointer(void) {
    void *p = xmalloc(64);
    TEST_ASSERT_NOT_NULL(p);
    free(p);
}

void test_xmalloc_various_sizes(void) {
    size_t sizes[] = {1, 16, 256, 4096, 65536};
    for (int i = 0; i < 5; i++) {
        void *p = xmalloc(sizes[i]);
        TEST_ASSERT_NOT_NULL(p);
        /* Verify we can write to the allocated memory */
        memset(p, 0xAB, sizes[i]);
        free(p);
    }
}

/* --- xcalloc tests ------------------------------------------------------- */

void test_xcalloc_returns_zeroed_memory(void) {
    int *arr = xcalloc(10, sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0, arr[i]);
    }
    free(arr);
}

void test_xcalloc_single_element(void) {
    char *p = xcalloc(1, sizeof(char));
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT(0, *p);
    free(p);
}

/* --- xrealloc tests ------------------------------------------------------ */

void test_xrealloc_grows_allocation(void) {
    char *p = xmalloc(16);
    memset(p, 'A', 16);
    p = xrealloc(p, 64);
    TEST_ASSERT_NOT_NULL(p);
    /* Original data preserved */
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_INT('A', p[i]);
    }
    free(p);
}

void test_xrealloc_null_acts_like_xmalloc(void) {
    void *p = xrealloc(NULL, 32);
    TEST_ASSERT_NOT_NULL(p);
    free(p);
}

void test_xrealloc_shrinks_allocation(void) {
    char *p = xmalloc(128);
    memset(p, 'B', 128);
    p = xrealloc(p, 16);
    TEST_ASSERT_NOT_NULL(p);
    /* First 16 bytes preserved */
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_EQUAL_INT('B', p[i]);
    }
    free(p);
}

/* --- xstrdup tests ------------------------------------------------------- */

void test_xstrdup_copies_string(void) {
    const char *src = "hello world";
    char *dup = xstrdup(src);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_STRING(src, dup);
    /* Must be a different pointer */
    TEST_ASSERT_NOT_EQUAL(src, dup);
    free(dup);
}

void test_xstrdup_empty_string(void) {
    char *dup = xstrdup("");
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_STRING("", dup);
    free(dup);
}

void test_xstrdup_special_characters(void) {
    const char *src = "café \t\n 日本語";
    char *dup = xstrdup(src);
    TEST_ASSERT_EQUAL_STRING(src, dup);
    free(dup);
}

/* --- xwcsdup tests ------------------------------------------------------- */

void test_xwcsdup_copies_wide_string(void) {
    const wchar_t src[] = L"hello";
    wchar_t *dup = xwcsdup(src, 5);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(dup, src, 5));
    TEST_ASSERT_EQUAL_INT(L'\0', dup[5]);
    free(dup);
}

void test_xwcsdup_empty_string(void) {
    wchar_t *dup = xwcsdup(L"", 0);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_INT(L'\0', dup[0]);
    free(dup);
}

void test_xwcsdup_unicode(void) {
    const wchar_t src[] = L"Привет мир";
    int len = (int)wcslen(src);
    wchar_t *dup = xwcsdup(src, len);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(dup, src, len));
    TEST_ASSERT_EQUAL_INT(L'\0', dup[len]);
    free(dup);
}

void test_xwcsdup_partial_copy(void) {
    const wchar_t src[] = L"hello world";
    wchar_t *dup = xwcsdup(src, 5);
    TEST_ASSERT_NOT_NULL(dup);
    TEST_ASSERT_EQUAL_INT(0, wmemcmp(dup, L"hello", 5));
    TEST_ASSERT_EQUAL_INT(L'\0', dup[5]);
    free(dup);
}

int main(void) {
    setlocale(LC_ALL, "");
    UNITY_BEGIN();

    /* xmalloc */
    RUN_TEST(test_xmalloc_returns_valid_pointer);
    RUN_TEST(test_xmalloc_various_sizes);

    /* xcalloc */
    RUN_TEST(test_xcalloc_returns_zeroed_memory);
    RUN_TEST(test_xcalloc_single_element);

    /* xrealloc */
    RUN_TEST(test_xrealloc_grows_allocation);
    RUN_TEST(test_xrealloc_null_acts_like_xmalloc);
    RUN_TEST(test_xrealloc_shrinks_allocation);

    /* xstrdup */
    RUN_TEST(test_xstrdup_copies_string);
    RUN_TEST(test_xstrdup_empty_string);
    RUN_TEST(test_xstrdup_special_characters);

    /* xwcsdup */
    RUN_TEST(test_xwcsdup_copies_wide_string);
    RUN_TEST(test_xwcsdup_empty_string);
    RUN_TEST(test_xwcsdup_unicode);
    RUN_TEST(test_xwcsdup_partial_copy);

    return UNITY_END();
}
