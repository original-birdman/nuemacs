#ifndef UTF8_H
#define UTF8_H

/* The maximum number of bytes in a utf8 sequence (since 2003) */
#define MAX_UTF8_LEN  4
#define MAX_UTF8_CHAR 0x0010FFFF
typedef unsigned int unicode_t;

unsigned utf8_to_unicode(char *line, unsigned index, unsigned len, unicode_t *res);
unsigned unicode_to_utf8(unsigned int c, char *utf8);

/* GGR
 * Define a structure to hold the data for a single grapheme.
 * This may include additonal unicode characters such as combining
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

int next_utf8_offset(char *, int, int, int);
int prev_utf8_offset(char *, int, int);

#define COM_DIA 1
#define ZW_JOIN 2
#define SPMOD_L 3
#define DIR_MRK 4
int zerowidth_type(unicode_t);
int char_replace(int, int);
unicode_t display_for(unicode_t);

char *tolower_utf8(char *, int, int *, int *);
unsigned int uclen_utf8(char *);
int nocasecmp_utf8(char *, int, int, char *, int, int);
int unicode_back_utf8(int, char *, int);

#endif
