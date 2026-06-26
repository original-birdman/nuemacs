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
    size_t want = (need + DYN_INCR) & ~(DYN_INCR - 1);
    if (want > INT_MAX) {
        illegal_dbaction("Attempt to allocate too long a buffer");
    }
    size_t offset = (size_t)(ds->asp - ds->buf);

/* Wish to ensure that buf, asp and alloc are updated correctly
 * even if a SIGWINCH signal arrives after the realloc but
 * before the settings.
 * Unlikely, but...
 */
    sigset_t sigwinch_set, incoming_set;
    sigemptyset(&sigwinch_set);
    sigaddset(&sigwinch_set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &sigwinch_set, &incoming_set);

    ds->buf = Xrealloc(ds->buf, want);
    ds->asp = ds->buf + offset;
    ds->alloc = want;

/* Now we can re-enable the signal */

    sigprocmask(SIG_SETMASK, &incoming_set, NULL);
    return;
}

/* Return the value, but check for NULL and return "" for that
 * Intended for string use.
 */

const char *_dbp_val_nc(db *ds) {
    return ds->asp? ds->asp: "";
}

/* Set value to the n bytes. Never more than an int for n.
 * Cater for being called with mp == NULL and n == 0 (from ltext()?).
 */
void _dbp_setn(db *ds, const void *mp, int n) {
    if (!mp && (n == 0)) mp = "";
    size_t need = (size_t)n;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memcpy(ds->buf, mp, (size_t)n);
    ds->blen = n;
    ds->alen = n;
    ds->asp = ds->buf;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* String copy-in a NUL-terminated string */

void _dbp_set(db *ds, const char *str) {
    if (str) _dbp_setn(ds, str, istrlen(str));
    return;
}

/* Insert n copies of a char into buffer */

void _dbp_replicatech_at(db *ds, char c, int n, int offs) {
    int movers = ds->blen - offs;
    if ((movers < 0) || ((ds->blen - ds->alen) > offs))
        illegal_dbaction("Illegal db replicatech");
    size_t need = (size_t)(ds->blen + n);
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memmove(ds->buf+offs+n, ds->buf+offs, (size_t)movers);
    memset(ds->buf+offs, c, (size_t)n);
    ds->blen += n;
    ds->alen += n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Insert n chars into buffer */

void _dbp_insertn_at(db *ds, const void *mp, int n, int offs) {
    int movers = ds->blen - offs;
    if ((movers < 0) || ((ds->blen - ds->alen) > offs))
        illegal_dbaction("Illegal db insertn");
    size_t need = (size_t)(ds->blen + n);
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memmove(ds->buf+offs+n, ds->buf+offs, (size_t)movers);
    memcpy(ds->buf+offs, mp, (size_t)n);
    ds->blen += n;
    ds->alen += n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Delete n chars from buffer */

void _dbp_deleten_at(db *ds, int n, int offs) {
/* Since we are deleting we must already have enough space
 * But we mustn't delete from before the "actual start pointer".
 */
    if ((n + offs) > ds->blen)  n = ds->blen - offs;
    if (((ds->blen - ds->alen) > offs) || (n < 0))
        illegal_dbaction("Illegal db deleten");
    int movers = ds->blen - offs - n;
    memmove(ds->buf+offs, ds->buf+offs+n, (size_t)movers);
    ds->blen -= n;
    ds->alen -= n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Overwrite n chars at offset.
 * The full length MUST ALREADY be valid for the target!
 */
void _dbp_overwriten_at(db *ds, const void *mp, int n, int offs) {

/* We mustn't change anything from before the "actual start pointer". */
    if (((ds->blen - ds->alen) > offs) || ((offs + n) > ds->blen))
        illegal_dbaction("Illegal db overwriten");
    memmove(ds->buf+offs, mp, (size_t)n);
    return;
}

/* Update the "tail" from an offset.
 * Used when you want a standard prefix, but want to change the
 * rest of the buffer (in a loop?).
 * This can extend the length of the buffer.
 */
void _dbp_retailstr_at(db *ds, const char *ntail, int offs) {

/* We mustn't change anything from before the "actual start pointer". */
    if ((ds->blen - ds->alen) > offs) illegal_dbaction("Illegal db retailstr");

    size_t tlen = strlen(ntail);
    size_t need = tlen + (size_t)offs;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);

    memmove(ds->buf+offs, ntail, tlen);
    int diff = (int)(ds->asp - ds->buf);
    ds->blen = offs + (int)tlen;
    ds->alen = ds->blen - diff;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Set the buffer to n copies of char ch */

void _dbp_bufset(db *ds, const char ch, int n) {
    size_t need = (size_t)n;
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);
    memset(ds->buf, ch, (size_t)n);
    ds->asp = ds->buf;
    ds->blen = n;
    ds->alen = n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
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
void _dbp_truncate(db *ds, int n) {
/* We mustn't change anything from before the "actual start pointer". */
    int offset = ds->alen - ds->blen;
    if ((offset > n) || (n > ds->blen)) {
        illegal_dbaction("Illegal db truncate");
    }
    ds->blen = n;
    ds->alen = n - offset;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Truncate a value after n Unicode chars.
 * We do not need any more space for this.
 */
void _dbp_uctruncate(db *ds, int n) {

    int bpos = 0;;
    while (n--) {
        bpos = next_utf8_offset(ds->buf, bpos, ds->blen, TRUE);
        if (bpos < 0) illegal_dbaction("Illegal db Unicode truncate");
    }
/* We mustn't change anything from before the "actual start pointer". */
    ds->alen = bpos - (int)(ds->asp - ds->buf);
    ds->blen = bpos;
    if ((ds->alen < 0) || (ds->blen < 0))
         illegal_dbaction("Illegal db Unicode truncate");
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Append n bytes
 * Cater for being called with mp == NULL and n == 0 (from ltext()?).
 */
void _dbp_appendn(db *ds, const char *str, int n) {
    if (!str && (n == 0)) str = "";
    size_t need = (size_t)(ds->blen + n);
    if (ds->type & DB_STR) need++;
    if (need > ds->alloc) _dbp_realloc(ds, need);

/* Append n chars, set length and terminate if needed */

    memcpy(ds->buf + ds->blen, str, (size_t)n);
    ds->blen += n;
    ds->alen += n;
    if (ds->type & DB_STR) *(ds->buf+ds->blen) = '\0';
    return;
}

/* Append a string
 * Could this be a define?
 */
void _dbp_append(db *ds, const char *str) {
    _dbp_appendn(ds, str, istrlen(str));
    return;
}

/* Append a character */

void _dbp_addch(db *ds, const char ch) {
    size_t need = (size_t)(ds->blen + 1);
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

char _dbp_charat(db *ds, int w) {
    if (w >= ds->blen) return '\0';
    return *(ds->buf + w);
}

/* Set char at offset (0-based)
 * This DOES NOT APPEND ANY NUL - it assumes it is already there if needed.
 * It DOES set the len iff we put a NUL into a DB_STR type.
 */

void _dbp_setcharat(db *ds, int w, char c) {
/* We are allowed to overwrite the trailing NUL with a NUL in a DB_STR db */
    int tadj = ((c == '\0') && (ds->type & DB_STR))? 1: 0;
    if ((w >= ds->blen + tadj) || ((ds->buf + w) < ds->asp))
        illegal_dbaction("Illegal db setcharat");
    *(ds->buf + w) = c;
/* If we've written a NUL, change the stored length */
    if ((ds->type & DB_STR) && (c == '\0')) {
        ds->blen = w;
        ds->alen = w - (int)(ds->asp - ds->buf);
    }
    return;
}

/* Update the actual string pointer and, from it, the length left.
 * ONLY for UPS buffers.
 * Must be updated to a value within the vald buffer.
 */
void _dbp_upval(db *ds, const char *np) {
    if (!(ds->type & DB_UPS) || (np < ds->buf) || (np > ds->buf + ds->blen)) {
        illegal_dbaction("Illegal db upval");
    }
    ds->asp = (char *)np;
    ds->alen = ds->blen - (int)(ds->asp - ds->buf);
    return;
}

/* sprintf-style call to format a db.
 * NOTE that this always append a NUL char
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
 * We know needed is not -ve, so can remove a compiler warnign with the cast.
 */
    else if ((unsigned)needed >= ds->alloc) {
        _dbp_realloc(ds, (size_t)needed);
        va_start(ap, fmt);
        ds->blen = vsnprintf(ds->buf, ds->alloc, fmt, ap);
        va_end(ap);
    }
    else {
        ds->blen = needed;
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
