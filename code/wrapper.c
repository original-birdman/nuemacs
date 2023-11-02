#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* NOTE: These allocation routines all exit uemacs on failure.
 * We could try to call exit_via_signal to attempt a dump of modified
 * files but if we've run out of memory that's not likely to work.(?)
 */
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

/* free() on Linux can be sent NULL and it will just do nothing.
 * So we can call Xfree/Xfree_and_set without first testing.
 * If some other system is different then a test can be added here.
 */
void Xfree(void *ptr) {
    free(ptr);
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
