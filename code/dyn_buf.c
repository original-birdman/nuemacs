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

/* DYN_INCR MUST be a power of 2
 * This must update both ds->buf and ds->asp
 */
#define DYN_INCR 64
static void _dbp_realloc(db *ds, size_t need) {
    size_t want = (need + DYN_INCR) & ~(DYN_INCR - 1);
    size_t offset = ds->asp - ds->buf;
    ds->buf = Xrealloc(ds->buf, want);
    ds->asp = ds->buf + offset;
    ds->alloc = want;
    return;
}

/* Return the value, but check for NULL and return "" for that
 * Intended for string use.
 */

const char *_dbp_val_nc(db *ds) {
    return ds->asp? ds->asp: "";
}

/* Set value to the n bytes */

void _dbp_setn(db *ds, const void *mp, size_t n) {
    size_t need = n;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memcpy(ds->buf, mp, n);
    ds->blen = n;
    ds->alen = n;
    ds->asp = ds->buf;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* String copy-in a NUL-terminated string */

void _dbp_set(db *ds, const char *str) {
    return _dbp_setn(ds, str, strlen(str));
}

/* Insert n chars into buffer */

void _dbp_insertn_at(db *ds, const void *mp, size_t n, size_t offs) {
    int movers = ds->blen - offs;
    if (movers < 0) return;   /* Offset too big */
    if ((ds->blen - ds->alen) > offs) return;   /* No insert before valid */
    size_t need = ds->blen + n;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memmove(ds->buf+offs+n, ds->buf+offs, movers);
    memcpy(ds->buf+offs, mp, n);
    ds->blen += n;
    ds->alen += n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Delete n chars from buffer */

void _dbp_deleten_at(db *ds, int n, size_t offs) {
/* Since we are deleting we must already have enough space
 * But we mustn't delete from before the "actual start pointer".
 */
    if ((ds->blen - ds->alen) > offs) return;
    if ((n + offs) > ds->blen)  n = ds->blen - offs;
    if (n < 0) return;
    int movers = ds->blen - offs - n;
    memmove(ds->buf+offs, ds->buf+offs+n, movers);
    ds->blen -= n;
    ds->alen -= n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Overwrite n chars at offset.
 * The full length must already be valid for the target.
 */
void _dbp_overwriten_at(db *ds, const void *mp, size_t n, size_t offs) {

/* We mustn't change anything from before the "actual start pointer". */
    if ((ds->blen - ds->alen) > offs) return;
    if ((offs + n) > ds->blen) return;
    memmove(ds->buf+offs, mp, n);
    return;
}

/* Clear a value.
 * Set the length to 0 and, for DB_STR, if val is allocated ensure byte 0 is 0.
 */
void _dbp_clear(db *ds) {
    ds->blen = 0;
    ds->alen = 0;
    ds->asp = ds->buf;
    if ((ds->buf) && (ds->type & DB_STR)) *(ds->buf) = '\0';
    return;
}

/* Truncate a value.
 * Do nothing if the current length is less than or equal to the request.
 * If this is a DB_STR buffer, append a NUL.
 * We do not need any more space for this.
 */
void _dbp_truncate(db *ds, size_t n) {
/* We mustn't change anything from before the "actual start pointer". */
    if ((ds->blen - ds->alen) > n) return;
    if (n >= ds->blen) return;
    size_t offset = ds->asp - ds->buf;
    ds->blen = n;
    ds->alen = n - offset;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Append n bytes */

void _dbp_appendn(db *ds, const char *str, size_t n) {
    size_t need = ds->blen + n;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate if needed */

    memcpy(ds->buf + ds->blen, str, n);
    ds->blen += n;
    ds->alen += n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
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
    size_t need = ds->blen + 1;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* We know the destination length, so just drop the new char at the end. */

    char *eloc = ds->buf + ds->blen;
    *eloc = ch;
    ds->blen++;
    ds->alen++;
    if (ds->type & DB_STR) *(++eloc) = '\0';
    return;
}

/* Get char at offset (0-based) */

char _dbp_charat(db *ds, size_t w) {
    if (w >= ds->blen) return '\0';
    return *(ds->buf + w);
}

/* Set char at offset (0-based)
 * This DOES NOT APPEND ANY NUL - it assumes it is already there if needed.
 * It DOES set the len iff we put a NUL into a DB_STR type.
 */

int _dbp_setcharat(db *ds, size_t w, char c) {
    if (w >= ds->blen) return 1;
    *(ds->buf + w) = c;
/* If we've written a NUL, change the stored length */
    if ((ds->type & DB_STR) && (c == '\0')) ds->blen = w;
    return 0;
}

/* Compare a string.
 * The non-n versions only make sense for DB_STR buffers.
 * GML - MAKE THESE #defines?
 */

char _dbp_cmp(db *ds, const char *str) {
    return strcmp(ds->buf, str);
}
char _dbp_cmpn(db *ds, const char *str, size_t n) {
    return strncmp(ds->buf, str, n);
}
char _dbp_casecmp(db *ds, const char *str) {
    return strcasecmp(ds->buf, str);
}
char _dbp_casecmpn(db *ds, const char *str, size_t n) {
    return strncasecmp(ds->buf, str, n);
}

/* Update the actual string pointer and, from it, the length left.
 * ONLY for UPS buffers.
 * Must be updated to a value within the vald buffer.
 */
void _dbp_upval(db *ds, char *np) {
    if (!(ds->type & DB_UPS)) return;
    if (np < ds->buf) return;
    if (np > ds->buf + ds->blen) return;
    ds->asp = np;
    ds->alen = ds->blen - (ds->asp - ds->buf);
    return;
}

/* sprintf-style call to format a db.
 * NOTE that this always append a NUL char
 */

void _dbp_sprintf(db *ds, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ds->blen = vsnprintf(ds->buf, ds->alloc, fmt, ap);
    va_end(ap);
    if (ds->blen >= ds->alloc) {  /* Go again */
        _dbp_realloc(ds, ds->blen);
        va_start(ap, fmt);
        ds->blen = vsnprintf(ds->buf, ds->alloc, fmt, ap);
        va_end(ap);
    }
    ds->alen = ds->blen;
    ds->asp = ds->buf;
}

/* Free (reset) a Dynamic String */

void _dbp_free(db *ds) {
    Xfree_setnull(ds->buf);
    ds->asp = NULL;
    ds->alloc = 0;
    ds->alen = 0;
    ds->blen = 0;
    return;
}
