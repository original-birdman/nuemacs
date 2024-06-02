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

#define REGION_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

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
    ue64I_t fsize;
    ue64I_t bsize;

    if (curwp->w.markp == NULL) {
        mlwrite_one("No mark set in this window");
        return FALSE;
    }
/* Handle the case of mark and dot being on the same line, as
 * the loop below does not.
 */
    if (curwp->w.dotp == curwp->w.markp) {
        rp->r_linep = curwp->w.dotp;
        if (curwp->w.doto < curwp->w.marko) {
            rp->r_offset = curwp->w.doto;
            rp->r_foffset = curwp->w.marko;
            rp->r_bytes = (ue64I_t) (curwp->w.marko - curwp->w.doto);
        }
        else {
            rp->r_offset = curwp->w.marko;
            rp->r_foffset = curwp->w.doto;
            rp->r_bytes = (ue64I_t) (curwp->w.doto - curwp->w.marko);
        }
        rp->r_endp = curwp->w.dotp;
        return TRUE;
    }

    blp = curwp->w.dotp;
    bsize = (ue64I_t) curwp->w.doto;
    flp = curwp->w.dotp;
    fsize = (ue64I_t) (lused(flp) - curwp->w.doto + 1);
/* We start at dot and look for the mark, spreading out... */
    while (flp != curbp->b_linep || lback(blp) != curbp->b_linep) {
        if (flp != curbp->b_linep) {
            flp = lforw(flp);   /* Look for mark after dot */
            if (flp == curwp->w.markp) {
                rp->r_linep = curwp->w.dotp;
                rp->r_offset = curwp->w.doto;
                rp->r_bytes = fsize + curwp->w.marko;
                rp->r_endp = curwp->w.markp;
                rp->r_foffset = curwp->w.marko;
                return TRUE;
             }
             fsize += lused(flp) + 1;
         }
         if (lback(blp) != curbp->b_linep) {
            blp = lback(blp);   /* Look for mark before dot */
            bsize += lused(blp) + 1;
            if (blp == curwp->w.markp) {
                rp->r_linep = blp;
                rp->r_offset = curwp->w.marko;
                rp->r_bytes = bsize - curwp->w.marko;
                rp->r_endp = curwp->w.dotp;
                rp->r_foffset = curwp->w.doto;
                return TRUE;
            }
        }
    }
    mlwrite_one("Bug: lost mark");
    return FALSE;
}

/* Kill the region.
 * Ask "getregion" to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 * If given and arg of 2, don't put the text killed onto the kill ring.
 */
int killregion(int f, int n) {
    UNUSED(f);
    int s;
    struct region region;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
        return rdonly();        /* we are in read only mode     */
    if ((s = getregion(&region)) != TRUE)
        return s;
/* If the last command was a yank we don't want to change the kill-ring
 * (the text is what is already on the top, or the last minibuffer).
 */
    int save_to_kill_ring;
    if (n == 2) {
        save_to_kill_ring = FALSE;
        com_flag = CFNONE;  /* This is NOT a kill */
    }
    else {
        save_to_kill_ring = (com_flag & CFYANK)? FALSE: TRUE;
        com_flag = CFKILL;  /* This is a kill */
    }
    if (save_to_kill_ring) kdelete();   /* kill-type command, so do magic */
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
    if ((com_flag & CFKILL) == 0) { /* Kill type command.   */
        kdelete();
        com_flag |= CFKILL;
    }
    linep = region.r_linep;         /* Current line.        */
    loffs = region.r_offset;        /* Current offset.      */
/* This copies bytes - doesn't need to know about graphemes. */
    while (region.r_bytes--) {
        if (loffs == lused(linep)) {  /* End of line. */
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
    if (b_end < lused(linep)) {   /* Shuffle chars along... */ \
        memmove(ltext(linep)+b_offs+replen, \
            ltext(linep)+b_end, \
            lused(linep)-b_end); \
    }

static int casechange_region(int newcase) { /* The handling function */
    struct region region;
    int s;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();            /* we are in read only mode     */
    if ((s = getregion(&region)) != TRUE) return s;

    lchange(WFHARD);                /* Marks buffer as changed */

/* Save where we are, then move us to the start of the region (we might
 * be at the end).
 */
    sysmark.p = curwp->w.dotp;
    sysmark.o = curwp->w.doto;
    curwp->w.dotp = region.r_linep;
    curwp->w.doto = region.r_offset;

/* Now move along grapheme-by-grapheme ensuring we have the required case.
 * ensure_case() will update pins, mark and sysmark, if needed, as we
 * move along.
 * We have to adjust r_foffset on the final line by any byte-length change
 * resulting from ensure_case().
 */
    while((curwp->w.dotp != region.r_endp) ||
          (curwp->w.doto < region.r_foffset)) {
        int bc = ensure_case(newcase);
        if (curwp->w.dotp == region.r_endp) region.r_foffset += bc;
        forw_grapheme(1);
    }

/* Now restore us to where we were */
    curwp->w.dotp = sysmark.p;
    curwp->w.doto = sysmark.o;

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

/* GGR extensions to regions */

/* Narrow-to-region (^X-<) makes all but the current region in
 * the current buffer invisible and unchangeable
 */
int narrow(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;             /* return status */

/* Avoid "may be used uninitialized" in gcc4.4.7 */
    struct region creg = creg;  /* region boundry structure */

/* Find the proper buffer and make sure we aren't already narrow */
    struct buffer *bp = curwp->w_bufp;  /* find the right buffer */
    if (bp->b_flag&BFNAROW) {
        mlwrite_one("This buffer is already narrowed");
        return(FALSE);
    }

/* Avoid complications later by checking for an buffer empty now. */
    if (bp->b_linep->l_fp == bp->b_linep) {
        mlwrite_one("You cannot narrow an empty buffer");
        return(FALSE);
    }

/* Find the boundaries of the current region */
    struct window orig_wp = *curwp;         /* Copy original struct */
    if ((status = getregion(&creg)) != TRUE) {
        *curwp = orig_wp;       /* restore original struct */
        return(status);
    }

/* We now have a region, but take it to be "whole lines" (so ignoring
 * doto and marko).
 * Top fragment is up to r_linep.
 * Bottom fragment starts at lforw(r_endp) (or possibly r_endp).
 */

/* Archive the top fragment - marking its end (for widen) with a NULL l_fp.
 * If this starts at line 1 there is nothing to do (b_topline stays NULL).
 * Otherwise, save the first line of the top fragment, make the top of the
 * narrowed region the new top and fix up pointers.
 */
    if (bp->b_linep->l_fp != creg.r_linep) {    /* Not on line 1 */
        bp->b_topline = bp->b_linep->l_fp;      /* Save start of top */
        creg.r_linep->l_bp->l_fp = NULL;        /* Mark end of top */
        bp->b_linep->l_fp = creg.r_linep;       /* Region top -> top */
        creg.r_linep->l_bp = bp->b_linep;       /* Back ptr to head */
    }

/* We now want to set curwp->w.dotp to be the first line of the bottom
 * section to hive off.
 * A region knows its last line, so just set the dotp to the start of
 * the next line - *unless* we that is the start of a line which is *not*
 * the top line of what is left - in which case use the current line
 * (this also caters for being on the final line).
 */
    if ((creg.r_foffset == 0) && (bp->b_linep->l_fp != creg.r_endp)) {
         curwp->w.dotp = creg.r_endp;
    }
    else {
        curwp->w.dotp = lforw(creg.r_endp);
        curwp->w.doto = 0;
    }

/* Archive the bottom fragment - marking its end (for widen) with a NULL l_fp.
 * If this starts at the "dummy" last line there is nothing to do (b_botline
 * stays NULL).
 * Otherwise, save the first line of the bottom fragment, mark the end of
 * the narrowed region as the end of a buffer and fix up pointers.
 * NOTE. that bp->b_botline is the *start* of the bottom section (the first
 * line qwe will *not* show) so bp->b_botline->l_bp is the last line which
 * we will show.
 */
    if (bp->b_linep != curwp->w.dotp) {         /* Not on last line */
        bp->b_botline = curwp->w.dotp;          /* Save start of bottom */
        bp->b_botline->l_bp->l_fp = bp->b_linep;    /* Mark end of region */
        bp->b_linep->l_bp->l_fp = NULL;         /* Mark end of bottom */
        bp->b_linep->l_bp = bp->b_botline->l_bp;    /* Back ptr from head */
    }

/* Let all the proper windows be updated with dot and mark both
 * on the first line (and the first window line) of the narrowed buffer.
 */
    for (struct window *wp = wheadp; wp; wp = wp->w_wndp) {
        if (wp->w_bufp == bp) {
            wp->w_linep = wp->w.dotp = wp->w.markp = creg.r_linep;
            wp->w.doto = wp->w.marko = 0;
            wp->w_flag |= (WFHARD|WFMODE);
        }
    }

/* And now remember we are narrowed */
    bp->b_flag |= BFNAROW;
    mlwrite_one(MLbkt("Buffer is narrowed"));
    return(TRUE);
}

/* widen-from-region (^X->) restores a narrowed region     */

int widen(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *lp;

/* Find the proper buffer and make sure we are narrow */
    struct buffer *bp = curwp->w_bufp;     /* find the right buffer */
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
    for (struct window *wp = wheadp; wp; wp = wp->w_wndp) {
        if (wp->w_bufp == bp) wp->w_flag |= (WFHARD|WFMODE);
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
