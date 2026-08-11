/* dyn_buf.h
 * Definitions to handle dynamic buffers
 */

#ifndef DYN_BUF_H_
#define DYN_BUF_H_

#include <stddef.h>

/* Define a Dynamic Buffer, and how to access its members
 * The flags are:
 *  DB_UPS      Allow the asp to walk alongthe string (tokenizer)
 *  DB_FXD      Do not allow any changes to the value
 */
#define DB_UPS 0x01
#define DB_FXD 0x02

typedef struct {
    char *buf;      /* The (NUL-terminated) string/buffer */
    char *asp;      /* The "actual start pointer" (may be beyond buf) */
    size_t alloc;   /* What we've allocated */
    int alen;       /* The actual length from asp (no trailing NUL) */
    int flags;      /* Set of flags */
} db;

/* We need to define these (globally, or statically in a file body)
 * So we also need to be able to declare the global ones in other
 * files.
 */
#define db_buf_initval { NULL, NULL, 0, 0, 0 }
#define db_bufdef(a) db (a) = db_buf_initval
#define db_dcl(a) db (a)
#define dbp_dcl(a) db (*a)

/* We need to access the entries for local/global entries (db_*)
 * and entries arriving as function parameters (dbp_*).
 */
#define db_buf(a)   ((const char *)(a).buf)
#define db_val(a)   ((const char *)(a).asp)
#define db_len(a)   ((const int)(a).alen)
#define db_max(a)   ((const size_t)(a).alloc)
#define db_type(a)  ((const int)(a).type)

#define dbp_buf(a)  ((const char *)((a)->buf))
#define dbp_val(a)  ((const char *)((a)->asp))
#define dbp_len(a)  ((const int)((a)->alen))
#define dbp_max(a)  ((const size_t)((a)->alloc))
#define dbp_type(a) ((const int)((a)->type))

/* The actual function calls (in dyn_str.c) to manipulate them.
 * Not expecting these to be called directly
 */

/* The buf/asp comment refers to which buffer pointer is used.
 * For the buf ones, the entire buffer is reinitialized, and the
 * asp field is set equal to the buf field.
 */
void _dbp_flagset(db *, int);
void _dbp_flagon(db *, int);
void _dbp_flagoff(db *, int);

const char *_dbp_val_nc(db *);                              /* asp */
void _dbp_setn(db *, const void *, int);                    /* buf */
void _dbp_set(db *, const char *);                          /* buf */
void _dbp_copy(db *, db *);                                 /* buf+asp */
void _dbp_replicatech_at(db *, char, int, int);             /* asp */
void _dbp_insertn_at(db *, const void *, int, int);         /* asp */
void _dbp_deleten_at(db *, int, int);                       /* asp */
void _dbp_overwriten_at(db *, const void *, int, int);      /* asp */
void _dbp_retailstr_at(db *, const char *, int);            /* asp */
void _dbp_bufset(db *, const char, int);                    /* buf */
void _dbp_clear(db *);                                      /* buf */
void _dbp_truncate(db *, int);                              /* asp */
void _dbp_uctruncate(db *, int);                            /* asp */
void _dbp_appendn(db *, const char *, int);                 /* asp */
void _dbp_append(db *, const char *);                       /* asp */
void _dbp_append_dbp(db *, db *);                           /* asp */
void _dbp_addch(db *, const char);                          /* asp */
char _dbp_charat(db *, int);                                /* asp */
void _dbp_setcharat(db *, int, char c);                     /* asp */
void _dbp_upval(db *, const char *);                        /* asp */
int _dbp_dbp_cmp(db *, db *);                               /* asp */
void _dbp_sprintf(db *ds, const char *fmt, ...);            /* buf */
void _dbp_free(db *);                                       /* buf */

/* Currently just simple defines */
#define _dbp_cmp(ds, str) strcmp((ds)->asp, str)
#define _dbp_cmpn(ds, str, n) strncmp((ds)->asp, str, (size_t)(n))
#define _dbp_casecmp(ds, str) strcasecmp((ds)->asp, str)
#define _dbp_casecmpn(ds, str, n) strncasecmp((ds)->asp, str, (size_t)(n))

#define _dbp_memchr(ds, c) memchr((ds)->asp, c, (size_t)(ds)->alen)

/* Defines for actual use in user code
 * The db_* calls are for "local" usage whereas
 * the dbp_* calls are for values arriving as function parameters.
 */
#define db_flagset(ds, f) _dbp_flagset(&(ds), f)
#define dbp_flagset(ds, f) _dbp_flagset(ds, f)
#define db_flagon(ds, f) _dbp_flagon(&(ds), f)
#define dbp_flagon(ds, f) _dbp_flagon(ds, f)
#define db_flagoff(ds, f) _dbp_flagoff(&(ds), f)
#define dbp_flagoff(ds, f) _dbp_flagoff(ds, f)

#define db_val_nc(val) _dbp_val_nc(&(val))
#define dbp_val_nc(val) _dbp_val_nc(val)

#define db_setn(to_ds, from_buf, flen) _dbp_setn(&(to_ds), from_buf, flen)
#define dbp_setn(to_ds, from_buf, flen) _dbp_setn((to_ds), from_buf, flen)

#define db_set(to_ds, from_str) _dbp_set(&(to_ds), from_str)
#define dbp_set(to_ds, from_str) _dbp_set((to_ds), from_str)

#define db_copy(to_ds, from_dsp) _dbp_copy(&(to_ds), from_dsp)
#define dbp_copy(to_ds, from_dsp) _dbp_copy((to_ds), from_dsp)

#define db_replicatech_at(to_ds, ch, flen, w) \
     _dbp_replicatech_at(&(to_ds), ch, flen, w)
#define dbp_replicatech_at(to_ds, ch, flen, w) \
     _dbp_replicatech_at((to_ds), ch, flen, w)

#define db_insertn_at(to_ds, from_buf, flen, w) \
     _dbp_insertn_at(&(to_ds), from_buf, flen, w)
#define dbp_insertn_at(to_ds, from_buf, flen, w) \
     _dbp_insertn_at((to_ds), from_buf, flen, w)

#define db_deleten_at(to_ds, n, w) _dbp_deleten_at(&(to_ds), n, w)
#define dbp_deleten_at(to_ds, n, w) _dbp_deleten_at((to_ds), n, w)

#define db_overwriten_at(to_ds, from_buf, flen, w) \
     _dbp_overwriten_at(&(to_ds), from_buf, flen, w)
#define dbp_overwriten_at(to_ds, from_buf, flen, w) \
     _dbp_overwriten_at((to_ds), from_buf, flen, w)

#define db_retailstr_at(to_ds, ntail, nlen) \
     _dbp_retailstr_at(&(to_ds), ntail, nlen)
#define dbp_retailstr_at(to_ds, ntail, nlen) \
     _dbp_retailstr_at((to_ds), ntail, nlen)

#define db_clear(ds) _dbp_clear(&(ds))
#define dbp_clear(ds) _dbp_clear((ds))

#define db_truncate(ds, n) _dbp_truncate(&(ds), n)
#define dbp_truncate(ds, n) _dbp_truncate((ds), n)

#define db_uctruncate(ds, n) _dbp_uctruncate(&(ds), n)
#define dbp_uctruncate(ds, n) _dbp_uctruncate((ds), n)

#define db_appendn(to_ds, add, applen) _dbp_appendn(&(to_ds), add, applen)
#define dbp_appendn(to_ds, add, applen) _dbp_appendn((to_ds), add, applen)

#define db_append(to_ds, add) _dbp_append(&(to_ds), add)
#define dbp_append(to_ds, add) _dbp_append((to_ds), add)

#define db_append_dbp(to_ds, from_dsp) _dbp_append_dbp(&(to_ds), from_dsp)
#define dbp_append_dbp(to_ds, from_dsp) _dbp_append_dbp((to_ds), from_dsp)

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

#define db_db_cmp(ds, ods) _dbp_dbp_cmp(&(ds), &(ods))
#define dbp_db_cmp(ds, ods) _dbp_dbp_cmp(ds, &(ods))
#define db_dbp_cmp(ds, ods) _dbp_dbp_cmp(&(ds), ods)
#define dbp_dbp_cmp(ds, ods) _dbp_dbp_cmp(ds, ods)

#define db_memchr(ds, c) _dbp_memchr(&(ds), c)
#define dbp_memchr(ds, c) _dbp_memchr((ds), c)

#define db_sprintf(ds, ...) _dbp_sprintf(&(ds), __VA_ARGS__)
#define dbp_sprintf(ds, ...) _dbp_sprintf((ds), __VA_ARGS__)

#define db_free(ds) _dbp_free(&(ds))
#define dbp_free(ds) _dbp_free((ds))

#define db_upval(ds, charp) _dbp_upval(&(ds), charp)
#define dbp_upval(ds, charp) _dbp_upval((ds), charp)

#define db_bufset(ds, ch, n) _dbp_bufset(&(ds), ch, n)
#define dbp_bufset(ds, ch, n) _dbp_bufset((ds), ch, n)

#endif
