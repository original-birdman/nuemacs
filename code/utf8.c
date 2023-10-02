#include <stdio.h>

#define UTF8_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"
#include "util.h"
#include "utf8proc.h"

/* utf8_to_unicode()
 *
 * Convert a UTF-8 sequence to its unicode value, and return the length of
 * the sequence in bytes.
 *
 * NOTE! Invalid UTF-8 will be converted to a one-byte sequence, so you can
 * either use it as-is (ie as Latin1) or you can check for invalid UTF-8
 * by checking for a length of 1 and a result > 127.
 *
 * NOTE 2! This does *not* verify things like minimality. So overlong forms
 * are happily accepted and decoded, as are the various "invalid values".
 *
 * GGR NOTE! This code assumes that the incoming index is <= len.
 *           So it can always return a +ve result, and a "valid" res.
 *  NOW CHANGED to return 0 and a unicode char of 0.
 */
unsigned utf8_to_unicode(char *line, unsigned index, unsigned len,
     unicode_t *res) {
    if (index >= len) {
        *res = UEM_NOCHAR;
        return 0;
    }

    unsigned value;
    unsigned char c = line[index];
    unsigned bytes, mask, i;

    *res = c;
    line += index;
    len -= index;

/* 0xxxxxxx is valid utf8
 * 10xxxxxx is invalid UTF-8, we assume it is Latin1
 */
    if (c < 0xc0) return 1;

/* OK, it's 11xxxxxx, do a stupid decode */
    mask = 0x20;
    bytes = 2;
    while (c & mask) {
        bytes++;
        mask >>= 1;
    }

/* Invalid? Do it as a single byte Latin1
 */
    if (bytes > MAX_UTF8_LEN) return 1;
    if (bytes > len) return 1;

    value = c & (mask-1);

/* OK, do the bytes */
    for (i = 1; i < bytes; i++) {
        c = line[i];
        if ((c & 0xc0) != 0x80) return 1;
        value = (value << 6) | (c & 0x3f);
    }
    if (value > MAX_UTF8_CHAR) {
        return 1;
    }
    *res = value;
    return bytes;
}

/* Check whether the character is a Combining Class one - needed for
 * grapheme handling in display.c
 * The combi_range array is largish for a check-per-character but
 * since most expected characters are below the first entry it
 * will actually be quick - provided we make teh quick check first.
 * A better(?) test might just be to get the utf8proc_category_string()
 * for the character and see whether it starts with "M" (Mn, Mc, Me).
 * But still include a lower range check at least....
 */
#include "combi.h"
#define CR_MAX (ARRAY_SIZE(combi_range) - 1)

int combining_type(unicode_t uc) {

    if ((uc < combi_range[0].start) || (uc > combi_range[CR_MAX].start))
        return FALSE;

/* Binary chop version */
    int first = 0;
    int last = CR_MAX;

    while (first <= last) {
        int middle = (first + last)/2;
        if (uc < combi_range[middle].start) {       /* look lower */
            last = middle - 1;
        } else if (uc > combi_range[middle].end) {  /* look higher */
            first = middle + 1;
        } else return TRUE;
    }
    return FALSE;
}

/* unicode_to_utf8()
 *
 * Convert a unicode value to its canonical utf-8 sequence.
 *
 * NOTE! This does not check for - or care about - the "invalid" unicode
 * values.  Also, converting a utf-8 sequence to unicode and back does
 * *not* guarantee the same sequence, since this generates the shortest
 * possible sequence, while utf8_to_unicode() accepts both Latin1 and
 * overlong utf-8 sequences.
 */
unsigned unicode_to_utf8(unsigned int c, char *utf8) {
    int bytes = 1;

    *utf8 = c;
/* We have a unicode point - anything over 0x7f is multi-byte */
    if (c >= 0x80) {
/* The next two values will be the prefix and maximum values storable
 * in the final (-> first) byte *after* we've shifted them.
 */
        unsigned int prefix = 0x80;
        unsigned max = 0x3f;
        char *bp, *ep;
        bp = ep = utf8;
        do {
            *ep++ = 0x80 + (c & 0x3f);
            bytes++;
            prefix = 0x80 | (prefix >> 1);
            max >>= 1;
            c >>= 6;            /* We use 6-bits in each extension byte */
        } while (c > max);
        *ep = (prefix | c);     /* Add in the final byte */

/* We now need to reverse the bytes we've put in.
 * So use two pointers (begin and end of the bytes to reverse)
 * and swap their contents as they move towards each other.
 * Once they meet the work is done.
 */
        do {
            char a = *bp;   /* Original begin */
            *bp++ = *ep;    /* Copy end to begin */
            *ep-- = a;      /* Original begin into end */
        } while (bp < ep);
    }
    return bytes;
}

/* GGR functions to get offset of previous/next character in a buffer.
 * Added here to keep utf8 character handling together
 * Optional (grapheme_start):
 *    check the following character(s) for having a non-zero Combining
 *    Class too.
 */
int
 next_utf8_offset(char *buf, int offs, int max_offset, int grapheme_start) {

    if (offs >= max_offset) return -1;
    unicode_t c;

/* Just use utf8_to_unicode */

    offs += utf8_to_unicode(buf, offs, max_offset, &c);
    if (grapheme_start) {
        while(1) {      /* Look for any attached Combining modifiers */
            int next_incr = utf8_to_unicode(buf, offs, max_offset, &c);
            if (!combining_type(c)) break;
            offs += next_incr;
            if (offs >= max_offset) return -1;
        }
    }
    return offs;
}

/* Step back a byte at a time.
 * If the first byte isn't a utf8 continuation byte (10xxxxxx) that is it.
 * It it *is* a continuation byte look back another byte, up to 3
 * times. If we then find a utf8 leading byte (that is a correct one for
 * the length we have found) we use that.
 * If we fail along the way we revert to the original "back one byte".
 * Optional (grapheme_start):
 *    if we find a Combining character we go back to the next previous one.
 */
int prev_utf8_offset(char *buf, int offset, int grapheme_start) {

    if (offset <= 0) return -1;
    unicode_t res = 0;
    int offs = offset;
    do {
        unsigned char c = buf[--offs];
        res = c;
        unicode_t poss;
        int got_utf8 = 0;
        if ((c & 0xc0) == 0x80) {           /* Ext byte? */
            int trypos = offs;
            int tryb = MAX_UTF8_LEN;
            signed char marker = 0xc0;      /* Extend sign-bit here */
            unsigned char valmask = 0x1f;
            int bits_sofar = 0;
            int addin;
            poss = c & 0x3f;                /* 6-bits */
            while ((--trypos >= 0) && (--tryb >= 0)) {
                c = buf[trypos];
                if ((c & 0xc0) == 0x80) {   /* Ext byte */
                    marker >>= 1;           /* Shift right..*/
                    valmask >>= 1;          /* Fewer... */
                    addin = (c & 0x3f);
                    bits_sofar += 6;
                    addin <<= bits_sofar;
                    poss |= addin;          /* 6-bits more */
                    continue;
                }
/* Have we found a valid start code?
 * NOTE that the test needs marker as an unsigned char, to stop sign
 * extension in the test.
 */
                if ((c & ~valmask) == ch_as_uc(marker)) {
                    addin = (c & valmask);
                    bits_sofar += 6;
                    addin <<= bits_sofar;
                    poss |= addin;
                    offs = trypos;
                    got_utf8 = 1;
                }
                break;                      /* By default, now done */
            }
        }
        if (got_utf8) res = poss;
/* The offs test is > 0, as the first thing to happen in the loop is
 * its decrement, and it mustn't go -ve.
 */
    } while(grapheme_start && (offs > 0) && combining_type(res));
    return offs;
}

/* Routines to build full graphemes...
 * Build the grapheme starting at the current position and return
 * the offset that follows it.
 * So repeated calls move forwards through the buffer, if offs is
 * incremented by the return value.
 * If offs == max -> newline.
 * If offs > max -> UEM_NOCHAR
 * If no_ex_alloc is set, don't allocate any ex fields (but do count them).
 */
int build_next_grapheme(char *buf, int offs, int max, struct grapheme *gc,
    int no_ex_alloc) {
    unicode_t c;

    gc->cdm = 0;    /* Must initialize these two... */
    gc->ex = NULL;  /* ...before offset check returns. */

/* Allow (buf, 0, -1, &gc) to work for NUL-terminated buf string */
    if ((offs == 0) && (max == -1)) max = strlen(buf);
    if (offs >= max) {
        if (offs == max) gc->uc = '\n';
        else             gc->uc = UEM_NOCHAR;
        return ++offs;
    }

    offs += utf8_to_unicode(buf, offs, max, &(gc->uc));
    int first = 1;
    while(1) {      /* Look for any attached Combining modifiers */
        int next_incr = utf8_to_unicode(buf, offs, max, &c);
        if (!combining_type(c)) break;
        offs += next_incr;
        if (first) {
            gc->cdm = c;
            first = 0;
        }
        else if (!no_ex_alloc) {
/* Need to create or extend an ex section.
 * Such section always end with a UEM_NOCHAR entry (to ignore).
 */
            int xc = 0;
            if (gc->ex != NULL) {
                while(gc->ex[xc] != UEM_NOCHAR) xc++;
            }
            gc->ex = Xrealloc(gc->ex, (xc+2)*sizeof(unicode_t));
            gc->ex[xc] = c;
            gc->ex[xc+1] = UEM_NOCHAR;
	}
    }
    return offs;
}
/* Find the start of the prevous grapheme, get that grapheme then
 * return the offset *of its start*.
 * So repeated calls move backwards through the buffer.
 * NOTE: that we don't handle the NUL-terminated buf string here!
 */
int build_prev_grapheme(char *buf, int offs, int max, struct grapheme *gc,
     int no_ex_alloc) {
    int final_off = prev_utf8_offset(buf, offs, TRUE);
    build_next_grapheme(buf, final_off, max, gc, no_ex_alloc);
    return final_off;
}

/* Handler for the char-replace function.
 * Definitions...
 */
#define DFLT_REPCHAR 0xFFFD
static unicode_t repchar = DFLT_REPCHAR;    /* Light Shade */
struct repmap_t {
    unicode_t start;
    unicode_t end;
};
static struct repmap_t *remap = NULL;

/* Code */

int char_replace(int f, int n) {
    UNUSED(f); UNUSED(n);

    int status;
    char buf[NLINE];

    status = mlreply("reset | repchar [U+]xxxx | [U+]xxxx[-[U+]xxxx] ",
          buf, NLINE - 1, CMPLT_NONE);
    if (status != TRUE)         /* Only act on +ve response */
        return status;

    char *rp = buf;
    char tok[NLINE];
    while(*rp != '\0') {
        rp = token(rp, tok, NLINE);
        if (tok[0] == '\0') break;
        if (!strcmp(tok, "reset")) {
            if (remap != NULL) {
                Xfree(remap);
                remap = NULL;
                repchar = DFLT_REPCHAR;
            }
        }
        else if (!strcmp(tok, "repchar")) {
            rp = token(rp, tok, NLINE);
            if (tok[0] == '\0') break;
            int ci;
            if (tok[0] == 'U' && tok[1] == '+')
                ci = 2;
            else
                ci = 0;
            int newval = strtol(tok+ci, NULL, 16);
            if (newval > 0 && newval <= 0x0010FFFF) repchar = newval;
        }
        else {  /* Assume a char to map (possibly a range) */
            char *tp = tok;
            if (*tp == 'U' && *(tp+1) == '+') tp += 2;
            unsigned int lowval = strtoul(tp, &tp, 16);
            if (lowval & 0x80000000) continue;  /* Ignore any such... */
            unsigned int topval;
            if (*tp == '-') {               /* We have a range */
                tp++;                       /* Skip over the - */
                if (*tp == 'U' && *(tp+1) == '+') tp += 2;
                topval = strtol(tp, NULL, 16);
                if (topval <= 0) continue;  /* Ignore this... */
            }
            else topval = lowval;
            if (lowval > topval || lowval > 0x0010FFFF || topval > 0x0010FFFF)
                continue;                   /* Error - just ignore */

/* Get the current size - we want one more, or two more if it's
 * currently empty
 */
            int need;
            if (remap == NULL) {
                need = 2;
            }
            else {
                need = 0;
                while (remap[need++].start != UEM_NOCHAR);
                need++;
            }
            remap = Xrealloc(remap, need*sizeof(struct repmap_t));
            if (need == 2)  {               /* Newly allocated */
                remap[0].start = lowval;
                remap[0].end = topval;
                remap[1].start = remap[1].end = UEM_NOCHAR;
                continue;                   /* All done... */
            }
/* Need to insert into list sorted */
            int ri = 0;
            while (remap[ri].start <= lowval) {
                ri++;
            }
            if (remap[ri].start == lowval) { /* Replacement */
                remap[ri].start = lowval;
                remap[ri].end = topval;
            }
            else {
                int lowtmp, toptmp;
                while(lowval != UEM_NOCHAR) {
                    lowtmp = remap[ri].start;
                    toptmp = remap[ri].end;
                    remap[ri].start = lowval;
                    remap[ri].end = topval;
                    lowval = lowtmp;
                    topval = toptmp;
                    ri++;
                }
                remap[ri].start = remap[ri].end = UEM_NOCHAR;
            }
        }
    }
    return TRUE;
}

/* Use the replacement char for mapped ones.
 * NOTE: that we might need two replacement chars!
 *  One for mapping chars that need to take up a column (normal)
 *  Another to replace zero-width chars (could map to 0 to "do nothing")
 * If so, add a second arg to indicate which is needed.
 */

unicode_t display_for(unicode_t uc) {

    if (uc > MAX_UTF8_CHAR) return repchar;
    if (remap == NULL) return uc;
    for (int rc = 0; remap[rc].start != UEM_NOCHAR; rc++) {
        if (uc < remap[rc].start) return uc;
        if (uc <= remap[rc].end)  return repchar;
    }
    return uc;
}

/* Common code for the two "length" functions.
 * If -ve maxxlen assume we have a NUL-terminated string.
 */
static unsigned int utf8_to_uclen(char *str, int count_graphemes,
     int maxlen) {
    unsigned int len = 0;
    int offs = 0;
    if (maxlen < 0) maxlen = strlen(str);
    while (maxlen > offs) {     /* Until we run out */
        len++;
        offs = next_utf8_offset(str, offs, maxlen, count_graphemes);
    }
    return len;
}

/* Get the number of unicode chars in a NUL-terminated utf8 string.
 * This is NOT the character/glyph count!!!
 */
unsigned int uclen_utf8(char *str) {
    return utf8_to_uclen(str, FALSE, -1);
}
/* Get the number of characters/glyphs corresponding to a
 * NUL-terminated utf8 string.
 * This ignores combining unicode characters (so counts full graphemes).
 */
unsigned int glyphcount_utf8(char *str) {
    return utf8_to_uclen(str, TRUE, -1);
}
/* Get the number of characters/glyphs corresponding to a
 * utf8 string og a given length
 * This ignores combining unicode characters (so counts full graphemes).
 */
unsigned int glyphcount_utf8_array(char *str, int maxlen) {
    return utf8_to_uclen(str, TRUE, maxlen);
}

/* Compare two utf8 buffers case-insensitively.
 * This assumes that the second buffer is already lowercased.
 */
int nocasecmp_utf8(char *tbuf, int t_start, int t_maxlen,
              char *lbuf, int l_start, int l_maxlen) {

    unicode_t tc, lc;
    while (t_start < t_maxlen && l_start < l_maxlen) {
        t_start += utf8_to_unicode(tbuf, t_start, t_maxlen, &tc);
        l_start += utf8_to_unicode(lbuf, l_start, l_maxlen, &lc);
        tc = utf8proc_tolower(tc);
        if (tc != lc) return ((tc>lc)-(tc<lc));
    }
    return 0;
}

/* Determine the offset within a buffer when you go back <n> unicode
 * chars.
 * Returns -1 if you try to go back beyond the start of the buffer.
 */
int unicode_back_utf8(int n_back, char *buf, int offset) {

    if (n_back <= 0) return offset;
    while (offset) {       /* Until we reach the BOL */
        offset = prev_utf8_offset(buf, offset, FALSE);
        if (--n_back <= 0) break;
    }
    if (n_back != 0) return -1;
    return offset;
}

/* ------------------------------------------------------------ */

/* Called multiple times with differing args */
static void add_to_res(struct mstr *mstr, char *buf, int nc, int incr) {

    if ((mstr->utf8c + nc) >= mstr->alloc) {    /* Allow for NUL we'll add */
        mstr->alloc += incr;
        mstr->str = Xrealloc(mstr->str, mstr->alloc);
    }
/* We have to be able to copy NULs */
    memcpy(mstr->str+mstr->utf8c, buf, nc);
    mstr->utf8c += nc;
    return;
}

/* We will be sent a utf8 string of defined length (or -1 to look for NUL).
 * Put the result into an alloc()ed buffer on the user-given variable.
 * It is the caller's responsibility to free() this.
 * This will *always* allocate a return string as it "cannot" fail.
 * If the "want" key is not recognized it is treated as UTF8_TITLE
 * (which is more likely to show up with unexpected results...)
 */
void utf8_recase(int want, char *in, int len, struct mstr *mstr) {
    utf8proc_int32_t (*caser)(utf8proc_int32_t);

/* Set the case-mapping handler we wish to use */

    switch (want) {
    case UTF8_UPPER:
        caser = utf8proc_toupper;
        break;
    case UTF8_LOWER:
        caser = utf8proc_tolower;
        break;
    case UTF8_TITLE:
    default:
        caser = utf8proc_totitle;
        break;
    }

/* Initialize some counters */

    mstr->utf8c = 0;        /* None there yet */
    mstr->uc = 0;           /* Count unicode chars as we go */
    mstr->grphc = -1;       /* Not known */

/* Have we been asked to determine the length? */

    if (len < 1) len = strlen(in);

/* Now we know the length, Quickly handle this perverse case. */

    if (len == 0) {
        mstr->str = Xmalloc(1);
        *(mstr->str) = '\0';
        mstr->alloc = 1;
        return;
    }

    mstr->str = NULL;       /* So Xrealloc() works at the start */
    mstr->alloc = 0;        /* Currently allocated */
    int rec_incr = len + 1; /* Allocate in steps of len+1...trailing NUL */

    int used;
    int offset = 0;
/* Was a for loop.
 * Now a do loop so the gcc analyzer knows we go through it at least
 * once (and so mstr allocates str in add_to_res().
 * It seems to think that len (which we know must be 1+) can start
 * at less than offset (which starts at 0).
 */
    do {
        unicode_t uc;
        used = utf8_to_unicode(in, offset, len, &uc);
        mstr->uc++;

/* There may be some special cases.
 * The German Sharp S (ß) uppercases to ẞ in libutf8proc, but that isn't
 * used in standard German - SS is. So lower->upper isn't reversible!
 * Other special cases may be needed...
 */
        if ((want == UTF8_UPPER) && (uc == 0x00df)) {
            add_to_res(mstr, "SS", 2, rec_incr);
            continue;
        }

/* If this isn't a base character or it doesn't change when
 * run through the case-mapper, just copy it verbatim.
 */
        unicode_t nuc;
        if (combining_type(uc) || ((nuc = caser(uc)) == uc)) {
            add_to_res(mstr, in+offset, used, rec_incr);
            continue;
        }

/* The case-mapper changed it, so copy the new unicode char to the
 * output as a utf8 sequence.
 */
        char tbuf[4];   /* Maxlen utf8 mapping */
        int newlen = unicode_to_utf8(nuc, tbuf);
        add_to_res(mstr, tbuf, newlen, rec_incr);
    } while ((offset += used) < len);
    *(mstr->str+mstr->utf8c) = '\0';    /* NUL terminate it */
    return;     /* Success */
}

/* The Julia utf8 library has a bug(?) for U+00a8 and U+00b4.
 * It marks them as zero-width (v2.3+) but they aren't
 * Also true for U+00B8, U+02C2-U+02C5, U+02D2-U+02DF, U+02E5-U+02EB,
 * U+02ED. Possibly others..
 */
int utf8char_width(unicode_t c) {
/* We expect to get mostly ASCII, with a known width of 1, so
 * handle that quickly.
 */
    if ((c >= 0x20) && (c < 0x7f)) return 1;
    switch(c) {
    case 0x00a8:
    case 0x00b4:
    case 0x00b8:
    case 0x02c2:
    case 0x02c3:
    case 0x02c4:
    case 0x02c5:
    case 0x02d2:
    case 0x02d3:
    case 0x02d4:
    case 0x02d5:
    case 0x02d6:
    case 0x02d7:
    case 0x02d8:
    case 0x02d9:
    case 0x02da:
    case 0x02db:
    case 0x02dc:
    case 0x02dd:
    case 0x02de:
    case 0x02df:
    case 0x02e5:
    case 0x02e6:
    case 0x02e7:
    case 0x02e8:
    case 0x02e9:
    case 0x02ea:
    case 0x02eb:
    case 0x02ed:
        return 1;
    }
    return utf8proc_charwidth(c);
}

/* GGR Some functions to handle struct grapheme usage */

/* Convert a grapheme to a dynamically-allocated byte-array (passed in)
 * May recase the base character
 * Returns the currently allocated size.
 */
static int grapheme_to_bytes(struct grapheme *gc, char **rp, int alen,
     int nocase) {
    char ub[6];
    int ulen;
    int reslen;

    if (nocase) ulen = unicode_to_utf8(utf8proc_toupper(gc->uc), ub);
    else        ulen = unicode_to_utf8(gc->uc, ub);

    if (ulen+1 > alen) {    /* Round up new allocation to multiple of 16 */
        alen = 16*(((ulen+1)/16)+1);
        *rp = Xrealloc(*rp, alen);
    }
    memcpy(*rp, ub, ulen);
    reslen = ulen;
    if (!gc->cdm) goto done;

    ulen = unicode_to_utf8(gc->cdm, ub);
    if (reslen+ulen+1 > alen) {
        *rp = Xrealloc(*rp, alen+16);
        alen += 16;
    }
    memcpy(*rp+reslen, ub, ulen);
    reslen += ulen;
    unicode_t *uc_ex = gc->ex;
    while (*uc_ex != UEM_NOCHAR) {
        ulen = unicode_to_utf8(*uc_ex++, ub);
        if (reslen+ulen+1 > alen) {
            *rp = Xrealloc(*rp, alen+16);
            alen += 16;
        }
        memcpy(*rp+reslen, ub, ulen);
        reslen += ulen;
    }
done:
    *(*rp+reslen) = '\0';
    return alen;
}
/* Only interested in same/not same here
 * Add some simple case testing via flags.
 * Assume only the base character is case-dependent, and that combining
 * marks must always be in the same order.
 */
struct gr_array {
    char *bytes;
    int alloc;
};

static struct gr_array gr1 = { NULL, 0 };
static struct gr_array gr2 = { NULL, 0 };

int same_grapheme(struct grapheme *gp1, struct grapheme *gp2, int flags) {
    if ((flags & USE_WPBMODE) && !(curwp->w_bufp->b_mode & MDEXACT)) {
        if (utf8proc_toupper(gp1->uc) != utf8proc_toupper(gp2->uc))
            goto check_equiv;
    }
    else {
        if (gp1->uc != gp2->uc) goto check_equiv;
    }
    if (gp1->cdm != gp2->cdm) return FALSE;
    if ((gp1->ex == NULL) && (gp2->ex == NULL)) return TRUE;
    if ((gp1->ex == NULL) && (gp2->ex != NULL)) goto check_equiv;
    if ((gp1->ex != NULL) && (gp2->ex == NULL)) goto check_equiv;
/* Have to compare the ex lists!... */
    unicode_t *ex1 = gp1->ex;
    unicode_t *ex2 = gp2->ex;
    while(1) {
        if (*ex1 != *ex2) goto check_equiv;
        if ((*ex1 == UEM_NOCHAR) || (*ex2 == UEM_NOCHAR)) break;
        ex1++; ex2++;
    }
    if ((*ex1 == UEM_NOCHAR) && (*ex2 == UEM_NOCHAR)) return TRUE;

/* If we fail anywhere above we also need to check if we've been asked
 * to do equivalence checking...which ONLY applies in Magic mode (and
 * hence only if the flags have asked us to check the buffer mode).
 * It also cannot make a difference if the base char is ASCII and there
 * is no combining character (which is likely to be by far the most common
 * case, and so worth checking).
 * The Unicode normalization info is in Standard Annex #15:
 *  https://unicode.org/reports/tr15
 */
check_equiv:
    if (!((flags & USE_WPBMODE) &&
          ((curwp->w_bufp->b_mode & MD_MAGEQV) == MD_MAGEQV))) {
        return FALSE;       /* Not asking for Magic+Equiv */
    }
    if (gp1->uc <= 0x7f && !gp1->cdm) return FALSE;
    if (gp2->uc <= 0x7f && !gp2->cdm) return FALSE;

/* We have to turn the graphemes into byte arrays, normalize them and
 * check the result.
 * Also need to allow for case-insensitivity on the first char.
 */
    int nocase = !(curwp->w_bufp->b_mode & MDEXACT);
    gr1.alloc = grapheme_to_bytes(gp1, &gr1.bytes, gr1.alloc, nocase);
    gr2.alloc = grapheme_to_bytes(gp2, &gr2.bytes, gr2.alloc, nocase);

/* Now convert these with the defined equiv_handler, compare then free
 * the temp strings.
 */

    utf8proc_uint8_t *nfkc1 = equiv_handler((utf8proc_uint8_t *)gr1.bytes);
    utf8proc_uint8_t *nfkc2 = equiv_handler((utf8proc_uint8_t *)gr2.bytes);
    int res = !strcmp((char *)nfkc1, (char *)nfkc2);
    Xfree(nfkc1);
    Xfree(nfkc2);
    return res;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_utf8(void) {
    if (remap) Xfree(remap);
    if (gr1.bytes) Xfree(gr1.bytes);
    if (gr2.bytes) Xfree(gr2.bytes);
    return;
}
#endif
