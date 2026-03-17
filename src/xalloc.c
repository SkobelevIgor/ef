#include "xalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void oom_abort(const char *func) {
    fprintf(stderr, "ef: %s: out of memory\n", func);
    abort();
}

void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) oom_abort("xmalloc");
    return p;
}

void *xcalloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p) oom_abort("xcalloc");
    return p;
}

void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) oom_abort("xrealloc");
    return p;
}

char *xstrdup(const char *s) {
    if (!s) return NULL;
    char *dup = strdup(s);
    if (!dup) oom_abort("xstrdup");
    return dup;
}

wchar_t *xwcsdup(const wchar_t *src, int len) {
    if (len < 0) len = 0;
    wchar_t *dst = malloc(sizeof(wchar_t) * (len + 1));
    if (!dst) oom_abort("xwcsdup");
    wmemcpy(dst, src, len);
    dst[len] = L'\0';
    return dst;
}
