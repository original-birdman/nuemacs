/* dyn_buf.c
 * Utility code to handle dynamic buffers (strings or binary data)
 * All moving is done by mem* calls with specific lengths
 * CANNOT use the dbp_val(x) etc. macros here, as they are marked as
 * const. This is the only place where we should modify them.
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
    ds->val = Xrealloc(ds->val, want);
    ds->alloc = want;
    return;
}

/* Return the value, but check for NULL and return "" for that
 * Intended for string use.
 */

const char *_dbp_val_nc(db *ds) {
    return ds->val? ds->val: "";
}

/* Set value to the n bytes */

void _dbp_setn(db *ds, const void *mp, size_t n) {
    size_t need = n + ds->type;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memcpy(ds->val, mp, n);
    ds->len = n;
    if (ds->type == STR) *(ds->val+ds->len) = '\0';
    return;
}

/* String copy in a NUL-terminated string */

void _dbp_set(db *ds, const char *str) {
    return _dbp_setn(ds, str, strlen(str));
}

/* Insert n chars into buffer */

void _dbp_insertn_at(db *ds, const void *mp, size_t n, size_t offs) {
    int movers = ds->len - offs;
    if (movers < 0) return;   /* Offset too big */
    size_t need = ds->len + n + ds->type;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memmove(ds->val+offs+n, ds->val+offs, movers);
    memcpy(ds->val+offs, mp, n);
    ds->len += n;
    if (ds->type == STR) *(ds->val+ds->len) = '\0';
    return;
}

/* Delete n chars from buffer */

void _dbp_deleten_at(db *ds, int n, size_t offs) {
/* Since we are deleting we must already have enough space */
    if ((n + offs) > ds->len)  n = ds->len - offs;
    if (n < 0) return;
    int movers = ds->len - offs - n;
    memmove(ds->val+offs, ds->val+offs+n, movers);
    ds->len -= n;
    if (ds->type == STR) *(ds->val+ds->len) = '\0';
    return;
}

/* Overwrite n chars at offset.
 * The full length must already be valid for the target.
 */
void _dbp_overwriten_at(db *ds, const void *mp, size_t n, size_t offs) {
     if ((offs + n) > ds->len) return;
     memmove(ds->val+offs, mp, n);
     return;
}

/* Clear a value.
 * Set the length to 0 and, for STR, if val is allocated ensure byte 0 is 0.
 */
void _dbp_clear(db *ds) {
    ds->len = 0;
    if ((ds->val) && (ds->type == STR)) *(ds->val) = '\0';
    return;
}

/* Truncate a value.
 * Do nothing if the current length is less than or equal to the request.
 * If this is a STR buffer, append a NUL.
 * We do not need any more space for this.
 */
void _dbp_truncate(db *ds, size_t n) {
    if (n >= ds->len) return;
    ds->len = n;
    if (ds->type == STR) *(ds->val+ds->len) = '\0';
    return;
}

/* Append n bytes */

void _dbp_appendn(db *ds, const char *str, size_t n) {
    size_t need = ds->len + n + ds->type;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate if needed */

    memcpy(ds->val + ds->len, str, n);
    ds->len += n;
    if (ds->type == STR) *(ds->val+ds->len) = '\0';
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
    size_t need = ds->len + 1 + ds->type;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* We know the destination length, so just drop the new char at the end. */

    char *eloc = ds->val + ds->len;
    *eloc++ = ch;
    ds->len++;
    if (ds->type == STR) *eloc = '\0';
    return;
}

/* Get char at offset (0-based) */

char _dbp_charat(db *ds, size_t w) {
    if (w >= ds->len) return '\0';
    return *(ds->val + w);
}

/* Set char at offset (0-based)
 * This DOES NOT APPEND ANY NUL - it assumes it is already there if needed.
 * It DOES set the len iff we put a NUL into a STR type.
 */

int _dbp_setcharat(db *ds, size_t w, char c) {
    if (w >= ds->len) return 1;
    *(ds->val + w) = c;
/* If we've written a NUL, change the stored length */
    if ((ds->type == STR) && (c == '\0')) ds->len = w;
    return 0;
}

/* Compare a string.
 * The non-n versions only make sense for STR buffers.
 * GML - MAKE THESE #defines?
 */

char _dbp_cmp(db *ds, const char *str) {
    return strcmp(ds->val, str);
}
char _dbp_cmpn(db *ds, const char *str, size_t n) {
    return strncmp(ds->val, str, n);
}
char _dbp_casecmp(db *ds, const char *str) {
    return strcasecmp(ds->val, str);
}
char _dbp_casecmpn(db *ds, const char *str, size_t n) {
    return strncasecmp(ds->val, str, n);
}

/* sprintf-style call to format a db.
 * NOTE that this always append a NUL char
 */

void _dbp_sprintf(db *ds, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ds->len = vsnprintf(ds->val, ds->alloc, fmt, ap);
    va_end(ap);
    if (ds->len >= ds->alloc) {  /* Go again */
        _dbp_realloc(ds, ds->len);
        va_start(ap, fmt);
        ds->len = vsnprintf(ds->val, ds->alloc, fmt, ap);
        va_end(ap);
    }
}

/* Free (reset) a Dynamic String */

void _dbp_free(db *ds) {
    Xfree_setnull(ds->val);
    ds->len = 0;
    ds->alloc = 0;
    return;
}
