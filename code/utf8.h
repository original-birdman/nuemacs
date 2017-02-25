#ifndef UTF8_H
#define UTF8_H

/* The maximum number of bytes in a utf8 sequence (since 2003) */
#define MAX_UTF8_LEN 4
typedef unsigned int unicode_t;

unsigned utf8_to_unicode(char *line, unsigned index, unsigned len, unicode_t *res);
unsigned unicode_to_utf8(unsigned int c, char *utf8);

int next_utf8_offset(char *, int, int);
int prev_utf8_offset(char *, int, int);

#endif
