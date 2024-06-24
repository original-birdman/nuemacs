#ifndef LINE_H_
#define LINE_H_

#include "utf8.h"
#include "dyn_buf.h"

#define BLOCK_SIZE 16 /* Line block chunk size. */

/*
 * All text is kept in circularly linked lists of "struct line" structures.
 * These begin at the header line (which is the blank line beyond the
 * end of the buffer).
 * This line is pointed to by the "struct buffer". Each line contains a the
 * number of bytes in the line (the "used" size), the size of the text array,
 * and the text. The end of line is not stored as a byte; it's implied.
 * It should also be possible for a line to end with zero-width space
 * marker!!!
 * Future additions will include update hints, and a list of marks
 * into the line.
 */
struct line {
    struct line *l_fp;      /* Link to the next line        */
    struct line *l_bp;      /* Link to the previous line    */
    db_dcl(l_);             /* Chars in dynamic buffer      */
};

#define lforw(lp)       ((lp)->l_fp)
#define lback(lp)       ((lp)->l_bp)

/* lp will always be a pointer, but l_ is not */

#define lgetc(lp, n)    (db_charat((lp->l_), n))
#define lputc(lp, n, c) (db_setcharat((lp->l_), n, c))
#define lused(lp)       (db_len(lp->l_))
#define lsize(lp)       (db_max(lp->l_))
#define ltext(lp)       (db_val(lp->l_))
#define ldb(lp)         (lp->l_)

/* Externally visible calls */

#ifndef LINE_C

extern struct line *lalloc(void);           /* Allocate a line. */
extern void lfree(struct line *lp);
extern void lchange(int flag);
extern int insspace(int f, int n);
extern int linsert_byte(int, unsigned char);
extern int linstr(const char *instr);
extern int linsert_uc(int n, unicode_t c);
extern int lover(const char *ostr);
extern int lnewline(void);
extern int ldelete(ue64I_t n, int kflag);
extern int ldelgrapheme(ue64I_t n, int kflag);
extern int lgetgrapheme(struct grapheme *, int);
extern int lputgrapheme(struct grapheme *gp);
extern const char *getctext(void);

extern void kdelete(void);
extern void addto_lastmb_ring(const char *);
extern int kinsert(int c);
extern int yank(int f, int n);
extern int yank_replace(int, int);
extern int yankmb(int f, int n);

#endif

/* A macro to determine the effect on the "display column" of adding a
 * given character.
 * Used by offset_for_curgoal(basic.c), setccol/getccol(random.c)
 * and updpos(display.c).
 * These need to have a common view of this.
 * NOTE that we can't just rely on utf8proc_charwidth() here as it gives a
 * zero width for, e.g., control chars but we need to use 2 for them.
 */
#define update_screenpos_for_char(scol, uc) \
    if (uc == '\t') { scol |= tabmask; scol++; }    /* Round up */  \
    else if (uc < 0x20 || uc == 0x7f)  scol += 2;   /* ^X */        \
    else if (uc >= 0x80 && uc <= 0xa0) scol += 3;   /* \nn */       \
    else if ((scol == 0) && combining_type(uc)) scol = 1;           \
    else scol += utf8char_width(uc);                /* Allow my overrides */

#endif  /* LINE_H_ */
