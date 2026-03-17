#ifndef EF_XALLOC_H
#define EF_XALLOC_H

#include <stddef.h>
#include <wchar.h>

/* Safe allocation wrappers — abort on failure.
   Use these instead of raw malloc/calloc/realloc/strdup. */

void   *xmalloc(size_t size);
void   *xcalloc(size_t count, size_t size);
void   *xrealloc(void *ptr, size_t size);
char   *xstrdup(const char *s);
wchar_t *xwcsdup(const wchar_t *src, int len);

#endif /* EF_XALLOC_H */
