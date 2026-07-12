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

/* We'll take an int, but pass on a size_t for number of elements */
void *Xreallocarray(const void *optr, int n_elem, size_t size) {

/* See man realloc and
 *  https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3621.txt
 */
    if ((n_elem == 0) || (size == 0))
        die("reallocarray: reallocation of 0 is undefined");

/* Centos and Debian Mips 8/9 do not have reallocarray, so we'll need
 * to write one.
 * We'll show good faith and test for overflow.
 * We could use __builtin_mul_overflow for gcc5 and ggc6, but this
 * needs to work for gcc4 too.
 * And the old code isn't for real work - just for different compiler
 * warnings.
 */
    void *ret;
#if __GNUC__ <= 6
    size_t total = n_elem*size;
    if ((total/n_elem) != size) die("reallocarray: Overlarge request");
    ret = realloc(op, total);
#else
    ret = reallocarray((void *)optr, (size_t)n_elem, size);
#endif
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
