/* Definitions used by idxsorter.c */

struct fields {
    int offset;         /* byte offset from start of record */
    int len;            /* field length in chars, or bytes */
    char type;          /* C, S, U, P or I */
};

/* The function call itself */

int idxsort_fields(unsigned char *, int[], int , int , int , struct fields *);
void make_next_idx(int *, int *, int);
