/* Definitions used by idxsorter.c */

struct fields {
    int offset;         /* byte offset from start of record */
    char type;          /* C, S, U, P or I */
    short len;          /* field length in chars, or bytes */
};

/* The function call itself */

int idxsort_fields(unsigned char *, int[], int , int , int , struct fields *);
