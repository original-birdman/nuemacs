#include <stdio.h>
#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

#include "utf8proc.h"

/*
 * utf8_to_unicode()
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
        *res = 0;
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

/* Ok, it's 11xxxxxx, do a stupid decode */
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

/* Ok, do the bytes */
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

static void reverse_string(char *begin, char *end) {
    do {
        char a = *begin, b = *end;
        *end = a; *begin = b;
        begin++; end--;
    } while (begin < end);
}

/*
 * unicode_to_utf8()
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
        unsigned int prefix = 0x40;
        char *p = utf8;
        do {
            *p++ = 0x80 + (c & 0x3f);
            bytes++;
            prefix >>= 1;
            c >>= 6;
        } while (c > prefix);
        *p = c - 2*prefix;
        reverse_string(utf8, p);
    }
    return bytes;
}

/* GGR functions to get offset of previous/next character in a buffer.
 * Added here to keep utf8 character handling together
 * Optional (grapheme_start):
 *    check the following character(s) for being zero-width too.
 */
int
 next_utf8_offset(char *buf, int offset, int max_offset, int grapheme_start) {

    unicode_t c;

/* Just use utf8_to_unicode */

    int offs = offset;
    offs += utf8_to_unicode(buf, offs, max_offset, &c);
    if (grapheme_start) {
        while(1) {      /* Look for any attached zero-width modifiers */
            int next_incr = utf8_to_unicode(buf, offs, max_offset, &c);
            if (!zerowidth_type(c)) break;
            offs += next_incr;
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
 *    if we find a zero-width character we go back to the next previous one.
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
 * extention in the test.
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
    } while(grapheme_start && (offs >= 0) && zerowidth_type(res));
    return offs;
}

/* Check whether the character is a zero-width one - needed for
 * grapheme handling in display.c
 */

static struct range_t {
    unicode_t start;
    unicode_t end;
    int       type;     /* Possibly not needed... */
} const zero_width[] = { /* THIS LIST MUST BE IN SORTED ORDER!! */
    {0x02B0, 0x02FF, SPMOD_L},  /* Spacing Modifier Letters */
    {0x0300, 0x036F, COM_DIA},  /* Combining Diacritical Marks */
    {0x1AB0, 0x1AFF, COM_DIA},  /* Combining Diacritical Marks Extended */
    {0x1DC0, 0x1DFF, COM_DIA},  /* Combining Diacritical Marks Supplement */
    {0x200B, 0x200D, ZW_JOIN},  /* Zero-width space, non-joiner, joiner */
    {0x200E, 0x200F, DIR_MRK},  /* L2R, R2L mark */
    {0x202A, 0x202E, DIR_MRK},  /* More L2R, R2L marks */
    {0X2060, 0X206F, ZW_JOIN},  /* Other ZW joiners */
    {0x20D0, 0x20FF, COM_DIA},  /* Combining Diacritical Marks for Symbols */
    {0xFE20, 0xFE2F, COM_DIA},  /* Combining Half Marks */
    {UEM_NOCHAR, UEM_NOCHAR, 0} /* End of list marker */
};
static int spmod_l_is_zw = 0;   /* Probably not?? */
int zerowidth_type(unicode_t uc) {
    for (int rc = 0; zero_width[rc].start != UEM_NOCHAR; rc++) {
        if (uc < zero_width[rc].start) return 0;
        if (uc <= zero_width[rc].end) {
            if ((zero_width[rc].type == SPMOD_L) && !spmod_l_is_zw)
                return 0;
            return zero_width[rc].type;
        }
    }
    return 0;
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

    status = mlreplyall(
         "reset | repchar [U+]xxxx | [U+]xxxx[-[U+]xxxx] ", buf, NLINE - 1);
    if (status != TRUE)         /* Only act on +ve response */
        return status;

    char *rp = buf;
    char tok[NLINE];
    while(*rp != '\0') {
        rp = token(rp, tok, NLINE);
        if (tok[0] == '\0') break;
        if (!strcmp(tok, "reset")) {
            if (remap != NULL) {
                free(remap);
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
            if (lowval && 0x80000000) continue; /* Ignore any such... */
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

/* Get the number of unicode chars in a NUL-terminated utf8 string.
 */
unsigned int uclen_utf8(char *str) {

    unsigned int len = 0;
    int offs = 0;
    while (*(str+offs)) {       /* Until we reach the NUL */
        len++;
        offs = next_utf8_offset(str, offs, offs+10, FALSE);
    }
    return len;
}

/* Compare two utf8 buffers case-insensitvely.
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
 */
int utf8_recase(int want, char *in, int len, struct mstr *mstr) {
    utf8proc_int32_t (*caser)(utf8proc_int32_t);

/* Quickly handle this preverse case. */

    mstr->alloc = 0;        /* Currently allocated */
    mstr->utf8c = 0;        /* None there yet */
    mstr->uc = 0;           /* Count unicode chars as we go */
    mstr->grphc = -1;       /* Not known */

/* Set the case-mapping handler we wish to use */

    switch (want) {
    case UTF8_UPPER:
        caser = utf8proc_toupper;
        break;
    case UTF8_LOWER:
        caser = utf8proc_tolower;
        break;
    case UTF8_TITLE:
        caser = utf8proc_totitle;
        break;
    default:
        return -1;
    }

/* We only check this now, once we know the real length... */

    if (len < 1) len = strlen(in);
    if (len == 0) {
        mstr->str = Xmalloc(1);
        *(mstr->str) = '\0';
        mstr->alloc = 1;
        return 0;
    }

    mstr->str = NULL;       /* So Xrealloc() works at the start */
    int rec_incr = len + 1; /* Allocate in steps of len+1...trailing NUL */

    int used;
    for (int offset = 0; offset < len; offset += used) {
        unicode_t uc;
        used = utf8_to_unicode(in, offset, len, &uc);
        mstr->uc++;

/* There may be some special cases.
 *  The German Sharp S (ß) uppercases to ẞ in libutf8proc, but it isn't
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
        if (zerowidth_type(uc) || ((nuc = caser(uc)) == uc)) {
            add_to_res(mstr, in+offset, used, rec_incr);
            continue;
        }

/* The case-mapper changed it, so copy the new unicode char to the
 * output as a utf8 sequence.
 */
        char tbuf[4];   /* Maxlen utf8 mapping */
        int newlen = unicode_to_utf8(nuc, tbuf);
        add_to_res(mstr, tbuf, newlen, rec_incr);
    }
    *(mstr->str+mstr->utf8c) = '\0';    /* NUL terminate it */
    return 0;   /* Success */
}
