#include <stdlib.h>

#include "usage.h"

void *Xmalloc(size_t size) {
    void *ret = malloc(size);
    if (!ret) die("malloc: Out of memory");
    return ret;
}

void *Xrealloc(void *optr, size_t size) {
    void *ret = realloc(optr, size);
    if (!ret) die("realloc: Out of memory");
    return ret;
}
