#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* NOTE: These allocation routines all exit uemacs on failure.
 * We could try to call exit_via_signal to attempt a dump of modified
 * files but if we've run out of memory that's not likely to work.(?)
 */
static void die(const char* err) {
    fprintf(stderr, "FATAL: %s\n", err);
    exit(128);
}

void *Xmalloc(size_t size) {
    void *ret = malloc(size);
    if (!ret) die("malloc: Out of memory");
    return ret;
}

void *Xrealloc(const void *optr, size_t size) {
    void *ret = realloc((void *)optr, size);
    if (!ret) die("realloc: Out of memory");
    return ret;
}

/* Centos doesn't have reallocarray, so we'll need to write one.
 * But we''l dispense with the n*isz overflow check.
 */
#if __GNUC__ <= 4
void *reallocarray(void *op, size_t n, size_t isz) {
    return realloc(op, n*isz);
}
#endif

/* We'll take an int, but pass on a size_t for number of elements */
void *Xreallocarray(const void *optr, int n_elem, size_t size) {
    void *ret = reallocarray((void *)optr, (size_t)n_elem, size);
    if (!ret) die("reallocarray: Out of memory");
    return ret;
}

/* free() on Linux can be sent NULL and it will just do nothing.
 * So we can call Xfree/Xfree_and_set without first testing.
 * If some other system is different then a test can be added here.
 */
void Xfree(const void *ptr) {
    free((void *)ptr);
    return;
}

/* Will be used via the Xfree_setnull #define */
void Xfree_and_set(void **ptr) {
    free(*ptr);
    *ptr = NULL;
    return;
}

char *Xstrdup (const char *ostr) {
    char *nstr = strdup(ostr);
    if (!nstr) die("strdup: Out of memory");
    return nstr;
}

/* Update a malloc()ed string value
 * Will be used via the update_val #define
 */
void update_val_func(char **val, const char *newval) {
    *val = Xrealloc(*val, strlen(newval)+1);
    strcpy(*val, newval);
}
