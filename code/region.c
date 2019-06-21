/*      region.c
 *
 *      The routines in this file deal with the region, that magic space
 *      between "." and mark. Some functions are commands. Some functions are
 *      just for internal use.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Kill the region.
 * Ask "getregion" to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 */
int killregion(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;
    struct region region;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
        return rdonly();        /* we are in read only mode     */
    if ((s = getregion(&region)) != TRUE)
        return s;
/* If the last command was a yank we don't want to change the kill-ring
 * (the text is what is already on the top, or the last minibuffer).
 */
    int save_to_kill_ring = (lastflag & CFYANK)? FALSE: TRUE;
    if ((lastflag & CFKILL) == 0) {         /* This is a kill type  */
        if (save_to_kill_ring) kdelete();   /* command, so do magic */
    }
    thisflag |= CFKILL;     /* kill buffer stuff.   */
    curwp->w_dotp = region.r_linep;
    curwp->w_doto = region.r_offset;
    return ldelete(region.r_size, save_to_kill_ring);
}

/* Copy all of the characters in the region to the kill buffer.
 * Don't move dot at all.
 * This is a bit like a kill region followed by a yank.
 * Bound to "M-W".
 */
int copyregion(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *linep;
    int loffs;
    int s;
    struct region region;

    if ((s = getregion(&region)) != TRUE)
        return s;
    if ((lastflag & CFKILL) == 0)   /* Kill type command.   */
        kdelete();
    thisflag |= CFKILL;
    linep = region.r_linep;         /* Current line.        */
    loffs = region.r_offset;        /* Current offset.      */
    while (region.r_size--) {
        if (loffs == llength(linep)) {  /* End of line. */
            if ((s = kinsert('\n')) != TRUE) return s;
            linep = lforw(linep);
            loffs = 0;
        }
        else {                      /* Middle of line.      */
            if ((s = kinsert(lgetc(linep, loffs))) != TRUE) return s;
            ++loffs;
        }
    }
    mlwrite(MLpre "region copied" MLpost);
    return TRUE;
}

/* Change case of region to the given newcase.
 * unicode(utf8) aware.
 * Zap all of the lower case characters in the region to upper case.
 * Use the region code to set the limits, then work through each line
 * in order re-casign tehe text on that line which needs to be changed.
 * Involves fiddling with the line buffer structures.
 * Call "lchange" to ensure that redisplay is done in all buffers.
 * Bound to "C-X C-L".
 */

/* Turns out a macro (replicating code) is more efficient in terms
 * of executable size than a function...
 */
#define ccr_Tail_Copy \
    if (b_end < llength(linep)) {   /* Shuffle chars along... */ \
        memmove(linep->l_text+b_offs+replen, \
            linep->l_text+b_end, \
            llength(linep)-b_end); \
    }

/* If mark or dot are on this line then we might need to update
 * their offset after the move.
 * They can only be at the start or end of the moved chars,
 * and if at the start there is nothing to do, as that hasn't moved.
 * Turns out a macro is more efficient (in terms of executable size)
 * than a function...
 */
#define MarkDotFixup(amount) \
    if ((curwp->w_markp == linep) && \
        (curwp->w_marko == (b_offs + this_blen))) \
          curwp->w_marko += amount; \
    if ((curwp->w_dotp == linep) && \
        (curwp->w_doto == (b_offs + this_blen))) \
          curwp->w_doto += amount

static int casechange_region(int newcase) { /* The handling function */
    struct region region;
    int s;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();            /* we are in read only mode     */
    if ((s = getregion(&region)) != TRUE) return s;

    lchange(WFHARD);                /* Marks buffer as changed */

/* On line 1 we start at the offset
 * On any succeeding lines we start at 0.
 * Remember to include the newline as we count down.
 */
    struct line *linep = region.r_linep;
    struct mstr mstr;
    for (int b_offs = region.r_offset; region.r_size > 0; linep = lforw(linep)) {
        int b_end = region.r_size + b_offs;
        if (b_end > llength(linep)) b_end = llength(linep);
        int this_blen = b_end - b_offs;
        utf8_recase(newcase, linep->l_text+b_offs, this_blen, &mstr);
        int replen = mstr.utf8c;            /* Less code when copied.. */
        char *repstr = mstr.str;            /* ...to simple local vars */
        if (replen == this_blen) {          /* Easy - just overwrite */
            memcpy(linep->l_text+b_offs, repstr, replen);
            free(repstr);
        }
        else if (replen < this_blen) {      /* So guaranteed the space */
            memcpy(linep->l_text+b_offs, repstr, replen);
            free(repstr);
            ccr_Tail_Copy;
            int b_less = this_blen - replen;
            llength(linep) -= b_less;       /* Fix-up length */
            MarkDotFixup(-b_less);
        }
        else {              /* replen > this_blen  Potentially trickier */
            int b_more = replen - this_blen;
            if ((linep->l_size - linep->l_used) >= b_more) {    /* OK */
                ccr_Tail_Copy;  /* Must move the tail out-of-the-way first!! */
                memcpy(linep->l_text+b_offs, repstr, replen);
                free(repstr);
                llength(linep) += b_more;   /* Fix-up length */
            }
            else {
/* Need to allocate a new line structure to replace the currewnt one... */
                struct line *newl = lalloc(llength(linep) + b_more);
/* Copy in and leading text on this line... */
                if (b_offs) memcpy(newl->l_text, linep->l_text, b_offs);
/* Copy in the replacement text */
                memcpy(newl->l_text+b_offs, repstr, replen);
                free(repstr);
/* Copy in and trailing text on this line... */
                ccr_Tail_Copy;
/* Now need to replace the current line with the new one in the linked list */
                lforw(lback(linep)) = newl;
                lback(lforw(linep)) = newl;
                lforw(newl) = lforw(linep);
                lback(newl) = lback(linep);
/* If mark or dot were on this old line then we need to move them to
 * the new one
 */
                if (curwp->w_markp == linep) curwp->w_markp = newl;
                if (curwp->w_dotp == linep)  curwp->w_dotp = newl;
                free(linep);
                linep = newl;
            }
            MarkDotFixup(b_more);
        }
        region.r_size -= this_blen + 1;
        b_offs = 0;
    }
    return TRUE;
}

/* Uppercase region.
 * Just a front-end to casechange_region()
 */
int upperregion(int f, int n) {
    UNUSED(f); UNUSED(n);
    return casechange_region(UTF8_UPPER);
}
/* Lowercase region.
 * Just a front-end to casechange_region()
 */
int lowerregion(int f, int n) {
    UNUSED(f); UNUSED(n);
    return casechange_region(UTF8_LOWER);
}

/* This routine figures out the bounds of the region in the current window
 * and fills in the fields of the "struct region" structure pointed
 * to by "rp".
 * Because the dot and mark are usually very close together we scan
 * outwards from dot looking for mark. This should save time.
 * Return a standard code.
 * Callers of this routine should be prepared to get an "ABORT" status;
 * we might make this have the conform thing later.
 */
int getregion(struct region *rp) {
    struct line *flp;
    struct line *blp;
    long fsize;
    long bsize;

    if (curwp->w_markp == NULL) {
        mlwrite("No mark set in this window");
        return FALSE;
    }
    if (curwp->w_dotp == curwp->w_markp) {
        rp->r_linep = curwp->w_dotp;
        if (curwp->w_doto < curwp->w_marko) {
            rp->r_offset = curwp->w_doto;
            rp->r_size = (long) (curwp->w_marko - curwp->w_doto);
        }
        else {
            rp->r_offset = curwp->w_marko;
            rp->r_size = (long) (curwp->w_doto - curwp->w_marko);
        }
        return TRUE;
    }
    blp = curwp->w_dotp;
    bsize = (long) curwp->w_doto;
    flp = curwp->w_dotp;
    fsize = (long) (llength(flp) - curwp->w_doto + 1);
    while (flp != curbp->b_linep || lback(blp) != curbp->b_linep) {
        if (flp != curbp->b_linep) {
            flp = lforw(flp);
            if (flp == curwp->w_markp) {
                rp->r_linep = curwp->w_dotp;
                rp->r_offset = curwp->w_doto;
                rp->r_size = fsize + curwp->w_marko;
                return TRUE;
             }
             fsize += llength(flp) + 1;
         }
         if (lback(blp) != curbp->b_linep) {
            blp = lback(blp);
            bsize += llength(blp) + 1;
            if (blp == curwp->w_markp) {
                rp->r_linep = blp;
                rp->r_offset = curwp->w_marko;
                rp->r_size = bsize - curwp->w_marko;
                return TRUE;
            }
        }
    }
    mlwrite("Bug: lost mark");
    return FALSE;
}

/* GGR extensions to regions */

/* Narrow-to-region (^X-<) makes all but the current region in
 * the current buffer invisable and unchangable
 */
int narrow(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;             /* return status */
    struct buffer *bp;      /* buffer being narrowed */
    struct window *wp;      /* windows to fix up pointers in as well */
    struct region creg;     /* region boundry structure */

/* Find the proper buffer and make sure we aren't already narrow */
    bp = curwp->w_bufp;     /* find the right buffer */
    if (bp->b_flag&BFNAROW) {
        mlwrite("This buffer is already narrowed");
        return(FALSE);
    }

/* We want to include all of the lines that mark and dot are on.
 * We also need to reset the original mark and dot if we return
 * without narrowing...
 */
    struct window orig_wp = *curwp;         /* Copy original struct */
    struct line *lp = lforw(bp->b_linep);
    int fix_up = 0;
/* Is mark or doto earliest in the file?
 * We need to move the *other* one to the start of its next line if
 * it is not at the start of a line.
 */
    while (lp != bp->b_linep) {
        if (lp == curwp->w_dotp) {          /* doto */
            fix_up = 1;
            break;
        }
        else if (lp == curwp->w_markp) {    /* mark */
            fix_up = -1;
            break;
        }
        lp = lforw(lp);
    }
    if ((fix_up == 1) && (curwp->w_marko > 0)) {
        curwp->w_markp = lforw(curwp->w_markp);
        curwp->w_marko = 0;
    }
    else if ((fix_up == -1) && (curwp->w_doto > 0)) {
        curwp->w_dotp = lforw(curwp->w_dotp);
        curwp->w_doto = 0;
    }

/* Find the boundaries of the current region */
    if ((status = getregion(&creg)) != TRUE) {
        *curwp = orig_wp;       /* restore original struct */
        return(status);
    }
    curwp->w_dotp = creg.r_linep;   /* only by full lines please! */
    curwp->w_doto = 0;
    creg.r_size += (long)creg.r_offset;

/* Might no longer be possible for this to happen because we now move
 * to the start of next line after the later of mark/doto.
 * But leave it here just in case...
 */
    if (creg.r_size <= (long)curwp->w_dotp->l_used) {
        mlwrite("Must narrow at least 1 full line");
        *curwp = orig_wp;       /* restore original struct */
        return(FALSE);
    }

/* Archive the top fragment */
    if (bp->b_linep->l_fp != creg.r_linep) {
        bp->b_topline = bp->b_linep->l_fp;
        creg.r_linep->l_bp->l_fp = (struct line *)NULL;
        bp->b_linep->l_fp = creg.r_linep;
        creg.r_linep->l_bp = bp->b_linep;
    }

/* Move forward to the end of this region (a long number of bytes perhaps) */
    while (creg.r_size > (long)32000) {
        forw_grapheme(32000);
        creg.r_size -= (long)32000;
    }
    forw_grapheme((int)creg.r_size);
    curwp->w_doto = 0;              /* only full lines! */

/* Archive the bottom fragment */
    if (bp->b_linep != curwp->w_dotp) {
        bp->b_botline = curwp->w_dotp;
        bp->b_botline->l_bp->l_fp = bp->b_linep;
        bp->b_linep->l_bp->l_fp = (struct line *)NULL;
        bp->b_linep->l_bp = bp->b_botline->l_bp;
    }

/* Let all the proper windows be updated */
    wp = wheadp;
    while (wp) {
        if (wp->w_bufp == bp) {
            wp->w_linep = creg.r_linep;
            wp->w_dotp = creg.r_linep;
            wp->w_doto = 0;
            wp->w_markp = creg.r_linep;
            wp->w_marko = 0;
            wp->w_flag |= (WFHARD|WFMODE);
        }
        wp = wp->w_wndp;
    }

/* And now remember we are narrowed */
    bp->b_flag |= BFNAROW;
    mlwrite(MLpre "Buffer is narrowed" MLpost);
    return(TRUE);
}

/* widen-from-region (^X->) restores a narrowed region     */

int widen(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *lp;       /* temp line pointer */
    struct buffer *bp;     /* buffer being narrowed */
    struct window *wp;     /* windows to fix up pointers in as well */

/* Find the proper buffer and make sure we are narrow */
    bp = curwp->w_bufp;     /* find the right buffer */
    if ((bp->b_flag&BFNAROW) == 0) {
        mlwrite("This buffer is not narrowed");
        return(FALSE);
    }

/* Recover the top fragment */
    if (bp->b_topline != (struct line *)NULL) {
        lp = bp->b_topline;
        while (lp->l_fp != (struct line *)NULL) lp = lp->l_fp;
        lp->l_fp = bp->b_linep->l_fp;
        lp->l_fp->l_bp = lp;
        bp->b_linep->l_fp = bp->b_topline;
        bp->b_topline->l_bp = bp->b_linep;
        bp->b_topline = (struct line *)NULL;
    }

/* Recover the bottom fragment */
    if (bp->b_botline != (struct line *)NULL) {
        lp = bp->b_botline;
        while (lp->l_fp != (struct line *)NULL) lp = lp->l_fp;
        lp->l_fp = bp->b_linep;
        bp->b_linep->l_bp->l_fp = bp->b_botline;
        bp->b_botline->l_bp = bp->b_linep->l_bp;
        bp->b_linep->l_bp = lp;
        bp->b_botline = (struct line *)NULL;
    }

/* Let all the proper windows be updated */
    wp = wheadp;
    while (wp) {
        if (wp->w_bufp == bp) wp->w_flag |= (WFHARD|WFMODE);
        wp = wp->w_wndp;
    }
/* And now remember we are not narrowed */
    bp->b_flag &= (~BFNAROW);

/* Note the widening and redraw with the start of the ex-narrowed
 * region in middle of screen.
 * Unless we were called with a negative n.
 */
    mlwrite(MLpre "Buffer is widened" MLpost);
    if (n < 0) {    /* Allow message to be read, but no reposition */
        sleep(1);
    }
    else {
        reposition(FALSE, 0);
    }
    return(TRUE);
}
