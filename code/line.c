/*      line.c
 *
 * The functions in this file are a general set of line management utilities.
 * They are the only routines that touch the text. They also touch the buffer
 * and window structures, to make sure that the necessary updating gets done.
 * There are routines in this file that handle the kill buffer too. It isn't
 * here for any good reason.
 *
 * Note that this code only updates the dot and mark values in the window list.
 * Since all the code acts on the current window, the buffer that we are
 * editing must be being displayed, which means that "b_nwnd" is non zero,
 * which means that the dot and mark values in the buffer headers are nonsense.
 *
 */

#include "line.h"

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

#define BLOCK_SIZE 16 /* Line block chunk size. */

static int force_newline = 0;   /* lnewline may need to be told this */

/*
 * This routine allocates a block of memory large enough to hold a struct line
 * containing "used" characters. The block is always rounded up a bit. Return
 * a pointer to the new block, or NULL if there isn't any memory left. Print a
 * message in the message line if no space.
 */
struct line *lalloc(int used) {
    struct line *lp;
    int size;

    size = (used + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
    if (size == 0)              /* Assume that is an empty. */
        size = BLOCK_SIZE;  /* Line is for type-in. */
    if ((lp = (struct line *)malloc(sizeof(struct line) + size)) == NULL) {
        mlwrite(MLpre "OUT OF MEMORY" MLpost);
        return NULL;
    }
    lp->l_size = size;
    lp->l_used = used;
    return lp;
}

/*
 * Delete line "lp". Fix all of the links that might point at it (they are
 * moved to offset 0 of the next line. Unlink the line from whatever buffer it
 * might be in. Release the memory. The buffers are updated too; the magic
 * conditions described in the above comments don't hold here.
 */
void lfree(struct line *lp) {
    struct buffer *bp;
    struct window *wp;

    wp = wheadp;
    while (wp != NULL) {
        if (wp->w_linep == lp) wp->w_linep = lp->l_fp;
        if (wp->w_dotp == lp) {
            wp->w_dotp = lp->l_fp;
            wp->w_doto = 0;
        }
        if (wp->w_markp == lp) {
            wp->w_markp = lp->l_fp;
            wp->w_marko = 0;
        }
        wp = wp->w_wndp;
    }
    bp = bheadp;
    while (bp != NULL) {
        if (bp->b_nwnd == 0) {
            if (bp->b_dotp == lp) {
                bp->b_dotp = lp->l_fp;
                bp->b_doto = 0;
            }
            if (bp->b_markp == lp) {
                bp->b_markp = lp->l_fp;
                bp->b_marko = 0;
            }
        }
        bp = bp->b_bufp;
    }
    lp->l_bp->l_fp = lp->l_fp;
    lp->l_fp->l_bp = lp->l_bp;
    free((char *)lp);
}

/*
 * This routine gets called when a character is changed in place in the current
 * buffer. It updates all of the required flags in the buffer and window
 * system. The flag used is passed as an argument; if the buffer is being
 * displayed in more than 1 window we change EDIT to HARD. Set MODE if the
 * mode line needs to be updated (the "*" has to be set).
 */
void lchange(int flag) {
    struct window *wp;

    if (curbp->b_nwnd != 1)             /* Ensure hard.         */
        flag = WFHARD;
    if ((curbp->b_flag & BFCHG) == 0) { /* First change, so     */
        flag |= WFMODE;             /* update mode lines.   */
        curbp->b_flag |= BFCHG;
    }
    wp = wheadp;
    while (wp != NULL) {
        if (wp->w_bufp == curbp) wp->w_flag |= flag;
        wp = wp->w_wndp;
    }

/* If this is a translation table, remove any compiled data */

    if ((curbp->b_type == BTPHON) && curbp->ptt_headp) ptt_free(curbp);
}

/*
 * insert spaces forward into text
 *
 * int f, n;            default flag and numeric argument
 */
int insspace(int f, int n) {
    UNUSED(f);
    linsert_byte(n, ' ');
    back_grapheme(n);
    return TRUE;
}

/*
 * Insert "n" copies of the character "c" at the current location of dot. In
 * the easy case all that happens is the text is stored in the line. In the
 * hard case, the line has to be reallocated. When the window list is updated,
 * take special care; I screwed it up once. You always update dot in the
 * current window. You update mark, and a dot in another window, if it is
 * greater than the place where you did the insert. Return TRUE if all is
 * well, and FALSE on errors.
 */

int linsert_byte(int n, unsigned char c) {
    char *cp1;
    char *cp2;
    struct line *lp1;
    struct line *lp2;
    struct line *lp3;
    int doto;
    int i;
    struct window *wp;

    if (curbp->b_mode & MDVIEW) /* don't allow this command if */
        return rdonly();        /* we are in read only mode    */
    lchange(WFEDIT);
    lp1 = curwp->w_dotp;        /* Current line         */
    if (lp1 == curbp->b_linep) {/* At the end: special  */
        if (curwp->w_doto != 0) {
            mlwrite("bug: linsert");
            return FALSE;
        }
        if ((lp2 = lalloc(n)) == NULL)  /* Allocate new line   */
            return FALSE;
        lp3 = lp1->l_bp;        /* Previous line        */
        lp3->l_fp = lp2;        /* Link in              */
        lp2->l_fp = lp1;
        lp1->l_bp = lp2;
        lp2->l_bp = lp3;
        for (i = 0; i < n; ++i) lp2->l_text[i] = c;
        curwp->w_dotp = lp2;
        curwp->w_doto = n;
        return TRUE;
    }
    doto = curwp->w_doto;                   /* Save for later.      */
    if (lp1->l_used + n > lp1->l_size) {    /* Hard: reallocate     */
        if ((lp2 = lalloc(lp1->l_used + n)) == NULL) return FALSE;
        cp1 = &lp1->l_text[0];
        cp2 = &lp2->l_text[0];
        while (cp1 != &lp1->l_text[doto]) *cp2++ = *cp1++;
        cp2 += n;
        while (cp1 != &lp1->l_text[lp1->l_used]) *cp2++ = *cp1++;
        lp1->l_bp->l_fp = lp2;
        lp2->l_fp = lp1->l_fp;
        lp1->l_fp->l_bp = lp2;
        lp2->l_bp = lp1->l_bp;
        free((char *)lp1);
    }
    else {                          /* Easy: in place       */
        lp2 = lp1;                  /* Pretend new line     */
        lp2->l_used += n;
        cp2 = &lp1->l_text[lp1->l_used];
        cp1 = cp2 - n;
        while (cp1 != &lp1->l_text[doto]) *--cp2 = *--cp1;
    }
    for (i = 0; i < n; ++i)         /* Add the characters   */
        lp2->l_text[doto + i] = c;
    wp = wheadp;                    /* Update windows       */
    while (wp != NULL) {
        if (wp->w_linep == lp1) wp->w_linep = lp2;
        if (wp->w_dotp == lp1) {
            wp->w_dotp = lp2;
            if (wp == curwp || wp->w_doto > doto) wp->w_doto += n;
        }
        if (wp->w_markp == lp1) {
            wp->w_markp = lp2;
            if (wp->w_marko > doto) wp->w_marko += n;
        }
        wp = wp->w_wndp;
    }
    return TRUE;
}

/*
 * linstr -- Insert a string at the current point
 */

int linstr(char *instr) {
    int status = TRUE;
    char tmpc;

    if (instr != NULL)
        while ((tmpc = *instr)) {
/* GGR - linsert inserts unicode....but we've been sent a (utf8) string! */
            status = (tmpc == '\n' ? lnewline() : linsert_byte(1, tmpc));

/* Insertion error? */
            if (status != TRUE) {
                mlwrite("%%Out of memory while inserting");
                break;
            }
            instr++;
        }
    return status;
}

/*
 * lins_nc -- Insert bytes given a pointer an count
 * Currently only used from this file.
 */

static int lins_nc(char *instr, int nb) {
    int status = TRUE;
    char tmpc;

    if (instr != NULL)
        while (nb--) {
            tmpc = *instr;
/* GGR - linsert inserts unicode....but we've been sent a (utf8) string! */
            status = (tmpc == '\n' ? lnewline() : linsert_byte(1, tmpc));
/* Insertion error? */
            if (status != TRUE) {
                mlwrite("%%Out of memory while inserting");
                break;
            }
            instr++;
        }
    return status;
}

/*
 * Insert n copies of unicode char c
 */
int linsert_uc(int n, unicode_t c) {
    char utf8[6];
    int bytes = unicode_to_utf8(c, utf8), i;

    if (bytes == 1)
        return linsert_byte(n, ch_as_uc(utf8[0]));
    for (i = 0; i < n; i++) {
        int j;
        for (j = 0; j < bytes; j++) {
            unsigned char cb = utf8[j];
            if (!linsert_byte(1, cb)) return FALSE;
        }
    }
    return TRUE;
}

/*
 * Overwrite a character into the current line at the current position
 *
 * int c;       character to overwrite on current position
 */
int lowrite(unicode_t c) {
    if (curwp->w_doto < curwp->w_dotp->l_used &&
         (lgetc(curwp->w_dotp, curwp->w_doto) != '\t' ||
         ((curwp->w_doto) & tabmask) == tabmask))
                ldelchar(1, FALSE);
    return linsert_uc(1, c);
}

/*
 * lover -- Overwrite a string at the current point
 */
int lover(char *ostr) {
    int status = TRUE;
    char tmpc;

    if (ostr != NULL) {
        while ((tmpc = *ostr) && status == TRUE) {
            status = (tmpc == '\n' ? lnewline() : lowrite(tmpc));
            if (status != TRUE) {   /* Insertion error? */
                mlwrite("%%Out of memory while overwriting");
                break;
            }
            ostr++;
        }
    }
    return status;
}

/*
 * Insert a newline into the buffer at the current location of dot in the
 * current window. The funny ass-backwards way it does things is not a botch;
 * it just makes the last line in the file not a special case. Return TRUE if
 * everything works out and FALSE on error (memory allocation failure). The
 * update of dot and mark is a bit easier than in the above case, because the
 * split forces more updating.
 */
int lnewline(void) {
    char *cp1;
    char *cp2;
    struct line *lp1;
    struct line *lp2;
    int doto;
    struct window *wp;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
         return rdonly();           /* we are in read only mode     */
#if SCROLLCODE
    lchange(WFHARD | WFINS);
#else
    lchange(WFHARD);
#endif
/* Ignore the original comment at the start of the routine - we *do* need
 * to make a special case of the last line *if we are at the end of it*
 * AND it is not empty!
 * In that case we don't need to allocate a new line, but do need to
 * update the current line pointer (to the next line, which will be headp)
 * and set doto top zero.
 */
    if (force_newline) {
        force_newline = 0;          /* Reset the force */
    }
    else if (curwp->w_dotp->l_used
             && curwp->w_doto == curwp->w_dotp->l_used
             && lforw(curwp->w_dotp) == curbp->b_linep) {
        curwp->w_dotp = lforw(curwp->w_dotp);
        curwp->w_doto = 0;
        return TRUE;
    }

    lp1 = curwp->w_dotp;    /* Get the address and  */
    doto = curwp->w_doto;   /* offset of "."        */
    if ((lp2 = lalloc(doto)) == NULL)   /* New first half line      */
        return FALSE;
    cp1 = &lp1->l_text[0];  /* Shuffle text around  */
    cp2 = &lp2->l_text[0];
    while (cp1 != &lp1->l_text[doto]) *cp2++ = *cp1++;
    cp2 = &lp1->l_text[0];
    while (cp1 != &lp1->l_text[lp1->l_used]) *cp2++ = *cp1++;
    lp1->l_used -= doto;
    lp2->l_bp = lp1->l_bp;
    lp1->l_bp = lp2;
    lp2->l_bp->l_fp = lp2;
    lp2->l_fp = lp1;
    wp = wheadp;                        /* Windows              */
    while (wp != NULL) {
        if (wp->w_linep == lp1)     wp->w_linep = lp2;
        if (wp->w_dotp == lp1) {
            if (wp->w_doto < doto)  wp->w_dotp = lp2;
            else                    wp->w_doto -= doto;
        }
        if (wp->w_markp == lp1) {
            if (wp->w_marko < doto) wp->w_markp = lp2;
            else                    wp->w_marko -= doto;
        }
        wp = wp->w_wndp;
    }
    return TRUE;
}

int lgetchar(unicode_t *c) {
    int len = llength(curwp->w_dotp);
    char *buf = curwp->w_dotp->l_text;
    return utf8_to_unicode(buf, curwp->w_doto, len, c);
}

/* Get the grapheme structure for what point is looking at.
 * Returns the number of bytes used up by the utf8 string.
 */
int lgetgrapheme(struct grapheme *gp, int utf8_len_only) {
    int len = llength(curwp->w_dotp);

/* Fudge in NL if at eol.
 * ldelchar needs to know there is one char to step over
 * Also, return nothing if out of range(!).
 */
    if (curwp->w_doto == len) {
        gp->uc = 0x0a;
        gp->cdm = 0;
        gp->ex = NULL;
        return 1;
    }
    if (curwp->w_doto >= len) {
        gp->uc = 0;
        gp->cdm = 0;
        gp->ex = NULL;
        return 0;
    }
    char *buf = curwp->w_dotp->l_text;
    int used = utf8_to_unicode(buf, curwp->w_doto, len, &(gp->uc));
    gp->cdm = 0;
    gp->ex = NULL;
    unicode_t uc;
    int xtra = utf8_to_unicode(buf, curwp->w_doto+used, len, &uc);
    if (!zerowidth_type(uc)) {
        return used;
    }
    used += xtra;
    gp->cdm = uc;
    for (int xc = 0;;xc++) {
        xtra = utf8_to_unicode(buf, curwp->w_doto+used, len, &uc);
        if (!zerowidth_type(uc)) break;
        used += xtra;
        if (!utf8_len_only) {
            gp->ex = realloc(gp->ex, (xc+2)*sizeof(unicode_t));
            gp->ex[xc] = uc;
            gp->ex[xc+1] = UEM_NOCHAR;
        }
    }
    return used;
}

/* Put the grapheme structure into the buffer at the current point.
 * Just a simple matter of running linsert_uc() on each unicode char.
 */
int lputgrapheme(struct grapheme *gp) {

    int status = linsert_uc(1, gp->uc);
    if (gp->cdm == 0) return status;
    status = linsert_uc(1, gp->cdm);
    if (gp->ex == NULL) return status;
    int xc = 0;
    while (gp->ex[xc] != UEM_NOCHAR) {
        status = linsert_uc(1, gp->ex[xc]);
        if (status) return status;
    }
    return status;
}

/*
 * ldelete() really fundamentally works on bytes, not characters.
 * It is used for things like "scan 5 words forwards, and remove
 * the bytes we scanned".
 * GGR - cater for zero-width characters...with lgetgrapheme.
 *
 * If you want to delete characters, use ldelchar().
 */
int ldelchar(long n, int kflag) {
    while (n-- > 0) {
        struct grapheme gc;
        if (!ldelete(lgetgrapheme(&gc, TRUE), kflag)) return FALSE;
    }
    return TRUE;
}

/*
 * This function deletes "n" bytes, starting at dot. It understands how do deal
 * with end of lines, etc. It returns TRUE if all of the characters were
 * deleted, and FALSE if they were not (because dot ran into the end of the
 * buffer. The "kflag" is TRUE if the text should be put in the kill buffer.
 *
 * long n;              # of chars to delete
 * int kflag;            put killed text in kill buffer flag
 */
int ldelete(long n, int kflag) {
    char *cp1;
    char *cp2;
    struct line *dotp;
    int doto;
    int chunk;
    struct window *wp;

    if (curbp->b_mode & MDVIEW) /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    while (n != 0) {
        dotp = curwp->w_dotp;
        doto = curwp->w_doto;
        if (dotp == curbp->b_linep)     /* Hit end of buffer.   */
            return FALSE;
        chunk = dotp->l_used - doto;    /* Size of chunk.       */
        if (chunk > n) chunk = n;
        if (chunk == 0) {       /* End of line, merge.  */
#if SCROLLCODE
            lchange(WFHARD | WFKILLS);
#else
            lchange(WFHARD);
#endif
            if (ldelnewline() == FALSE
                  || (kflag != FALSE && kinsert('\n') == FALSE))
                return FALSE;
            --n;
            continue;
        }
        lchange(WFEDIT);
        cp1 = &dotp->l_text[doto];      /* Scrunch text.        */
        cp2 = cp1 + chunk;
        if (kflag != FALSE) {           /* Kill?                */
            while (cp1 != cp2) {
                if (kinsert(*cp1) == FALSE) return FALSE;
                ++cp1;
            }
            cp1 = &dotp->l_text[doto];
        }
        while (cp2 != &dotp->l_text[dotp->l_used]) *cp1++ = *cp2++;
        dotp->l_used -= chunk;
        wp = wheadp;                    /* Fix windows          */
        while (wp != NULL) {
            if (wp->w_dotp == dotp && wp->w_doto >= doto) {
                wp->w_doto -= chunk;
                if (wp->w_doto < doto) wp->w_doto = doto;
            }
            if (wp->w_markp == dotp && wp->w_marko >= doto) {
                wp->w_marko -= chunk;
                if (wp->w_marko < doto) wp->w_marko = doto;
            }
            wp = wp->w_wndp;
        }
        n -= chunk;
    }
    return TRUE;
}

/*
 * getctext:    grab and return a string with the text of
 *              the current line
 */
char *getctext(void) {
    struct line *lp;            /* line to copy */
    int size;                   /* length of line to return */
    char *sp;                   /* string pointer into line */
    char *dp;                   /* string pointer into returned line */
    static char rline[NSTRING]; /* line to return */

/* Find the contents of the current line and its length */
    lp = curwp->w_dotp;
    sp = lp->l_text;
    size = lp->l_used;
    if (size >= NSTRING) size = NSTRING - 1;

/* Copy it across */
    dp = rline;
    while (size--) *dp++ = *sp++;
    *dp = 0;
    return rline;
}

/*
 * putctext:
 *      replace the current line with the passed in text
 *
 * char *iline;                 contents of new line
 */
int putctext(char *iline) {
    int status;

/* Delete the current line */
    curwp->w_doto = 0;      /* starting at the beginning of the line */
    if ((status = killtext(TRUE, 1)) != TRUE) return status;

/* Insert the new line */
    if ((status = linstr(iline)) != TRUE) return status;
    status = lnewline();
    backline(TRUE, 1);
    return status;
}

/*
 * Delete a newline. Join the current line with the next line. If the next line
 * is the magic header line always return TRUE; merging the last line with the
 * header line can be thought of as always being a successful operation, even
 * if nothing is done, and this makes the kill buffer work "right". Easy cases
 * can be done by shuffling data around. Hard cases require that lines be moved
 * about in memory. Return FALSE on error and TRUE if all looks ok. Called by
 * "ldelete" only.
 */
int ldelnewline(void) {
    char *cp1;
    char *cp2;
    struct line *lp1;
    struct line *lp2;
    struct line *lp3;
    struct window *wp;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    lp1 = curwp->w_dotp;
    lp2 = lp1->l_fp;
    if (lp2 == curbp->b_linep) {    /* At the buffer end.           */
        if (lp1->l_used == 0)   /* Blank line.                  */
            lfree(lp1);
        return TRUE;
    }
    if (lp2->l_used <= lp1->l_size - lp1->l_used) {
        cp1 = &lp1->l_text[lp1->l_used];
        cp2 = &lp2->l_text[0];
        while (cp2 != &lp2->l_text[lp2->l_used]) *cp1++ = *cp2++;
        wp = wheadp;
        while (wp != NULL) {
            if (wp->w_linep == lp2) wp->w_linep = lp1;
            if (wp->w_dotp == lp2) {
                wp->w_dotp = lp1;
                wp->w_doto += lp1->l_used;
            }
            if (wp->w_markp == lp2) {
                wp->w_markp = lp1;
                wp->w_marko += lp1->l_used;
            }
            wp = wp->w_wndp;
        }
        lp1->l_used += lp2->l_used;
        lp1->l_fp = lp2->l_fp;
        lp2->l_fp->l_bp = lp1;
        free((char *)lp2);
        return TRUE;
    }
    if ((lp3 = lalloc(lp1->l_used + lp2->l_used)) == NULL) return FALSE;
    cp1 = &lp1->l_text[0];
    cp2 = &lp3->l_text[0];
    while (cp1 != &lp1->l_text[lp1->l_used]) *cp2++ = *cp1++;
    cp1 = &lp2->l_text[0];
    while (cp1 != &lp2->l_text[lp2->l_used]) *cp2++ = *cp1++;
    lp1->l_bp->l_fp = lp3;
    lp3->l_fp = lp2->l_fp;
    lp2->l_fp->l_bp = lp3;
    lp3->l_bp = lp1->l_bp;
    wp = wheadp;
    while (wp != NULL) {
        if (wp->w_linep == lp1 || wp->w_linep == lp2) wp->w_linep = lp3;
        if (wp->w_dotp == lp1)
            wp->w_dotp = lp3;
        else if (wp->w_dotp == lp2) {
            wp->w_dotp = lp3;
            wp->w_doto += lp1->l_used;
        }
        if (wp->w_markp == lp1)
            wp->w_markp = lp3;
        else if (wp->w_markp == lp2) {
            wp->w_markp = lp3;
            wp->w_marko += lp1->l_used;
        }
        wp = wp->w_wndp;
    }
    free((char *)lp1);
    free((char *)lp2);
    return TRUE;
}

/*
 * Delete all of the text saved in the kill buffer. Called by commands when a
 * new kill context is being created. The kill buffer array is released, just
 * in case the buffer has grown to immense size. No errors.
 */
void kdelete(void) {
    struct kill *kp;        /* ptr to scan kill buffer chunk list */

/* First, delete all the chunks on the bottom item.
 * This is the one we are about to remove.
 */
    if (kbufh[KRING_SIZE-1] != NULL) {
        kbufp = kbufh[KRING_SIZE-1];
        struct kill *tp = kbufh[KRING_SIZE-1];
        while (tp != NULL) {
            kp = kbufp->d_next;
            free(tp);
            tp = kp;
        }
    }

/* Move the remaining ones down */

    int ix = KRING_SIZE-1;
    while (ix--) {
        kbufh[ix+1] = kbufh[ix];
        kused[ix+1] = kused[ix];
    }

/* Create a new one at the top and reset all kill buffer pointers */

    kbufh[0] = kbufp = NULL;
    kused[0] = KBLOCK;
}

/* A function to rotate the lastmb ring - NOT bindable.
 */
static char *lastmb[KRING_SIZE] = {[0 ... KRING_SIZE-1] = NULL};

static void rotate_lastmb_ring(int n) {
    if (n == 0) return;

    int rotate_count = n % KRING_SIZE;
    if (rotate_count < 0) rotate_count += KRING_SIZE;
    rotate_count = KRING_SIZE - rotate_count;   /* So we go the right way */
    if (rotate_count > 0) {
        char *tmp[KRING_SIZE];
        int dx = rotate_count;
        for (int ix = 0; ix < KRING_SIZE; ix++, dx++) {
            dx %= KRING_SIZE;
            tmp[dx] = lastmb[ix];
        }
        memcpy(lastmb, tmp, sizeof(tmp));
    }
    return;
}

/* Add an entry at the top.
 * Done by rotating the entries by -1 and replacing the now-top entry
 */
void addto_lastmb_ring(char *mb_text) {
    rotate_lastmb_ring(-1);
    if (lastmb[0]) free(lastmb[0]);
    lastmb[0] = strdup(mb_text);
    return;
}

/* A function to rotate the kill ring.
 * Not normally bound, but is bindable.
 */

int rotate_kill_ring(int f, int n) {
    UNUSED(f);
    if (n == 0) return TRUE;

    int rotate_count = n % KRING_SIZE;
    if (rotate_count < 0) rotate_count += KRING_SIZE;
    rotate_count = KRING_SIZE - rotate_count;   /* So we go the right way */
    if (rotate_count > 0) {
        struct kill *tmp_bufh[KRING_SIZE];
        int tmp_used[KRING_SIZE];
        int dx = rotate_count;
        for (int ix = 0; ix < KRING_SIZE; ix++, dx++) {
            dx %= KRING_SIZE;
            tmp_bufh[dx] = kbufh[ix];
            tmp_used[dx] = kused[ix];
        }
        memcpy(kbufh, tmp_bufh, sizeof(tmp_bufh));
        memcpy(kused, tmp_used, sizeof(tmp_used));
    }
    kbufp = kbufh[0];
    return TRUE;
}

/*
 * Insert a character to the kill buffer, allocating new chunks as needed.
 * Return TRUE if all is well, and FALSE on errors.
 *
 * int c;                       character to insert in the kill buffer
 */
int kinsert(int c) {
    struct kill *nchunk;    /* ptr to newly malloced chunk */

/* Check to see if we need a new chunk */
    if (kused[0] >= KBLOCK) {
        if ((nchunk = (struct kill *)malloc(sizeof(struct kill))) == NULL)
            return FALSE;
        if (kbufh[0] == NULL)  /* set head ptr if first time */
            kbufh[0] = nchunk;
        if (kbufp != NULL)  /* point the current to this new one */
            kbufp->d_next = nchunk;
        kbufp = nchunk;
        kbufp->d_next = NULL;
        kused[0] = 0;
    }

/* And now insert the character */
    kbufp->d_chunk[kused[0]++] = c;
    return TRUE;
}

/*
 * Yank text back from the kill buffer. This is really easy. All of the work
 * is done by the standard insert routines. All you do is run the loop, and
 * check for errors. Bound to "C-Y".
 * The yankmb() code is very similar, but the lins* loop differs.
 */
int yank(int f, int n) {
    UNUSED(f);

    char *sp;                       /* pointer into string to insert */
    struct kill *kp;                /* pointer into kill buffer */

/* Don't allow this command if we are in read only mode */

    if (curbp->b_mode & MDVIEW) return rdonly();

    if (yank_mode == GNU) {     /* A *given* numeric arg is kill rotating */
        if (f && n) rotate_kill_ring(f, n); /* No rotate on default */
        n = 1;                              /* With 1 loop pass */
    }
    else {                      /* Must be the old style - no -ve */
        if (n < 0) return FALSE;
    }

/* We are about to yank, so define an empty region at the current point,
 * in case we don't actually have any text to yank.
 * This ensure that we have a valid region after any yank for any
 * re-kill or yank_replace().
 */
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;

/* Make sure there is something to yank */
    if (kbufh[0] == NULL) {
        thisflag |= CFYANK;         /* It's still a yank... */
        last_yank = NormalYank;     /* Save the type */
        return TRUE;                /* not an error, just nothing */
    }

/* We need to handle the case of being at the start of an empty buffer.
 * If we just let things run and yank (say) 2 complete lines, the current
 * point will be at the start of the third line. With SCROLLCODE set this
 * will be on-screen so nothing gets redrawn - the added text is left
 * positioned just off the top of the screen; which is a bit disconcerting.
 * So if the next line is the same as the previous line (which can only
 * happen if we are in a single-line buffer, when both point to the headp)
 * we set a flag to do a final reposition().
 */
    int need_reposition = (lforw(curwp->w_dotp) == lback(curwp->w_dotp));

/* Handle the end of buffer. If we do nothing the mark will move with it
 * whereas we want it to stay after the last current character (any final
 * newline doesn't really exist).
 * So we remember the previous line and later set the mark to the next
 * line from there, offset zero. (same code is in yankmb())
 */
    struct line *fixup_line = NULL;
    if (curwp->w_dotp == curbp->b_linep && curwp->w_doto == 0)
         fixup_line = lback(curbp->b_linep);

/* For each time.... */
    while (n--) {
        kp = kbufh[0];
        while (kp != NULL) {
            int nb, status;
            if (kp->d_next == NULL) nb = kused[0];
            else                    nb = KBLOCK;
            sp = kp->d_chunk;
            if (!fixup_line) force_newline = 1;
            if ((status = lins_nc(sp, nb)) != TRUE) return status;
            force_newline = 0;
            kp = kp->d_next;
        }
    }
/* This will display the inserted text, leaving the last line at the last
 * but one line, if there is sufficient text for that, otherwise with the
 * top line at the top.
 */
    if (need_reposition) reposition(TRUE, -1);

/* Do any fixup for the original mark being at end of buffer */
    if (fixup_line) {
        curwp->w_markp = lforw(fixup_line);
        curwp->w_marko = 0;
    }
    thisflag |= CFYANK;         /* This is a yank */
    last_yank = NormalYank;     /* Save the type */
    return TRUE;
}

/* Yank back last minibuffer
 * The yank() code is very similar, but the lins* loop differs.
 */
int yankmb(int f, int n) {
    UNUSED(f);

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if  */
        return(rdonly());           /* we are in read only mode     */
    if (n < 0) return (FALSE);

/* We are about to yank, so define an empty region at the current point,
 * in case we don't actually have any text to yank.
 * This ensure that we have a valid region after any yank for any
 * re-kill or yank_replace().
 */
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;

/* Make sure there is something to yank */
    if (lastmb[0] && strlen(lastmb[0]) == 0) {
        thisflag |= CFYANK;             /* It's still a yank... */
        last_yank = MiniBufferYank;     /* Save the type */
        return TRUE;                    /* not an error, just nothing */
    }

/* We need to handle the case of being at the start of an empty buffer.
 * If we just let things run and yank (say) 2 complete lines, the current
 * point will be at the start of the third line. With SCROLLCODE set this
 * will be on-screen so nothing gets redrawn - the added text is left
 * positioned just off the top of the screen; which is a bit disconcerting.
 * So if the next line is the same as the previous line (which can only
 * happen if we are in a single-line buffer, when both point to the headp)
 * we set a flag to do a final reposition().
 */
    int need_reposition = (lforw(curwp->w_dotp) == lback(curwp->w_dotp));

/* Handle the end of buffer. If we do nothing the mark will move with it
 * whereas we want it to stay after the last current character (any final
 * newline doesn't really exist).
 * So we remember the previous line and later set the mark to the next
 * line from there, offset zero. (same code is in yankmb())
 */
    struct line *fixup_line = NULL;
    if (curwp->w_dotp == curbp->b_linep /* && curwp->w_doto == 0 */)
         fixup_line = lback(curbp->b_linep);

    while (n--) {
        int status;
        if (!fixup_line) force_newline = 1;
        if ((status = linstr(lastmb[0])) != TRUE) return status;
        force_newline = 0;
    }

/* This will display the inserted text, leaving the last line at the last
 * but one line, if there is sufficient text for that, otherwise with the
 * top line at the top.
 */
    if (need_reposition) reposition(TRUE, -1);

/* Do any fixup for the original mark being at end of buffer */
    if (fixup_line) {
        curwp->w_markp = lforw(fixup_line);
        curwp->w_marko = 0;
    }
    thisflag |= CFYANK;         /* This is a yank */
    last_yank = MiniBufferYank; /* Save the type */
    return (TRUE);
}

/* A function to replace the last yank with text from the kill-ring.
 * Can *only* be used when a yank (or this function) was the last
 * function used.
 */
int yank_replace(int f, int n) {
    UNUSED(f);
    int s;

/* Don't allow this command if we are in read only mode */

    if (curbp->b_mode & MDVIEW) return rdonly();

    int do_kill = 1;
    if (!(lastflag & CFYANK)) {
        if (yank_mode == GNU) {
           mlwrite("Last command was not a yank!");
           return FALSE;
        }
        else
            do_kill = 0;
    }

/* This is essentially the killregion() code (q.v.).
 * But we don't want to check for some things, and don't wan't ldelete()
 * to move the deleted region to the kill ring (it's alreayd there...).
 */
    if (do_kill) {              /* May not be killing */
        struct region region;
        if ((s = getregion(&region)) != TRUE) return s;
        curwp->w_dotp = region.r_linep;
        curwp->w_doto = region.r_offset;
/* Don't put killed text onto kill ring */
        if ((s = ldelete(region.r_size, FALSE)) != TRUE) return s;
    }

/* yank()/yankmb()  already do what we want, so just use them.
 * But for this "indirect" call we need to send different values for n
 * to yank() as we've done all the required rotating.
 * NOTE: that we make this call even if there is nothing to yank. This
 *       allows yank*() to set any required bits, e.g. the yank region
 *       and setting CFYANK in thisflag
 */
    if (last_yank == NormalYank) {
        rotate_kill_ring(0, n);
        return yank(0, (yank_mode == GNU)? 0: 1);
    }
    else {
        rotate_lastmb_ring(n);
        return yankmb(0, 1);
    }
}
