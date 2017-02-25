#include "utf8.h"

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
 */
unsigned utf8_to_unicode(char *line, unsigned index, unsigned len, unicode_t *res)
{
        unsigned value;
        unsigned char c = line[index];
        unsigned bytes, mask, i;

        *res = c;
        line += index;
        len -= index;

        /*
         * 0xxxxxxx is valid utf8
         * 10xxxxxx is invalid UTF-8, we assume it is Latin1
         */
        if (c < 0xc0)
                return 1;

        /* Ok, it's 11xxxxxx, do a stupid decode */
        mask = 0x20;
        bytes = 2;
        while (c & mask) {
                bytes++;
                mask >>= 1;
        }

        /* Invalid? Do it as a single byte Latin1 */
        if (bytes > MAX_UTF8_LEN)
                return 1;
        if (bytes > len)
                return 1;

        value = c & (mask-1);

        /* Ok, do the bytes */
        for (i = 1; i < bytes; i++) {
                c = line[i];
                if ((c & 0xc0) != 0x80)
                        return 1;
                value = (value << 6) | (c & 0x3f);
        }
        *res = value;
        return bytes;
}

static void reverse_string(char *begin, char *end)
{
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
unsigned unicode_to_utf8(unsigned int c, char *utf8)
{
        int bytes = 1;

        *utf8 = c;
        if (c > 0x7f) {
                int prefix = 0x40;
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
 */
int next_utf8_offset(char *buf, int offset, int max_offset) {

        unicode_t c;
        
/* Just use utf8_to_unicode */

        int incr = utf8_to_unicode(buf, offset, max_offset, &c);
        return offset + incr;
}

int prev_utf8_offset(char *buf, int offset, int max_offset) {
    
/* Step back a byte at a time.
 * If the first byte isn't a utf8 continuation byte (10xxxxxx0 that is it.
 * It it *is* a continuation byte look back another byte, up to 3
 * times. If we then find a utf8 leading byte (that is a correct one for
 * the length we have found) we use that.
 * If we fail along the way we revert to the original "back one byte".
 */

        if (offset-- < 0) return 0;
        
        char c = buf[offset];
        if ((c & 0xc0) == 0x80) {       /* Ext byte? */
                int trypos = offset;
                int tryb = MAX_UTF8_LEN;
                int marker = 0xc0;
                int valmask = 0x1f;
                while ((--trypos >= 0) && (--tryb >= 0)) {
                        c = buf[trypos];
                        if ((c & 0xc0) == 0x80) {   /* Ext byte */
                                marker >>= 1;       /* Shift right..*/
                                marker |= 0x80;     /* ..set top bit */
                                valmask >>= 1;      /* Fewer... */
                                continue;
                        }
                        if ((c & ~valmask) == marker) { /* Start */
                                offset = trypos;
                        }
                        break;          /* By default, now done */
                }
        }
        return offset;
}


