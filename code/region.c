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
    long fsize;
    long bsize;

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
            rp->r_bytes = (long) (curwp->w.marko - curwp->w.doto);
        }
        else {
            rp->r_offset = curwp->w.marko;
            rp->r_bytes = (long) (curwp->w.doto - curwp->w.marko);
        }
        rp->r_endp = curwp->w.dotp;
        return TRUE;
    }

    blp = curwp->w.dotp;
    bsize = (long) curwp->w.doto;
    flp = curwp->w.dotp;
    fsize = (long) (lused(flp) - curwp->w.doto + 1);
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

/* These work as a pair.
 * get_marker_columns runs through the entries and
 * sets col_offset to the grapheme count of the position.
 * This is called before casechanging.
 * set_marker_offsets runs through the entries and
 * sets offset based on the col_offset count of the position.
 * This is called after casechanging and will deal with any byte-length
 * changing done by the casechange.
 */
static int sysmark_col_offset, mark_col_offset, dot_col_offset;
static void get_marker_columns(void) {
    for(linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
        mmi(mp, col_offset) = glyphcount_utf8_array(ltext(mmi(mp, lp)),
             mmi(mp, offset));
    }
    sysmark_col_offset = glyphcount_utf8_array(ltext(sysmark.p), sysmark.o);
    mark_col_offset = glyphcount_utf8_array(ltext(curwp->w.markp),
         curwp->w.marko);
    dot_col_offset = glyphcount_utf8_array(ltext(curwp->w.dotp), curwp->w.doto);
}
static int offset_for_col(char *text, int col, int maxlen) {
    int offs = 0;
    while(col--) offs = next_utf8_offset(text, offs, maxlen, TRUE);
    return offs;
}
static void set_marker_offsets(void) {
    for(linked_items *mp = macro_pin_headp; mp; mp = mp->next) {
        mmi(mp, offset) = offset_for_col(ltext(mmi(mp, lp)),
             mmi(mp, col_offset), lused(mmi(mp, lp)));
    }

    sysmark.o = offset_for_col(ltext(sysmark.p), lused(sysmark.p),
     sysmark_col_offset);

    curwp->w.marko = offset_for_col(ltext(curwp->w.markp),
     lused(curwp->w.markp), mark_col_offset);

    curwp->w.doto = offset_for_col(ltext(curwp->w.dotp),
     lused(curwp->w.dotp), dot_col_offset);
}

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

/* For dot and marks we know that they cannot be within the region so
 * they either stay still, or move by the full amount.
 * Pins are not the same, as they can be anywhere
 * So we need to get them to determine their *column* offset and
 * restore it after the change has been done.
 * We just get the columns for all at the start, then reset the columns
 * for all at the end, regardless of which buffer/line they are in.
 */
    get_marker_columns();

    for (int b_offs = region.r_offset; region.r_bytes > 0;
             linep = lforw(linep)) {
        int b_end = region.r_bytes + b_offs;
        if (b_end > lused(linep)) b_end = lused(linep);
        int this_blen = b_end - b_offs;
        utf8_recase(newcase, ltext(linep)+b_offs, this_blen, &mstr);
        int replen = mstr.utf8c;            /* Less code when copied.. */
        char *repstr = mstr.str;            /* ...to simple local vars */

        if (replen <= this_blen) {          /* Guaranteed the space */
            memcpy(ltext(linep)+b_offs, repstr, replen);
            if (replen < this_blen) {       /* Fix up the shortening */
                ccr_Tail_Copy;
                int b_less = this_blen - replen;
                lused(linep) -= b_less;     /* Fix-up length */
            }
        }
        else {              /* replen > this_blen  Potentially trickier */
            int b_more = replen - this_blen;
/* If we don't have the space, get some more */
            if ((lsize(linep) - lused(linep)) < b_more) {   /* Need more */
                ltextgrow(linep, b_more);
            }
            ccr_Tail_Copy;  /* Must move the tail out-of-the-way first!! */
            memcpy(ltext(linep)+b_offs, repstr, replen);
            lused(linep) += b_more;     /* Fix-up length */
        }
        Xfree(repstr);      /* Used this now (== mstr.str), so free */
        region.r_bytes -= this_blen + 1;
        b_offs = 0;
    }

    set_marker_offsets();

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
