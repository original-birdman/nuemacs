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
    curwp->w.dotp = region.r_linep;
    curwp->w.doto = region.r_offset;
    return ldelete(region.r_bytes, save_to_kill_ring);
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
/* This copies bytes - doesn't need to know about graphemes. */
    while (region.r_bytes--) {
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
    mlwrite_one(MLbkt("region copied"));
    return TRUE;
}

/* Change case of region to the given newcase.
 * unicode(utf8) aware.
 * Zap all of the lower case characters in the region to upper case.
 * Use the region code to set the limits, then work through each line
 * in order re-casing the text on that line which needs to be changed.
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
 * They can only be at the start or end of the moved chars (as the
 * region we are case-changing is that between dot and mark!) so
 * we only have to consider those case, not anythign in between.
 * If at the start there is nothing to do, as that hasn't moved.
 * Turns out a macro is more efficient (in terms of executable size)
 * than a function...
 */
#define MarkDotFixup(amount) \
    if ((sysmark.p == linep) && \
        (sysmark.o == (b_offs + this_blen))) \
          sysmark.o += amount; \
    if ((curwp->w.markp == linep) && \
        (curwp->w.marko == (b_offs + this_blen))) \
          curwp->w.marko += amount; \
    if ((curwp->w.dotp == linep) && \
        (curwp->w.doto == (b_offs + this_blen))) \
          curwp->w.doto += amount

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
    for (int b_offs = region.r_offset; region.r_bytes > 0;
             linep = lforw(linep)) {
        int b_end = region.r_bytes + b_offs;
        if (b_end > llength(linep)) b_end = llength(linep);
        int this_blen = b_end - b_offs;
        utf8_recase(newcase, linep->l_text+b_offs, this_blen, &mstr);
        int replen = mstr.utf8c;            /* Less code when copied.. */
        char *repstr = mstr.str;            /* ...to simple local vars */
        if (replen <= this_blen) {          /* Guaranteed the space */
            memcpy(linep->l_text+b_offs, repstr, replen);
            if (replen < this_blen) {       /* Fix up the shortening */
                ccr_Tail_Copy;
                int b_less = this_blen - replen;
                llength(linep) -= b_less;   /* Fix-up length */
                MarkDotFixup(-b_less);
            }
        }
        else {              /* replen > this_blen  Potentially trickier */
            int b_more = replen - this_blen;
/* If we don't have the space, get some more */
            if ((linep->l_size - linep->l_used) < b_more) { /* Need more */
                ltextgrow(linep, b_more);
            }
            ccr_Tail_Copy;  /* Must move the tail out-of-the-way first!! */
            memcpy(linep->l_text+b_offs, repstr, replen);
            llength(linep) += b_more;   /* Fix-up length */
            MarkDotFixup(b_more);
        }
        Xfree(repstr);      /* Used this now (== mstr.str), so free */
        region.r_bytes -= this_blen + 1;
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
 */
int getregion(struct region *rp) {
    struct line *flp;
    struct line *blp;
    long fsize;
    long bsize;

    if (curwp->w.markp == NULL) {
        mlwrite_one("No mark set in this window");
        return FALSE;
    }
    if (curwp->w.dotp == curwp->w.markp) {
        rp->r_linep = curwp->w.dotp;
        if (curwp->w.doto < curwp->w.marko) {
            rp->r_offset = curwp->w.doto;
            rp->r_bytes = (long) (curwp->w.marko - curwp->w.doto);
        }
        else {
            rp->r_offset = curwp->w.marko;
            rp->r_bytes = (long) (curwp->w.doto - curwp->w.marko);
        }
        return TRUE;
    }
    blp = curwp->w.dotp;
    bsize = (long) curwp->w.doto;
    flp = curwp->w.dotp;
    fsize = (long) (llength(flp) - curwp->w.doto + 1);
/* We start at dot and look for the mark, spreading out... */
    while (flp != curbp->b_linep || lback(blp) != curbp->b_linep) {
        if (flp != curbp->b_linep) {
            flp = lforw(flp);   /* Look for mark after dot */
            if (flp == curwp->w.markp) {
                rp->r_linep = curwp->w.dotp;
                rp->r_offset = curwp->w.doto;
                rp->r_bytes = fsize + curwp->w.marko;
                rp->r_endp = curwp->w.markp;
                return TRUE;
             }
             fsize += llength(flp) + 1;
         }
         if (lback(blp) != curbp->b_linep) {
            blp = lback(blp);   /* Look for mark before dot */
            bsize += llength(blp) + 1;
            if (blp == curwp->w.markp) {
                rp->r_linep = blp;
                rp->r_offset = curwp->w.marko;
                rp->r_bytes = bsize - curwp->w.marko;
                rp->r_endp = curwp->w.dotp;
                return TRUE;
            }
        }
    }
    mlwrite_one("Bug: lost mark");
    return FALSE;
}

/* GGR extensions to regions */

/* Narrow-to-region (^X-<) makes all but the current region in
 * the current buffer invisible and unchangeable
 */
int narrow(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;             /* return status */
    struct buffer *bp;      /* buffer being narrowed */
    struct window *wp;      /* windows to fix up pointers in as well */
/* Avoid "may be used uninitialized" in gcc4.4.7 */
    struct region creg = creg;  /* region boundry structure */

/* Find the proper buffer and make sure we aren't already narrow */
    bp = curwp->w_bufp;     /* find the right buffer */
    if (bp->b_flag&BFNAROW) {
        mlwrite_one("This buffer is already narrowed");
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
        if (lp == curwp->w.dotp) {          /* doto */
            fix_up = 1;
            break;
        }
        else if (lp == curwp->w.markp) {    /* mark */
            fix_up = -1;
            break;
        }
        lp = lforw(lp);
    }
    if ((fix_up == 1) && (curwp->w.marko > 0)) {
        curwp->w.markp = lforw(curwp->w.markp);
        curwp->w.marko = 0;
    }
    else if ((fix_up == -1) && (curwp->w.doto > 0)) {
        curwp->w.dotp = lforw(curwp->w.dotp);
        curwp->w.doto = 0;
    }

/* Find the boundaries of the current region */
    if ((status = getregion(&creg)) != TRUE) {
        *curwp = orig_wp;       /* restore original struct */
        return(status);
    }
    curwp->w.dotp = creg.r_linep;   /* only by full lines please! */
    curwp->w.doto = 0;
    creg.r_bytes += (long)creg.r_offset; /* Add in what we've added */

/* Might no longer be possible for this to happen because we now move
 * to the start of next line after the later of mark/doto.
 * Actually it is possible.
 * Just set a mark at the beginning of a line then narrow.
 */
    if (creg.r_bytes <= (long)curwp->w.dotp->l_used) {
        mlwrite_one("Must narrow at least 1 full line");
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

/* A region know knows its last line, so just set the curwp to this
 * with a 0 offset.
 */
    curwp->w.dotp = creg.r_endp;
    curwp->w.doto = 0;              /* only full lines! */

/* Archive the bottom fragment */
    if (bp->b_linep != curwp->w.dotp) {
        bp->b_botline = curwp->w.dotp;
        bp->b_botline->l_bp->l_fp = bp->b_linep;
        bp->b_linep->l_bp->l_fp = (struct line *)NULL;
        bp->b_linep->l_bp = bp->b_botline->l_bp;
    }

/* Let all the proper windows be updated */
    wp = wheadp;
    while (wp) {
        if (wp->w_bufp == bp) {
            wp->w_linep = creg.r_linep;
            wp->w.dotp = creg.r_linep;
            wp->w.doto = 0;
            wp->w.markp = creg.r_linep;
            wp->w.marko = 0;
            wp->w_flag |= (WFHARD|WFMODE);
        }
        wp = wp->w_wndp;
    }

/* And now remember we are narrowed */
    bp->b_flag |= BFNAROW;
    mlwrite_one(MLbkt("Buffer is narrowed"));
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
        mlwrite_one("This buffer is not narrowed");
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

/* If we are on the last line of the narrowed section (the "phantom",
 * empty one) then we need to set the start of any bottom segment as
 * the current position. Otherwise we go to the end of the widened buffer.
 */
        if (curwp->w.dotp == curbp->b_linep) {
            curwp->w.dotp = bp->b_botline;
        }

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
    mlwrite_one(MLbkt("Buffer is widened"));
    if (n < 0) {    /* Allow message to be read, but no reposition */
        sleep(1);
    }
    else {
        reposition(FALSE, 0);
    }
    return(TRUE);
}
