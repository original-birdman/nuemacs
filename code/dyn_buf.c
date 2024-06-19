/* db.c
 * Utility code to handle dynamic buffers (strings or binary data)
 * the "b" calls are for binary data (which are not NUL-terminated)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "dyn_buf.h"
#include "efunc.h"

/* DYN_INCR MUST be a power of 2 */
#define DYN_INCR 64
static void _dbp_realloc(db *ds, size_t need) {
    size_t want = (need + DYN_INCR) & ~(DYN_INCR - 1);
    dbp_val(ds) = Xrealloc(dbp_val(ds), want);
    dbp_max(ds) = want;
    return;
}

/* Return the value, but check for NULL and return "" for that
 * Intended for string use.
 */

char *_dbp_val_nc(db *ds) {
    return dbp_val(ds)? dbp_val(ds): "";
}

/* Binary data copy in - so NO NUL-termination
 * Also, does not assume current value has terminating NUL.
 */
void _dbp_bset(db *ds, const void *mp, size_t n) {
    if (n > dbp_max(ds)) _dbp_realloc(ds, n);
    memcpy(dbp_val(ds), mp, n);
    dbp_len(ds) = n;
    return;
}

/* Clear a value.
 * Set the length to 0 and, if val is allocated, ensure byte 0 is 0.
 */
void _dbp_clear(db *ds) {
    dbp_len(ds) = 0;
    if (ds->val) *(ds->val) = '\0';
    return;
}

/* String copy in n bytes - NUL-terminate the result */

void _dbp_setn(db *ds, const char *str, size_t n) {
    size_t need = n + 1;    /* Allow for the NUL */
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);

/* Copy in, set length and terminate at n chars */
    memcpy(dbp_val(ds), str, n);
    dbp_len(ds) = n;
    *(dbp_val(ds)+n) = '\0';
    return;
}

/* String copy in a string - so NUL-terminate a binary setting*/

void _dbp_set(db *ds, const char *str) {
    return _dbp_setn(ds, str, strlen(str));
}

/* Append a string of n bytes */

void _dbp_appendn(db *ds, const char *str, size_t n) {
    size_t need = dbp_len(ds) + n + 1;  /* Allow for the NUL */
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate at need-1 chars */

    memcpy(dbp_val(ds) + dbp_len(ds), str, n);
    need--;     /* Length without NUL */
    *(dbp_val(ds)+need) = '\0';
    dbp_len(ds) = need;
    return;
}

/* Append a string
 * Could this be a define?
 */
void _dbp_append(db *ds, const char *str) {
    _dbp_appendn(ds, str, strlen(str));
    return;
}

/* Append a character */

void _dbp_addch(db *ds, const char ch) {
    size_t n = dbp_len(ds) + 1;
    if (n >= dbp_max(ds)) _dbp_realloc(ds, n + 1);

/* We know the destination length, so just drop the new char at the end. */

    char *eloc = dbp_val(ds) + dbp_len(ds);
    *eloc++ = ch;
    *eloc = '\0';
    dbp_len(ds) = n;
    return;
}

/* Get char at offset (0-based) */

char _dbp_charat(db *ds, size_t w) {
    if (w >= dbp_len(ds)) return '\0';
    return *(dbp_val(ds) + w);
}

/* Set char at offset (0-based) */

int _dbp_setcharat(db *ds, size_t w, char c) {
    if (w >= dbp_len(ds)) return 1;
    *(dbp_val(ds) + w) = c;
/* If we've written a NUL, change the stored length */
    if (c == '\0') dbp_len(ds) = w;
    return 0;
}

/* Compare a string.
 * MAKE THESE #defines?
 */

char _dbp_cmp(db *ds, const char *str) {
    return strcmp(dbp_val(ds), str);
}
char _dbp_cmpn(db *ds, const char *str, size_t n) {
    return strncmp(dbp_val(ds), str, n);
}
char _dbp_casecmp(db *ds, const char *str) {
    return strcasecmp(dbp_val(ds), str);
}
char _dbp_casecmpn(db *ds, const char *str, size_t n) {
    return strncasecmp(dbp_val(ds), str, n);
}

/* sprintf-style call to format a db */

void _dbp_sprintf(db *ds, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    dbp_len(ds) = vsnprintf(dbp_val(ds), dbp_max(ds), fmt, ap);
    va_end(ap);
    if (dbp_len(ds) >= dbp_max(ds)) {  /* Go again */
        _dbp_realloc(ds, dbp_len(ds));
        va_start(ap, fmt);
        dbp_len(ds) = vsnprintf(dbp_val(ds), dbp_max(ds), fmt, ap);
        va_end(ap);
    }
}

/* Free (reset) a Dynamic String */

void _dbp_free(db *ds) {
    free(dbp_val(ds));
    dbp_len(ds) = 0;
    dbp_max(ds) = 0;
    dbp_val(ds) = NULL;
    return;
}
