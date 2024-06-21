/* dyn_buf.c
 * Utility code to handle dynamic buffers (strings or binary data)
 * All moving is done by mem* calls with specific lengths
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

/* Set value to the n bytes */

void _dbp_setn(db *ds, const void *mp, size_t n) {
    size_t need = n + dbp_type(ds);
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);
    memcpy(dbp_val(ds), mp, n);
    dbp_len(ds) = n;
    if (dbp_type(ds) == STR) *(dbp_val(ds)+dbp_len(ds)) = '\0';
    return;
}

/* String copy in a NUL-terminated string */

void _dbp_set(db *ds, const char *str) {
    return _dbp_setn(ds, str, strlen(str));
}

/* Insert n chars into buffer */

void _dbp_insertn_at(db *ds, const void *mp, size_t n, size_t offs) {
    int movers = dbp_len(ds) - offs;
    if (movers < 0) return;   /* Offset too big */
    size_t need = dbp_len(ds) + n + dbp_type(ds);
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);
    memmove(dbp_val(ds)+offs+n, dbp_val(ds)+offs, movers);
    memcpy(dbp_val(ds)+offs, mp, n);
    dbp_len(ds) += n;
    if (dbp_type(ds) == STR) *(dbp_val(ds)+dbp_len(ds)) = '\0';    
    return;
}

/* Delete n chars from buffer */

void _dbp_deleten_at(db *ds, int n, size_t offs) {
/* Since we are deleting we must already have enough space */
    if ((n + offs) > dbp_len(ds))  n = dbp_len(ds) - offs;
    if (n < 0) return;
    int movers = dbp_len(ds) - offs - n;
    memmove(dbp_val(ds)+offs, dbp_val(ds)+offs+n, movers);
    dbp_len(ds) -= n;
    if (dbp_type(ds) == STR) *(dbp_val(ds)+dbp_len(ds)) = '\0';
    return;
}

/* Clear a value.
 * Set the length to 0 and, for STR, if val is allocated ensure byte 0 is 0.
 */
void _dbp_clear(db *ds) {
    dbp_len(ds) = 0;
    if ((ds->val) && (dbp_type(ds) == STR)) *(ds->val) = '\0';
    return;
}

/* Append n bytes */

void _dbp_appendn(db *ds, const char *str, size_t n) {
    size_t need = dbp_len(ds) + n + dbp_type(ds);
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate if needed */

    memcpy(dbp_val(ds) + dbp_len(ds), str, n);
    dbp_len(ds) += n;
    if (dbp_type(ds) == STR) *(dbp_val(ds)+dbp_len(ds)) = '\0';
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
    size_t need = dbp_len(ds) + 1 + dbp_type(ds);
    if (need > dbp_max(ds)) _dbp_realloc(ds, need);

/* We know the destination length, so just drop the new char at the end. */

    char *eloc = dbp_val(ds) + dbp_len(ds);
    *eloc++ = ch;
    dbp_len(ds)++;
    if (dbp_type(ds) == STR) *eloc = '\0';
    return;
}

/* Get char at offset (0-based) */

char _dbp_charat(db *ds, size_t w) {
    if (w >= dbp_len(ds)) return '\0';
    return *(dbp_val(ds) + w);
}

/* Set char at offset (0-based)
 * This DOES NOT APPEND ANY NUL - it assumes it is already there if needed.
 * It DOES set the len iff we put a NUL into a STR type.
 */

int _dbp_setcharat(db *ds, size_t w, char c) {
    if (w >= dbp_len(ds)) return 1;
    *(dbp_val(ds) + w) = c;
/* If we've written a NUL, change the stored length */
    if ((dbp_type(ds) == STR) && (c == '\0')) dbp_len(ds) = w;
    return 0;
}

/* Compare a string.
 * The non-n versions only make sense for STR buffers.
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

/* sprintf-style call to format a db.
 * NOTE that this always append a NUL char
 */

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
    Xfree_setnull(dbp_val(ds));
    dbp_len(ds) = 0;
    dbp_max(ds) = 0;
    return;
}
