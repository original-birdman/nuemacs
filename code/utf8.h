#ifndef UTF8_H
#define UTF8_H

/* Let's define an 8-byte integer type for use as a string/buffer/region
 * length. Now also for macro variable integers.
 * long may be 4- or 8-bytes
 * off_t should be 8-bytes, but there is no 8-byte specific printf
 * formatter, so we might as well define our own and use %ll.
 * long long is 8-bytes on 64-bit Kubuntu and 32-bit mips (OpenVix)
 *
 * This is here, as this file is included everywhere that matters.
 */
typedef long long ue64I_t;

/* The maximum number of bytes in a utf8 sequence (since 2003) */
#define MAX_UTF8_LEN  4
#define MAX_UTF8_CHAR 0x0010FFFF
typedef unsigned int unicode_t;

unsigned utf8_to_unicode(const char *line, unsigned index, unsigned len,
     unicode_t *res);
unsigned unicode_to_utf8(unsigned int c, char *utf8);

/* GGR
 * Define a structure to hold the data for a single grapheme.
 * This may include additional unicode characters such as combining
 * diacritics, or any zero-width character.
 * We expect most characters to have no cdm, and most that do have
 * them to only have one. So we access the first directly (as alignment
 * mean we'll probably get that space "free").
 */
struct grapheme {
    unicode_t  uc;          /* The "main" unicode character */
    unicode_t cdm;          /* A "combining diacritic marker" */
    unicode_t *ex;          /* A possible list of further cdm */
};

/* Flags for same_grapheme */
#define USE_WPBMODE 0x00000001

/* Flags and structures for utf8_recase */

#define UTF8_CKEEP 0    /* So we can init a var to 0 and do nothing */
#define UTF8_UPPER 1
#define UTF8_LOWER 2
#define UTF8_TITLE 3

/* A string with various lengths attached. -1 if unknown */
struct mstr {
    char *str;          /* May be NULL */
    int alloc;          /* Allocated size of str */
    int utf8c;          /* Bytes - excluding trailing NUL */
    int uc;             /* unicode points */
    int grphc;          /* grapheme count */
};

/* Externally visible calls */

#define uclen_utf8(str) utf8_to_uclen(str, FALSE, -1)
#define glyphcount_utf8(str) utf8_to_uclen(str, TRUE, -1)
#define glyphcount_utf8_array(str, max) utf8_to_uclen(str, TRUE, max)

#ifndef UTF8_C

int next_utf8_offset(const char *, int, int, int);
int prev_utf8_offset(const char *, int, int);

int build_next_grapheme(const char *, int, int, struct grapheme *, int);
int build_prev_grapheme(const char *, int, int, struct grapheme *, int);

int same_grapheme(struct grapheme *, struct grapheme *, int);

int combining_type(unicode_t);
int char_replace(int, int);
unicode_t display_for(unicode_t);

int utf8_to_uclen(const char *, int, int);

int nocasecmp_utf8(const char *, int, int, const char *, int, int);
int unicode_back_utf8(int, const char *, int);
int utf8char_width(unicode_t c);

void utf8_recase(int , const char *, int, struct mstr*);

#endif

#endif
