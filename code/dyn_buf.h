/* dyn_buf.h
 * Definitions to handle dynamic buffers
 */

#ifndef DYN_BUF_H_
#define DYN_BUF_H_

/* Define a Dynamic Buffer, and how to access its members
 * The enum values are specifically set, as it reflects the additional
 * buffer size required beyond the valid stored bytes.
 */
enum db_type { BUF = 0, STR = 1 };
typedef struct {
    char *val;      /* The (NUL-terminated) string */
    size_t len;     /* WITHOUT the NUL */
    size_t alloc;   /* What we've allocated */
    enum db_type type;
} db;

/* We need to define these (globally, or statically in a file body)
 * So we also need to be able to declare the global ones in other
 * files.
 */
#define db_buf_initval { NULL, 0, 0, BUF }
#define db_str_initval { NULL, 0, 0, STR }
#define db_bufdef(a) db a = db_buf_initval
#define db_strdef(a) db a = db_str_initval
#define db_dcl(a) db a

/* We need to access the entries for local/global entries (db_*)
 * and entries arriving as function parameters (dbp_*).
 */
#define db_val(a)   ((const char *)(a).val)
#define db_len(a)   ((const size_t)(a).len)
#define db_max(a)   ((const size_t)(a).alloc)
#define db_type(a)  ((const enum db_type)(a).type)
#define dbp_val(a)  ((const char *)(a)->val)
#define dbp_len(a)  ((const size_t)(a)->len)
#define dbp_max(a)  ((const size_t)(a)->alloc)
#define dbp_type(a) ((const enum db_type)(a)->type)

/* The actual function calls (in dyn_str.c) to manipulate them.
 * Not expecting these to be called directly
 */

const char *_dbp_val_nc(db *);
void _dbp_setn(db *, const void *, size_t);
void _dbp_set(db *, const char *);
void _dbp_insertn_at(db *, const void *, size_t, size_t);
void _dbp_deleten_at(db *, int, size_t);
void _dbp_overwriten_at(db *, const void *, size_t, size_t);
void _dbp_clear(db *);
void _dbp_truncate(db *, size_t);
void _dbp_appendn(db *, const char *, size_t);
void _dbp_append(db *, const char *);
void _dbp_addch(db *, const char);
char _dbp_charat(db *, size_t);
int  _dbp_setcharat(db *, size_t, char c);
char _dbp_cmp(db *, const char *);
char _dbp_cmpn(db *, const char *, size_t);
char _dbp_casecmp(db *, const char *);
char _dbp_casecmpn(db *, const char *, size_t);

void _dbp_sprintf(db *ds, const char *fmt, ...);

void _dbp_free(db *);


/* Defines for actual use in user code
 * The db_* calls are for "local" usage whereas
 * the dbp_* calls are for values arriving as function parameters.
 */
#define db_val_nc(val) _dbp_val_nc(&(val))
#define dbp_val_nc(val) _dbp_val_nc(val)

#define db_setn(to_ds, from_buf, flen) _dbp_setn(&(to_ds), from_buf, flen)
#define dbp_setn(to_ds, from_buf, flen) _dbp_setn((to_ds), from_buf, flen)

#define db_set(to_ds, from_str) _dbp_set(&(to_ds), from_str)
#define dbp_set(to_ds, from_str) _dbp_set((to_ds), from_str)

#define db_insertn_at(to_ds, from_buf, flen, w) \
     _dbp_insertn_at(&(to_ds), from_buf, flen, w)
#define dbp_insertn_at(to_ds, from_buf, flen, w) \
     _dbp_insertn_at((to_ds), from_buf, flen, w)

#define db_deleten_at(to_ds, n, w) _dbp_deleten_at(&(to_ds), n, w);
#define dbp_deleten_at(to_ds, n, w) _dbp_deleten_at((to_ds), n, w);

#define db_overwriten_at(to_ds, from_buf, flen, w) \
     _dbp_overwriten_at(&(to_ds), from_buf, flen, w)
#define dbp_overwriten_at(to_ds, from_buf, flen, w) \
     _dbp_overwriten_at((to_ds), from_buf, flen, w)

#define db_clear(ds) _dbp_clear(&(ds))
#define dbp_clear(ds) _dbp_clear((ds))

#define db_truncate(ds, n) _dbp_truncate(&(ds), n)
#define dbp_truncate(ds, n) _dbp_truncate((ds), n)

#define db_appendn(to_ds, add, alen) _dbp_appendn(&(to_ds), add, alen)
#define dbp_appendn(to_ds, add, alen) _dbp_appendn((to_ds), add, alen)

#define db_append(to_ds, add) _dbp_append(&(to_ds), add)
#define dbp_append(to_ds, add) _dbp_append((to_ds), add)

#define db_addch(to_ds, c) _dbp_addch(&(to_ds), c)
#define dbp_addch(to_ds, c) _dbp_addch((to_ds), c)

#define db_charat(to_ds, w) _dbp_charat(&(to_ds), w)
#define dbp_charat(to_ds, w) _dbp_charat((to_ds), w)

#define db_setcharat(to_ds, w, c) _dbp_setcharat(&(to_ds), w, c)
#define dbp_setcharat(to_ds, w, c) _dbp_setcharat((to_ds), w, c)

#define db_cmp(ds, s) _dbp_cmp(&(ds), s)
#define dbp_cmp(ds, s) _dbp_cmp((ds), s)

#define db_cmpn(ds, s, n) _dbp_cmpn(&(ds), s, n)
#define dbp_cmpn(ds, s, n) _dbp_cmpn((ds), s, n)

#define db_casecmp(ds, s) _dbp_casecmp(&(ds), s)
#define dbp_casecmp(ds, s) _dbp_casecmp((ds), s)

#define db_casecmpn(ds, s, n) _dbp_casecmpn(&(ds), s, n)
#define dbp_casecmpn(ds, s, n) _dbp_casecmpn((ds), s, n)

#define db_sprintf(ds, ...) _dbp_sprintf(&(ds), __VA_ARGS__)
#define dbp_sprintf(ds, ...) _dbp_sprintf((ds), __VA_ARGS__)

#define db_free(ds) _dbp_free(&(ds))
#define dbp_free(ds) _dbp_free((ds))

#endif
