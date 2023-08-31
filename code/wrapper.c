#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char* err) {
    fprintf(stderr, "FATAL: %s\n", err);
    exit(128);
}

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

void Xfree(void *ptr) {
    free(ptr);
    return;
}

char *Xstrdup (const char *ostr) {
    char *nstr = strdup(ostr);
    if (!nstr) die("strdup: Out of memory");
    return nstr;
}
    
