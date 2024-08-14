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

#include <stdio.h>
#if __sun
#include <alloca.h>
#endif

#define LINE_C

#include "line.h"
#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "utf8.h"

static int force_newline = 0;   /* lnewline may need to be told this */

/* This routine allocates a block of memory large enough to hold a struct
 * line.
 * Since the text part is now a dynamic buffer all lines are allocated
 * as empty.
 * The forward and backward pointers are not set at all - that is
 * for the caller to do.
 */
static db_bufdef(init_db);
struct line *lalloc(void) {
    struct line *lp;

    lp = (struct line *)Xmalloc(sizeof(struct line));
    lp->l_ = init_db;

    return lp;
}

/* Delete line "lp".
 * Fix all of the dot/pins/marks that might point at it (they are moved
 * to offset 0 of the next line).
 * Unlink the line from its line list.
 * Release the memory.
 */
void lfree(struct line *lp) {

    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_linep == lp) wp->w_linep = lp->l_fp;
        if (wp->w.dotp == lp) {
            wp->w.dotp = lp->l_fp;
            wp->w.doto = 0;
        }
        if (wp->w.markp == lp) {
            wp->w.markp = lp->l_fp;
            wp->w.marko = 0;
        }
    }
    for (struct buffer *bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if (bp->b_nwnd == 0) {
            if (bp->b.dotp == lp) {
                bp->b.dotp = lp->l_fp;
                bp->b.doto = 0;
            }
            if (bp->b.markp == lp) {
                bp->b.markp = lp->l_fp;
                bp->b.marko = 0;
            }
        }
    }
    if (sysmark.p == lp) {
        sysmark.p = lp->l_fp;
        sysmark.o = 0;
    }
    for (linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
        if (mmi(mp, lp) == lp) {
            mmi(mp, lp) = mmi(mp, lp)->l_fp;
            mmi(mp, offset) = 0;
        }
    }

    lp->l_bp->l_fp = lp->l_fp;
    lp->l_fp->l_bp = lp->l_bp;

    db_free(lp->l_);
    Xfree(lp);
}

/* This routine gets called when a character is changed in place in the
 * current buffer. It updates all of the required flags in the buffer and
 * window system. The flag used is passed as an argument; if the buffer is
 * being displayed in more than 1 window we change EDIT to HARD. Set MODE
 * if the mode line needs to be updated (the "*" has to be set).
 */
void lchange(int flag) {

    if (curbp->b_nwnd != 1)             /* Ensure hard.         */
        flag = WFHARD;
    if ((curbp->b_flag & BFCHG) == 0) { /* First change, so     */
        flag |= WFMODE;             /* update mode lines.   */
        curbp->b_flag |= BFCHG;
    }
/* If we are modfying the buffer that the match-group info points
 * to we have to mark them as invalid.
 */
    if (curbp == group_match_buffer) group_match_buffer = NULL;

    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_bufp == curbp) wp->w_flag |= flag;
    }

/* If this is a translation table, remove any compiled data */

    if ((curbp->b_type == BTPHON) && curbp->ptt_headp) ptt_free(curbp);
}

/* Insert "n" copies of the character "c" at the current location of dot. In
 * the easy case all that happens is the text is stored in the line. In the
 * hard case, the line has to be Xreallocated. When the window list is updated,
 * take special care; I screwed it up once. You always update dot in the
 * current window. You update mark, and a dot in another window, if it is
 * greater than the place where you did the insert. Return TRUE if all is
 * well, and FALSE on errors.
 */
int linsert_byte(int n, unsigned char c) {
    struct line *lp1;
    int doto;

    if (curbp->b_mode & MDVIEW) /* Don't allow this command if */
        return rdonly();        /* we are in read only mode    */
    lchange(WFEDIT);
    lp1 = curwp->w.dotp;        /* Current line         */
/* What we wish to insert */
    char *tbuf = alloca(n+1);
    memset(tbuf, c, n);
    if (lp1 == curbp->b_linep) {/* At the end: special  */
        if (curwp->w.doto != 0) {
            mlwrite_one("bug: linsert");
            return FALSE;
        }
        terminate_str(tbuf + n);
/* We have a function to add a line at the end of a buffer */
        addline_to_curb(tbuf);
/* addline_to_curb will put dot on the dummy end line, effectively
 * adding a newline at the end of it.
 * So we move dot back 1 char, to the end of what we've added....
 */
        backchar(0, 1);
        return TRUE;
    }
    doto = curwp->w.doto;       /* Save for later. */
/* Insert the new text */
    db_insertn_at(lp1->l_, tbuf, n, doto);

/* Update dot/mark/pins in windows
 * NOTE that the dot check is ">=", as we wish to move with dot as
 * we insert, but we want to leave mark and pin where they were if they
 * started at dot.
 */
    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if ((wp->w.dotp == lp1) && (wp->w.doto >= doto)) {
            wp->w.doto += n;
        }
        if ((wp->w.markp == lp1) && (wp->w.marko > doto)) {
            wp->w.marko += n;
        }
    }
    if ((sysmark.p == lp1) && (sysmark.o > doto)) sysmark.o += n;
    for(linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
        if (mmi(mp, lp) == lp1) {
            if (mmi(mp, offset) > doto) mmi(mp, offset) += n;
         }
    }

    return TRUE;
}

/* insert spaces forward into text
 *
 * int f, n;            default flag and numeric argument
 */
int insspace(int f, int n) {
    UNUSED(f);
    if (!linsert_byte(n, ' ')) return FALSE;
    back_grapheme(n);
    return TRUE;
}

/* Insert a newline into the buffer at the current location of dot in the
 * current window.
 * Now done by splitting of the "excess" text beyond the newline into
 * a new line buffer.
 */
int lnewline(void) {
    struct line *lp1;
    struct line *lp2;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if  */
         return rdonly();           /* we are in read only mode     */
    lchange(WFHARD | WFINS);

/* We need to make a special case of the last line *if we are at the
 * end of it*!
 * If it is not empty we just move dot to the next (dummy) line.
 * In that case we don't need to allocate a new line, but do need to
 * update the current line pointer (to the next line, which will be headp)
 * and set doto to zero.
 */
    if (force_newline) {
        force_newline = 0;          /* Reset the force */
    }
/* Are we at the end of the text on the last "real" line? */
    else if (lused(curwp->w.dotp)
             && (size_t)curwp->w.doto == lused(curwp->w.dotp)
             && lforw(curwp->w.dotp) == curbp->b_linep) {
        curwp->w.dotp = lforw(curwp->w.dotp);
        curwp->w.doto = 0;
        return TRUE;
    }
/* Are we are already on the dummy last line?
 * If so, we need to put a new empty line in place before it (and stay
 * where we are - on the dummy end line).
 * NOTE: that if the mark or a pin is on the last line, it
 *       will also move with it.
 */
    else if (curwp->w.dotp == curbp->b_linep) {
        lp1 = lalloc();
        lp2 = curwp->w.dotp;
/* Fix up back/forw pointers for these two lines */
        lp2->l_bp->l_fp = lp1;
        lp1->l_bp = lp2->l_bp;
        lp1->l_fp = lp2;
        lp2->l_bp = lp1;

/* If this also the top line of the window (i.e., the buffer was empty)
 * we need to update that to be the newly-allocated line.
 */
        if (curwp->w_linep == curwp->w.dotp) curwp->w_linep = lp1;

        return TRUE;
    }

/* Otherwise we just split off the excess text into a new line.
 * We do this such that lp1 is the start of the line and lp2 the end
 * of it (so follows lp1).
 */
    lp1 = curwp->w.dotp;        /* Get the address and  */
    int doto = curwp->w.doto;   /*   offset of "."      */
    int xs = lused(lp1) - doto;

/* Create a new line for the second part and copy the "trailing" text in */
    lp2 = lalloc();
    db_setn(ldb(lp2), ltext(lp1)+doto, xs);
    db_truncate(ldb(lp1), doto);    /* valid text left in lp1 */

/* Fix up back/forw pointers for the two lines */

    lp2->l_fp = lp1->l_fp;
    lp1->l_fp = lp2;
    lp2->l_bp = lp1;
    lp2->l_fp->l_bp = lp2;

/* When inserting a newline we want to keep any mark on the original line if
 * the newline is inserted after it, *including* if we insert a newline
 * when already at the end of line.
 * But we want to move dot to the next line if we insert a newline when
 * already at the end of line.
 * Hence the different < and <= tests below.
 */
    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if ((wp->w.dotp == lp1) && (wp->w.doto >= doto)) {
            wp->w.dotp = lp2;
            wp->w.doto -= doto;
        }
        if ((wp->w.markp == lp1) && (wp->w.marko > doto)) {
            wp->w.markp = lp2;
            wp->w.marko -= doto;
        }
    }
    if ((sysmark.p == lp1) && (sysmark.o > doto)) {
        sysmark.p = lp2;
        sysmark.o -= doto;
    }
    for (linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
        if ((mmi(mp, lp) == lp1) && (mmi(mp, offset) > doto)) {
            mmi(mp, lp) = mmi(mp, lp)->l_fp;
            mmi(mp, offset) -= doto;
        }
    }
    return TRUE;
}

/* linstr -- Insert a string at the current point
 */

int linstr(char *instr) {
    int status = TRUE;
    char tmpc;

/* We have to check this here to avoid the "Out of memory" message
 * on failure when linsert_byte() gripes about it.
 */
    if (curbp->b_mode & MDVIEW) /* don't allow this command if */
        return rdonly();        /* we are in read only mode    */

    if (instr != NULL)
        while ((tmpc = *instr)) {
/* GGR - linsert inserts unicode....but we've been sent a (utf8) string! */
            status = (tmpc == '\n' ? lnewline() : linsert_byte(1, tmpc));

/* Insertion error? */
            if (status != TRUE) {
                mlwrite_one("Out of memory while inserting");
                break;
            }
            instr++;
        }
    return status;
}

/* lins_nc -- Insert bytes given a pointer and count
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
                mlwrite_one("Out of memory while inserting");
                break;
            }
            instr++;
        }
    return status;
}

/* Insert n copies of unicode char c
 */
int linsert_uc(int n, unicode_t c) {
    char utf8[6];

/* Short-cut the most-likely case - an ASCII char */

    if (c <= 0x7f) return linsert_byte(n, c);

    int bytes = unicode_to_utf8(c, utf8);
    if (bytes == 1)     /* Extended Latin1... */
        return linsert_byte(n, ch_as_uc(utf8[0]));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < bytes; j++) {
            if (!linsert_byte(1, ch_as_uc(utf8[j]))) return FALSE;
        }
    }
    return TRUE;
}

/* Get the grapheme structure for what point is looking at.
 * Returns the number of bytes used up by the utf8 string.
 * If no_ex_alloc is TRUE, don't alloc() any ex parts - just count them.
 * Now just a front-end to build_next_grapheme.
 */
int lgetgrapheme(struct grapheme *gp, int no_ex_alloc) {
    int len = lused(curwp->w.dotp);

    int spos = curwp->w.doto;
    int epos = build_next_grapheme(ltext(curwp->w.dotp), spos, len, gp,
         no_ex_alloc);
    return (epos - spos);
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
        if (!status) return status;
    }
    return status;
}

int ldelete(ue64I_t, int);     /* Forward declaration */
/* ldelete() really fundamentally works on bytes, not characters.
 * It is used for things like "scan 5 words forwards, and remove
 * the bytes we scanned".
 * GGR - cater for combining characters...with lgetgrapheme.
 * If you want to delete character-places<, use ldelgrapheme().
 */
int ldelgrapheme(ue64I_t n, int kflag) {
    while (n-- > 0) {
        struct grapheme gc;
        if (!ldelete(lgetgrapheme(&gc, TRUE), kflag)) return FALSE;
    }
    return TRUE;
}

/* Delete a newline. Join the current line with the next line.
 *  If the current line is empty - just delete it
 *  If the next line is the magic header line just return TRUE and do nothing
 *  Append line2 text to the end of line1
 *  Fix up the forward/backward pointers as line2 is about to go.
 *  Fix up any window pointers (headp and marks) that refer to line2
 *  Free the now redundant line2.
 */
static int ldelnewline(void) {
    struct line *lp1;
    struct line *lp2;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if  */
        return rdonly();            /* we are in read only mode     */
    lp1 = curwp->w.dotp;
    lp2 = lp1->l_fp;

/* All we need to do is append the text content lp2 to the end of
 * lp1.
 * However, there are some special cases.
 */
    if (lused(lp1) == 0) {          /* Blank line? Just remove it   */
        lfree(lp1);
        return TRUE;
    }
    if (lp2 == curbp->b_linep) {    /* At the buffer end? Do nothing. */
        return TRUE;
    }

/* Add the lp2 text to lp1 */

    int orig_lp1_len = lused(lp1);  /* Might be needed */
    db_appendn(lp1->l_, ltext(lp2), lused(lp2));

/* Now fix up lp1 forward pointer and lp2 back pointer */

    lp1->l_fp = lp2->l_fp;
    lp2->l_fp->l_bp = lp1;

/* Fix up anything which referred to lp2 */

    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_linep == lp2) wp->w_linep = lp1;
        if (wp->w.markp == lp2) {
            wp->w.markp = lp1;
            wp->w.marko += orig_lp1_len;
        }
    }
    if (sysmark.p == lp2) {
        sysmark.p = lp1;
        sysmark.o += orig_lp1_len;
    }
    lfree(lp2);
    return TRUE;
}

/* lover -- Overwrite a string at the current point
 *          This is a utf8 string!
 */
int lover(char *ostr) {
    int status = TRUE;

    if (ostr != NULL) {
        int maxlen = strlen(ostr);
        int offs = 0;
        while (*ostr && status == TRUE) {
            int bc = next_utf8_offset(ostr, offs, maxlen, TRUE);

/* For an overwrite we delete the next character and insert the new one.
 * If we are at end-of-line we don't delete (so the line is extended)
 * and if we're at a tab-stop we may not need to do the delete.
 */
            if ((size_t)curwp->w.doto < lused(curwp->w.dotp) &&
                 (lgetc(curwp->w.dotp, curwp->w.doto) != '\t' ||
                 ((curwp->w.doto) & tabmask) == tabmask)) {
                status = ldelgrapheme(1, FALSE);
                if (status != TRUE) continue;
            }
/* Now the insertion */
            if ((bc == 1) && (*(ostr+offs) == '\n'))
                 status = lnewline();
            else status = lins_nc(ostr+offs, bc);
/* Is it still working? */
            if (status != TRUE) {   /* Insertion error? */
                mlwrite_one("Out of memory while overwriting");
                break;
            }
            ostr += bc;
        }
    }
    return status;
}

/* Insert a character to the kill buffer, allocating new chunks as needed.
 * Return TRUE if all is well, and FALSE on errors.
 *
 * int c;                       character to insert in the kill buffer
 */
int kinsert(int c) {
    struct kill *nchunk;    /* ptr to newly Xmalloced chunk */

/* Check to see if we need a new chunk */
    if (kused[0] >= KBLOCK) {
        nchunk = (struct kill *)Xmalloc(sizeof(struct kill));
        if (kbufh[0] == NULL)   /* set head ptr if first time */
            kbufh[0] = nchunk;
        if (kbufp != NULL)      /* point the current to this new one */
            kbufp->d_next = nchunk;
        kbufp = nchunk;
        kbufp->d_next = NULL;
        kused[0] = 0;
    }

/* And now insert the character */
    kbufp->d_chunk[kused[0]++] = c;
    return TRUE;
}

/* This function deletes "n" bytes, starting at dot. It understands how do deal
 * with end of lines, etc. It returns TRUE if all of the characters were
 * deleted, and FALSE if they were not (because dot ran into the end of the
 * buffer. The "kflag" is TRUE if the text should be put in the kill buffer.
 * ue64I_t n;            # of chars to delete
 * int kflag;            put killed text in kill buffer flag
 */
int ldelete(ue64I_t n, int kflag) {
    struct line *dotp;
    int doto;
    int chunk;

    if (curbp->b_mode & MDVIEW) /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    while (n != 0) {
        dotp = curwp->w.dotp;
        doto = curwp->w.doto;
        if (dotp == curbp->b_linep) /* Hit end of buffer.   */
            return FALSE;
        chunk = lused(dotp) - doto; /* Size of chunk.       */
        if (chunk > n) chunk = n;
        if (chunk == 0) {       /* End of line, merge.  */
            lchange(WFHARD | WFKILLS);
            if (ldelnewline() == FALSE
                  || (kflag != FALSE && kinsert('\n') == FALSE))
                return FALSE;
            --n;
            continue;
        }
        lchange(WFEDIT);
        if (kflag != FALSE) {                       /* Kill? */
            const char *cp1 = ltext(dotp) + doto;   /* Scrunch text. */
            const char *cp2 = cp1 + chunk;
            while (cp1 != cp2) {
                if (kinsert(*cp1) == FALSE) return FALSE;
                ++cp1;
            }
        }
        db_deleten_at(dotp->l_, chunk, doto);

/* Fix-up windows */
        for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
            if (wp->w.dotp == dotp && wp->w.doto > doto) {
                wp->w.doto -= chunk;
                if (wp->w.doto < doto) wp->w.doto = doto;
            }
            if (wp->w.markp == dotp && wp->w.marko > doto) {
                wp->w.marko -= chunk;
                if (wp->w.marko < doto) wp->w.marko = doto;
            }
        }
        if (sysmark.p == dotp && sysmark.o > doto) {
            sysmark.o -= chunk;
            if (sysmark.o < doto) sysmark.o = doto;
        }
        for (linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
            if (mmi(mp, lp) == dotp) {
                if (mmi(mp, offset) > doto) {
                    mmi(mp, offset) -= chunk;
/* It can't move left of the deletion point */
                    if (mmi(mp, offset) < doto) mmi(mp, offset) = doto;
                }
             }
        }
        n -= chunk;
    }
    return TRUE;
}

/* getctext:    grab and return a string with the text of
 *              the current line
 */
//GML Can this be db_bufdef ?? Once vars are db_bufdefs??
static db_strdef(rline);   /* Line to return */
const char *getctext(void) {

/* Could we just return ltext(curwp->w.dotp), having ensured it is
 * NUL-terminated?
 */
    db_setn(rline, ltext(curwp->w.dotp), lused(curwp->w.dotp));
    return db_val(rline);
}

/* Free up the first kill buffer ring entry for new text by pushing all
 * of the others down by 1.
 * If the ring was already all in use then free the last entry (which will
 * be pushed off the list) first.
 */
void kdelete(void) {

/* First, delete all the chunks on the bottom item.
 * This is the one we are about to remove.
 */
    if (kbufh[KRING_SIZE-1] != NULL) {
        struct kill *np;        /* Next pointer */
        struct kill *tp = kbufh[KRING_SIZE-1];
        while (tp != NULL) {
            np = tp->d_next;
            Xfree(tp);
            tp = np;
        }
    }

/* Move the remaining ones down */

    int ix = KRING_SIZE-1;
    while (ix--) {      /* Move KRING_SIZE-2 ... 0 to one higher */
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
void addto_lastmb_ring(const char *mb_text) {
    rotate_lastmb_ring(-1);
    Xfree(lastmb[0]);
    lastmb[0] = Xstrdup(mb_text);
    return;
}

/* A function to rotate the kill ring.
 */

static void rotate_kill_ring(int n) {
    if (n == 0) return;

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
    return;
}

/* yank and yankmb have some duplicate code to run, so run it here. */

static struct line *orig_line;
static int orig_doto, need_reposition, do_force;

static void setup_for_yank(void) {

/* We need to handle the case of being at the start of an empty buffer.
 * If we just let things run and yank (say) 2 complete lines, the current
 * point will be at the start of the third line. With SCROLLCODE set (which
 * is now hard-wired) this will be on-screen so nothing gets redrawn - the
 * added text is left positioned just off the top of the screen; which is
 * a bit disconcerting.
 * So if the next line is the same as the previous line (which can only
 * happen if we are in a single-line buffer, when both point to the headp)
 * we set a flag to do a final reposition().
 */
    need_reposition = (lforw(curwp->w.dotp) == lback(curwp->w.dotp));

/* Handle the end of buffer. lnewline() needs to know if that is
 * where we are (by force_newline being UNSET).
 *
 * "End of buffer" state includes being at the end of the line before the
 * final (dummy) line.
 */
    if (((curwp->w.doto == 0) && (curwp->w.dotp == curbp->b_linep)) ||
        (((size_t)curwp->w.doto == lused(curwp->w.dotp)) &&
             (lforw(curwp->w.dotp) == curbp->b_linep))) {
        do_force = 0;
    }
    else do_force = 1;

/* After the yank we want to set the mark to the current dot position,
 * so remember this now.
 */
    orig_line = curwp->w.dotp;
    orig_doto = curwp->w.doto;
    return;
}

static void finish_for_yank(int yank_type) {

    force_newline = 0;

/* This will display the inserted text, leaving the last line at the last
 * but one line, if there is sufficient text for that, otherwise with the
 * top line at the top.
 */
    if (need_reposition) reposition(TRUE, -1);

/* We've just yanked, so set the mark to where we were, which makes the
 * yanked text become the current region, for any following
 * re-kill or yank_replace().
 */
    curwp->w.markp = orig_line;
    curwp->w.marko = orig_doto;

    com_flag |= CFYANK;         /* This is a yank */
    last_yank = yank_type;      /* Save the type */
    return;
}

/* Yank text back from the kill buffer. This is really easy. All of the work
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
        if (f && n) rotate_kill_ring(n);    /* No rotate on default */
        n = 1;                              /* With 1 loop pass */
    }
    else {                      /* Must be the old style - no -ve */
        if (n < 0) return FALSE;
    }

/* Make sure there is something to yank */
    if (kbufh[0] == NULL) {
        com_flag |= CFYANK;         /* It's still a yank... */
        last_yank = NormalYank;     /* Save the type */
        return TRUE;                /* not an error, just nothing */
    }

    setup_for_yank();

/* For each time....
 * force_newline is set in the loop each time in case lnewline unsets it.
 * But it only needs to be reset here once, after the loop.
 */
    while (n--) {
        kp = kbufh[0];
        while (kp != NULL) {
            int nb, status;
            if (kp->d_next == NULL) nb = kused[0];
            else                    nb = KBLOCK;
            sp = kp->d_chunk;
            if (do_force) force_newline = 1;
            if ((status = lins_nc(sp, nb)) != TRUE) return status;
             kp = kp->d_next;
        }
    }
    finish_for_yank(NormalYank);    /* Finish up and set the type */
    return TRUE;
}

/* Yank back last minibuffer
 * The yank() code is very similar, but the lins* loop differs.
 */
int yankmb(int f, int n) {
    UNUSED(f);

    if (curbp->b_mode & MDVIEW) /* Don't allow this command if  */
        return(rdonly());       /* we are in read only mode     */

    if (yank_mode == GNU) {     /* A *given* numeric arg is kill rotating */
        if (f && n) rotate_lastmb_ring(n);  /* No rotate on default */
        n = 1;                              /* With 1 loop pass */
    }
    else {                      /* Must be the old style - no -ve */
        if (n < 0) return FALSE;
    }

/* Make sure there is something to yank */
    if (lastmb[0] && strlen(lastmb[0]) == 0) {
        com_flag |= CFYANK;         /* It's still a yank... */
        last_yank = MiniBufferYank; /* Save the type */
        return TRUE;                /* not an error, just nothing */
    }

    setup_for_yank();

/* force_newline is set in the loop each time in case lnewline unsets it.
 * But it only needs to be reset here once, after the loop.
 */
    while (n--) {
        int status;
        if (do_force) force_newline = 1;
        if ((status = linstr(lastmb[0])) != TRUE) return status;
    }
    finish_for_yank(MiniBufferYank);    /* Finish up and set the type */
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
    if (!(com_flag & CFYANK)) {
        if (yank_mode == GNU) {
           mlwrite_one("Last command was not a yank!");
           return FALSE;
        }
        else
            do_kill = 0;
    }

/* This is essentially the killregion() code (q.v.).
 * But we don't want to check for some things, and don't want ldelete()
 * to move the deleted region to the kill ring (it's already there...).
 */
    if (do_kill) {              /* May not be killing */
        struct region region;
        if ((s = getregion(&region)) != TRUE) return s;
        curwp->w.dotp = region.r_linep;
        curwp->w.doto = region.r_offset;
/* Don't put killed text onto kill ring */
        if ((s = ldelete(region.r_bytes, FALSE)) != TRUE) return s;
    }

/* yank()/yankmb()  already do what we want, so just use them.
 * But for this "indirect" call we need to send different values for n
 * to yank() as we've done all the required rotating.
 * NOTE: that we make this call even if there is nothing to yank. This
 *       allows yank*() to set any required bits, e.g. the yank region
 *       and setting CFYANK in com_flag
 */
    if (last_yank == NormalYank) {
        rotate_kill_ring(n);
        return yank(0, (yank_mode == GNU)? 0: 1);
    }
    else {
        rotate_lastmb_ring(n);
        return yankmb(0, (yank_mode == GNU)? 0: 1);
    }
}

#ifdef DO_FREE
void free_line(void) {
    for (int i = 0; i < KRING_SIZE; i++) {
        struct kill *tp = kbufh[i];
        while (tp != NULL) {
            struct kill *np = tp->d_next;
            Xfree(tp);
            tp = np;
        }
    }

    db_free(rline);
    return;
}
#endif
