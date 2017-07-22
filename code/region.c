/*      region.c
 *
 *      The routines in this file deal with the region, that magic space
 *      between "." and mark. Some functions are commands. Some functions are
 *      just for internal use.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/*
 * Kill the region. Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 */
int killregion(int f, int n)
{
        int s;
        struct region region;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
                return rdonly();        /* we are in read only mode     */
        if ((s = getregion(&region)) != TRUE)
                return s;
        if ((lastflag & CFKILL) == 0)   /* This is a kill type  */
                kdelete();      /* command, so do magic */
        thisflag |= CFKILL;     /* kill buffer stuff.   */
        curwp->w_dotp = region.r_linep;
        curwp->w_doto = region.r_offset;
        return ldelete(region.r_size, TRUE);
}

/*
 * Copy all of the characters in the
 * region to the kill buffer. Don't move dot
 * at all. This is a bit like a kill region followed
 * by a yank. Bound to "M-W".
 */
int copyregion(int f, int n)
{
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
                        if ((s = kinsert('\n')) != TRUE)
                                return s;
                        linep = lforw(linep);
                        loffs = 0;
                } else {                /* Middle of line.      */
                        if ((s = kinsert(lgetc(linep, loffs))) != TRUE)
                                return s;
                        ++loffs;
                }
        }
        mlwrite(MLpre "region copied" MLpost);
        return TRUE;
}

/*
 * Lower case region. Zap all of the upper
 * case characters in the region to lower case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
int lowerregion(int f, int n)
{
        struct line *linep;
        int loffs;
        int c;
        int s;
        struct region region;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if ((s = getregion(&region)) != TRUE)
                return s;
        lchange(WFHARD);
        linep = region.r_linep;
        loffs = region.r_offset;
        while (region.r_size--) {
                if (loffs == llength(linep)) {
                        linep = lforw(linep);
                        loffs = 0;
                } else {
                        c = lgetc(linep, loffs);
                        if (c >= 'A' && c <= 'Z')
                                lputc(linep, loffs, c + 'a' - 'A');
                        ++loffs;
                }
        }
        return TRUE;
}

/*
 * Upper case region. Zap all of the lower
 * case characters in the region to upper case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
int upperregion(int f, int n)
{
        struct line *linep;
        int loffs;
        int c;
        int s;
        struct region region;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if ((s = getregion(&region)) != TRUE)
                return s;
        lchange(WFHARD);
        linep = region.r_linep;
        loffs = region.r_offset;
        while (region.r_size--) {
                if (loffs == llength(linep)) {
                        linep = lforw(linep);
                        loffs = 0;
                } else {
                        c = lgetc(linep, loffs);
                        if (c >= 'a' && c <= 'z')
                                lputc(linep, loffs, c - 'a' + 'A');
                        ++loffs;
                }
        }
        return TRUE;
}

/*
 * This routine figures out the
 * bounds of the region in the current window, and
 * fills in the fields of the "struct region" structure pointed
 * to by "rp". Because the dot and mark are usually very
 * close together, we scan outward from dot looking for
 * mark. This should save time. Return a standard code.
 * Callers of this routine should be prepared to get
 * an "ABORT" status; we might make this have the
 * conform thing later.
 */
int getregion(struct region *rp)
{
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
                        rp->r_size =
                            (long) (curwp->w_marko - curwp->w_doto);
                } else {
                        rp->r_offset = curwp->w_marko;
                        rp->r_size =
                            (long) (curwp->w_doto - curwp->w_marko);
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

/*      Narrow-to-region (^X-<) makes all but the current region in
        the current buffer invisable and unchangable
*/

int narrow(int f, int n)
{
        int status;             /* return status */
        struct buffer *bp;      /* buffer being narrowed */
        struct window *wp;      /* windows to fix up pointers in as well */
        struct region creg;     /* region boundry structure */

        /* find the proper buffer and make sure we aren't already narrow */
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

        /* archive the top fragment */
        if (bp->b_linep->l_fp != creg.r_linep) {
                bp->b_topline = bp->b_linep->l_fp;
                creg.r_linep->l_bp->l_fp = (struct line *)NULL;
                bp->b_linep->l_fp = creg.r_linep;
                creg.r_linep->l_bp = bp->b_linep;
        }

        /* move forward to the end of this region
           (a long number of bytes perhaps) */
        while (creg.r_size > (long)32000) {
                forw_grapheme(32000);
                creg.r_size -= (long)32000;
        }
        forw_grapheme((int)creg.r_size);
        curwp->w_doto = 0;              /* only full lines! */

        /* archive the bottom fragment */
        if (bp->b_linep != curwp->w_dotp) {
                bp->b_botline = curwp->w_dotp;
                bp->b_botline->l_bp->l_fp = bp->b_linep;
                bp->b_linep->l_bp->l_fp = (struct line *)NULL;
                bp->b_linep->l_bp = bp->b_botline->l_bp;
        }

        /* let all the proper windows be updated */
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

        /* and now remember we are narrowed */
        bp->b_flag |= BFNAROW;
        mlwrite(MLpre "Buffer is narrowed" MLpost);
        return(TRUE);
}

/*      widen-from-region (^X->) restores a narrowed region     */

int widen(int f, int n)
{
        struct line *lp;       /* temp line pointer */
        struct buffer *bp;     /* buffer being narrowed */
        struct window *wp;     /* windows to fix up pointers in as well */

        /* find the proper buffer and make sure we are narrow */
        bp = curwp->w_bufp;     /* find the right buffer */
        if ((bp->b_flag&BFNAROW) == 0) {
                mlwrite("This buffer is not narrowed");
                return(FALSE);
        }

        /* recover the top fragment */
        if (bp->b_topline != (struct line *)NULL) {
                lp = bp->b_topline;
                while (lp->l_fp != (struct line *)NULL)
                        lp = lp->l_fp;
                lp->l_fp = bp->b_linep->l_fp;
                lp->l_fp->l_bp = lp;
                bp->b_linep->l_fp = bp->b_topline;
                bp->b_topline->l_bp = bp->b_linep;
                bp->b_topline = (struct line *)NULL;
        }

        /* recover the bottom fragment */
        if (bp->b_botline != (struct line *)NULL) {
                lp = bp->b_botline;
                while (lp->l_fp != (struct line *)NULL)
                        lp = lp->l_fp;
                lp->l_fp = bp->b_linep;
                bp->b_linep->l_bp->l_fp = bp->b_botline;
                bp->b_botline->l_bp = bp->b_linep->l_bp;
                bp->b_linep->l_bp = lp;
                bp->b_botline = (struct line *)NULL;
        }

        /* let all the proper windows be updated */
        wp = wheadp;
        while (wp) {
                if (wp->w_bufp == bp)
                        wp->w_flag |= (WFHARD|WFMODE);
                wp = wp->w_wndp;
        }
        /* and now remember we are not narrowed */
        bp->b_flag &= (~BFNAROW);
        mlwrite(MLpre "Buffer is widened" MLpost);
/* Redraw with the start of the ex-narrowed region in middle of screen */
        reposition(FALSE, 0);
        return(TRUE);
}
