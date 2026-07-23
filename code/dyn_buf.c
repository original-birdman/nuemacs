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
#include <errno.h>
#include <signal.h>

#include "dyn_buf.h"
#include "efunc.h"

/* An internal routine to rasie a signal if we try to set a value
 * at an illegal offset etc.
 * Will set a message to be shown.
 */

static void illegal_dbaction(const char *why) {
    dump_message = why;
    raise(SIGILL);
    exit(127);  /* Just in case... */
}

/* DYN_INCR MUST be a power of 2
 * This must update both ds->buf and ds->asp
 */
#define DYN_INCR (size_t)64
static void _dbp_realloc(db *ds, size_t need) {
    size_t want = (need + DYN_INCR - 1) & ~(DYN_INCR - 1);
    if (want > INT_MAX) {
        illegal_dbaction("Attempt to allocate too long a buffer");
    }
    size_t offset = (size_t)(ds->asp - ds->buf);
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

/* Set the buffer to the n bytes. Never more than an int for n.
 * Cater for being called with mp == NULL and n == 0 (from ltext()?).
 * Sets asp == buf.
 */
void _dbp_setn(db *ds, const void *mp, int n) {
    if (!mp && (n == 0)) mp = "";
    size_t need = (size_t)n + 1;    /* Always append a NUL */
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memcpy(ds->buf, mp, (size_t)n);
    ds->alen = n;
    ds->asp = ds->buf;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* String copy-in a NUL-terminated string */

void _dbp_set(db *ds, const char *str) {
    if (str) _dbp_setn(ds, str, istrlen(str));
    return;
}

/* _dbp_replicatech_at() and _dbp_insertn_at() only differ in how they
 * fill the created gap, so route through common code.
 * Acts on the asp value.
 */
enum repins_call_t { DBP_REPLICATE, DBP_INSERTN };
static void _dbp_ri_at(db *ds, const char *cp, int n, int offs,
     enum repins_call_t method) {
    int movers = ds->alen - offs;
    if ((movers < 0) || (offs < 0))
         illegal_dbaction("Illegal db replicatech/insertn");
    size_t need = (size_t)((ds->asp - ds->buf) + ds->alen + n) + 1;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memmove(ds->asp+offs+n, ds->asp+offs, (size_t)movers);
    if (method == DBP_REPLICATE)
        memset(ds->asp+offs, *cp, (size_t)n);
    else        /* DBP_INSERTN */
        memcpy(ds->asp+offs, cp, (size_t)n);
    ds->alen += n;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Insert n copies of a char into buffer */

void _dbp_replicatech_at(db *ds, char c, int n, int offs) {
    _dbp_ri_at(ds, &c, n, offs, DBP_REPLICATE);
    return;
}

/* Insert n chars into buffer */

void _dbp_insertn_at(db *ds, const void *mp, int n, int offs) {
    _dbp_ri_at(ds, mp, n, offs, DBP_INSERTN);
    return;
}

/* Delete n chars from buffer
 * If n chars takes you past the end of the buffer, just delete
 * to end of buffer (i.e. truncate at n).
 * Acts on the asp value.
 */
void _dbp_deleten_at(db *ds, int n, int offs) {
    if ((n < 0) || (offs < 0)) illegal_dbaction("Illegal db deleten");
/* Since we are deleting we must already have enough space
 * But we mustn't delete from before the "actual start pointer"
 */
    int end = n + offs;
    if (end == 0) return;   /* n and offs are both 0 - a no-op */
    if (end < 0) illegal_dbaction("end overflow in deleten");
    if (end > ds->alen)  {
        n = ds->alen - offs;    /* Nothing to move - just a truncate */
    }
    else {
        memmove(ds->asp+offs, ds->asp+offs+n, (size_t)(ds->alen - end));
    }
    ds->alen -= n;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Overwrite n chars at offset.
 * The full length MUST ALREADY be valid for the target!
 * Acts on the asp value.
 */
void _dbp_overwriten_at(db *ds, const void *mp, int n, int offs) {
    if ((n < 0) || (offs < 0)) illegal_dbaction("Illegal db overwriten");
    if (n == 0) return;     /* Nothing to do */
/* We mustn't change anything beyond the current end of data */
    int end = n + offs;     /* Must be > 0, as n == 0 has returned */
    if (end <= 0) illegal_dbaction("end overflow in overwriten");
    if (end > ds->alen) illegal_dbaction("Illegal db overwriten");
    memmove(ds->asp+offs, mp, (size_t)n);
    return;
}

/* Update the "tail" from an offset.
 * Used when you want a standard prefix, but want to change the
 * rest of the buffer (in a loop?).
 * This can extend the length of the buffer.
 * Acts on the asp value.
 */
void _dbp_retailstr_at(db *ds, const char *ntail, int offs) {

    size_t tlen = strlen(ntail);
    size_t need = tlen + (size_t)offs + 1;
    if (need > ds->alloc) _dbp_realloc(ds, need);

    memmove(ds->asp+offs, ntail, tlen);
    ds->alen = offs + (int)tlen;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Set the buffer to n copies of char ch
 * Sets asp == buf.
 */

void _dbp_bufset(db *ds, const char ch, int n) {
    size_t need = (size_t)n + 1;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    ds->asp = ds->buf;
    ds->alen = n;
    memset(ds->asp, ch, (size_t)n);
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Clear a value.
 * Set the length to 0 and, if buf is allocated, ensure byte 0 is 0.
 * Sets asp == buf.
 */
void _dbp_clear(db *ds) {
    ds->alen = 0;
    ds->asp = ds->buf;
    if (ds->buf) *(ds->asp) = '\0';
    return;
}

/* Truncate a value, appending a NUL.
 * We do not need any more space for this.
 * Acts on the asp value.
 */
void _dbp_truncate(db *ds, int n) {
    if ((n < 0) || (n > ds->alen)) illegal_dbaction("Illegal db truncate");
    ds->alen = n;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Truncate a value after n Unicode chars.
 * We do not need any more space for this.
 * Acts on the asp value.
 */
void _dbp_uctruncate(db *ds, int n) {

    int bpos = 0;;
    while (n--) {
        bpos = next_utf8_offset(ds->asp, bpos, ds->alen, TRUE);
        if (bpos < 0) illegal_dbaction("Illegal db Unicode truncate");
    }
/* We mustn't change anything from before the "actual start pointer". */
    ds->alen = bpos;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Append n bytes
 * Cater for being called with mp == NULL and n == 0 (from ltext()?).
 * Acts on the asp value.
 */
void _dbp_appendn(db *ds, const char *str, int n) {
    if (!str && (n == 0)) str = "";
    size_t need = (size_t)(ds->alen + n) + 1;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate if needed */

    memcpy(ds->asp + ds->alen, str, (size_t)n);
    ds->alen += n;
    *(ds->asp+ds->alen) = '\0';
    return;
}

/* Append a string
 * Could this be a define?
 * Acts on the asp value.
 */
void _dbp_append(db *ds, const char *str) {
    _dbp_appendn(ds, str, istrlen(str));
    return;
}

/* Append a character
 * Acts on the asp value.
 */
void _dbp_addch(db *ds, const char ch) {
    size_t need = (size_t)(ds->alen + 2);   /* 2 for new char + NUL */
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* We know the destination length, so just drop the new char at the end. */

    char *eloc = ds->asp + ds->alen;
    *eloc = ch;
    ds->alen++;
    *(++eloc) = '\0';
    return;
}

/* Get char at offset (0-based)
 * Acts on the asp value.
 */
char _dbp_charat(db *ds, int w) {
    if (w >= ds->alen) return '\0';
    return *(ds->asp + w);
}

/* Set char at offset (0-based)
 * This DOES NOT APPEND ANY NUL - it assumes it is already there.
 * It DOES NOT change alen, even if you set a NUL.
 * Acts on the asp value.
 */
void _dbp_setcharat(db *ds, int w, char c) {
/* We can only overwrite data that is already there */
    if ((w < 0) || (w >= ds->alen)) illegal_dbaction("Illegal db setcharat");
    *(ds->asp + w) = c;
    return;
}

/* Update the actual string pointer and, from it, the length left.
 * ONLY for UPS buffers.
 * For use in code where a function grabs successive tokens .
 * This does not change anything in the buffer.
 * Must be updated to a valid value within the buffer.
 * Acts on the asp value.
 */
void _dbp_upval(db *ds, const char *np) {
    if (!(ds->flags & DB_UPS) || (np < ds->buf) || (np > ds->asp + ds->alen)) {
        illegal_dbaction("Illegal db upval");
    }
    ds->alen -= (int)(np - ds->asp);    /* Decrease by how much ptr moves */
    ds->asp = (char *)np;
    return;
}

/* sprintf-style call to format a db.
 * NOTE that this always append a NUL char
 * Sets asp == buf.
 */
void _dbp_sprintf(db *ds, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int needed = vsnprintf(ds->buf, ds->alloc, fmt, ap);
    va_end(ap);
    if (needed < 0) {   /* Error */
        dbp_set(ds, "db(p)_sprintf error: ");
        dbp_append(ds, strerror(errno));
    }
/* ds->buf not large enough. Go again.
 * We know needed is not -ve, so can remove a compiler warning with the cast.
 * Add the 1 for the trailing NUL.
 */
    else if ((unsigned)needed >= ds->alloc) {
        _dbp_realloc(ds, (size_t)needed + 1);
        va_start(ap, fmt);
        ds->alen = vsnprintf(ds->buf, ds->alloc, fmt, ap);
        va_end(ap);
    }
    else {
        ds->alen = needed;
    }
    ds->asp = ds->buf;
    return;
}

/* Free (reset) a Dynamic String */

void _dbp_free(db *ds) {
    Xfree_setnull(ds->buf);
    ds->asp = NULL;
    ds->alloc = 0;
    ds->alen = 0;
    return;
}
