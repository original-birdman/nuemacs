/*      word.c
 *
 *      The routines in this file implement commands that work word or a
 *      paragraph at a time.  There are all sorts of word mode commands.  If I
 *      do any sentence mode commands, they are likely to be put in this file.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>

#define WORD_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "utf8proc.h"
#include "utf8.h"

/* This is set by inword when a call tests a grapheme with a
 * zero-width work-break attached.
 */
static int zw_break = 0;

/* Utility routine to determine whether the current char is part of
 * some given classes, and what to return on a zero-width break.
 * called via defines (in efunc.h)
 *
 * inword -> (wp, "LN", FALSE)
 *      so is looking for Letters and Numbers
 * at_abreak -> (wp, "ZC", TRUE)
 *      so is looking for Separators and Others.
 *
 * The full set of Class characters (1st char only) are:
 *  C   Others
 *  L   Letters
 *  M   Marks
 *  N   Numbers
 *  P   Punctuation
 *  S   Symbols
 *  Z   Separators
 */
int class_check(struct inwbuf *inwp, char *classes, int res_on_zwb) {
    struct grapheme gc;
    struct line *mylp;
    int myoffs;

/* If not sent an lbuf, use current position */
    if (inwp) {
        mylp = inwp->lp;
        myoffs = inwp->offs;
    }
    else {
        mylp = curwp->w.dotp;
        myoffs = curwp->w.doto;
    }

    zw_break = 0;
    if (myoffs == lused(mylp)) {    /* Handle end of line */
        gc.uc = '\n';
        if (inwp) {                 /* Switch caller to next one */
            inwp->offs = 0;
            inwp->lp = lforw(mylp);
        }
    }
    else {
/* Don't build any ex... */
        myoffs =
             build_next_grapheme(ltext(mylp), myoffs, lused(mylp), &gc, 1);
        if (inwp) inwp->offs = myoffs;
        if (gc.uc == 0x200B && gc.cdm == 0) {   /* NOT a combining char */
            zw_break = 1;
            return res_on_zwb;
        }
    }

/* We only look at the base character to determine whether this is a
 * word character.
 */
    const char *uc_class =
         utf8proc_category_string((utf8proc_int32_t)gc.uc);
    for (char *mp = classes; *mp; mp++)
        if (uc_class[0] == *mp) return TRUE;
    return FALSE;
}

/* Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "back_grapheme" routine.
 * This always ends up at the previous start of a word.
 * Error if you try to move beyond the start of the buffer...BUT, since
 * the mechanism involves going 1 char too far (to get "out" of the word
 * then stepping forward one, we must handle *arriving* at BOF while
 * looking for the "before word" char correctly.
 */
int forwword(int, int);     /* Forward declaration */
int backword(int f, int n) {
/* Have to go back one at the start it case we are looking at the
 * first char of a word.
 */
    if (n < 0) return forwword(f, -n);
    if (back_grapheme(1) <= 0) return FALSE;
    while (n--) {
        while (inword(NULL) == FALSE)   /* Get to a word if on whitespace */
            if (back_grapheme(1) <= 0) return FALSE;
        do {    /* Must be inword to get here, so move then check */
            if (back_grapheme(1) <= 0) {
/* If this is because we are at b-of-file then we are at the the first
 * word and have got there successfully.
 */
                if ((curwp->w.doto == 0) &&
                    (lback(curwp->w.dotp) == curwp->w_bufp->b_linep))
                    return TRUE;
                else return FALSE;
            }
        } while (inword(NULL));
    }
    return (forw_grapheme(1) > 0);  /* Success count => T/F */
}

/* Move the cursor forward by the specified number of words. All of the motion
 * is done by "forw_grapheme".
 * Error if you try and move beyond the buffer's end.
 */
int forwword(int f, int n) {
    if (n < 0) return backword(f, -n);
    while (n--) {
/* GGR - reverse loop order according to ggr-style state
 * Determines whether you end up at the end of the current word (ggr-style)
 * or the start of next.
 * In GGR style we skip over any non-word chars, then over word chars
 * and so end at the end of the next word.
 * In non-GGR style we skip over any word chars, then over non-word chars
 * and so end at the start of the next word.
 */
        int state1 = (ggr_opts&GGR_FORWWORD)? FALSE: TRUE;
        while (inword(NULL) == state1)
            if (forw_grapheme(1) <= 0) return FALSE;
        do {    /* Can't be in state1 to get here, so move then check */
            if (forw_grapheme(1) <= 0) return FALSE;
        }
        while (inword(NULL) != state1);
    }
    return TRUE;
}

/* GGR
 * Force the case of the current character (or the main character of
 * a multi-char grapheme) to be a particular case.
 * For use by upper/lower/cap-word() and equiv.
 */
void ensure_case(int want_case) {

    utf8proc_int32_t (*caser)(utf8proc_int32_t);
/* Set the case-mapping handler we wish to use */

    switch (want_case) {
    case UTF8_UPPER:
        caser = utf8proc_toupper;
        break;
    case UTF8_LOWER:
        caser = utf8proc_tolower;
        break;
    case UTF8_TITLE:
        caser = utf8proc_totitle;
        break;
    default:
        return;
    }

    int saved_doto = curwp->w.doto;     /* Save position */
    struct grapheme gc;                 /* Have to free any gc.ex we get! */
    int orig_utf8_len = lgetgrapheme(&gc, FALSE);   /* Doesn't move doto */
/* We only look at the base character for casing.
 * If it's not changed by the caser(), leave now...
 * Although we will (try to) handle the perverse case of a German sharp.
 * If we have a bare ß and are uppercasing, insert "SS" instead...
 * Since utf8_recase() in utf8.c does this.
 *
 * If we have something to do we insert the new character first (so
 * that any mark at the point gets updated correctly by linsert_*())
 * then delete the old char, then restore doto (possibly adjusted).
 */
    int doto_adj = 0;
    if ((want_case == UTF8_UPPER) &&    /* Bare German sharp? */
        (gc.uc == 0x00df) && (gc.cdm == 0)) {
        linsert_byte(2, 'S');           /* Inserts "SS" */
        orig_utf8_len = 2;
        doto_adj = 1;
    }
    else {
        unicode_t nuc = caser(gc.uc);   /* Get the case-translated uc */
        if (nuc == gc.uc) goto ensure_ex_free;  /* No change */
        int start = curwp->w.doto;
        gc.uc = nuc;
        lputgrapheme(&gc);              /* Insert the whole thing */
        int new_utf8_len = curwp->w.doto - start;   /* Bytes added */

/* If either mark is on this line we may have to update it to reflect
 * any change in byte count
 */
        if (new_utf8_len != orig_utf8_len) {        /* Byte count changed */
            if ((curwp->w.markp == curwp->w.dotp) &&    /* Same line... */
                (curwp->w.marko > curwp->w.doto)) {     /* and beyond dot? */
                 curwp->w.markp += (new_utf8_len - orig_utf8_len);
            }
            if ((sysmark.p == curwp->w.dotp) &&     /* Same line... */
                (sysmark.o > curwp->w.doto)) {      /* and beyond dot? */
                 sysmark.p += (new_utf8_len - orig_utf8_len);
            }
        }
    }
    ldelete(orig_utf8_len, FALSE);
    curwp->w.doto = saved_doto + doto_adj;          /* Restore position */
    lchange(WFHARD);
ensure_ex_free:
    Xfree(gc.ex);
    return;
}

/* Move the cursor forward by the specified number of words. As you move,
 * convert any characters to upper case. Error if you try and move beyond the
 * end of the buffer. Bound to "M-U".
 */
int upperword(int f, int n) {
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if  */
        return rdonly();            /* we are in read only mode     */
    if (n < 0) return FALSE;
    while (n--) {
        while (inword(NULL) == FALSE) {
            if (forw_grapheme(1) <= 0) return FALSE;
        }
        int prev_zw_break = zw_break;
        while ((inword(NULL) != FALSE) || prev_zw_break) {
            ensure_case(UTF8_UPPER);
            if (forw_grapheme(1) <= 0) return FALSE;
            prev_zw_break = zw_break;
        }
    }
    return TRUE;
}

/* Move the cursor forward by the specified number of words. As you move
 * convert characters to lower case. Error if you try and move over the end of
 * the buffer. Bound to "M-L".
 */
int lowerword(int f, int n) {
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if  */
        return rdonly();            /* we are in read only mode     */
    if (n < 0) return FALSE;
    while (n--) {
        while (inword(NULL) == FALSE) {
            if (forw_grapheme(1) <= 0) return FALSE;
        }
        int prev_zw_break = zw_break;
        while ((inword(NULL) != FALSE)  || prev_zw_break) {
            ensure_case(UTF8_LOWER);
            if (forw_grapheme(1) <= 0) return FALSE;
            prev_zw_break = zw_break;
        }
    }
    return TRUE;
}

/* Move the cursor forward by the specified number of words. As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case. Error if you try and move past the end of the
 * buffer. Bound to "M-C".
 */
int capword(int f, int n) {
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if (n < 0) return FALSE;
    while (n--) {
        while (inword(NULL) == FALSE) {
            if (forw_grapheme(1) <= 0) return FALSE;
        }
        int prev_zw_break = zw_break;
        if (inword(NULL) != FALSE) {
            ensure_case(UTF8_UPPER);
            if (forw_grapheme(1) <= 0) return FALSE;
            while ((inword(NULL) != FALSE) || prev_zw_break) {
                ensure_case(UTF8_LOWER);
                if (forw_grapheme(1) <= 0) return FALSE;
            }
        }
    }
    return TRUE;
}

/* Kill forward by "n" words. Remember the location of dot. Move forward by
 * the right number of words. Put dot back where it was and issue the kill
 * command for the right number of characters. With a zero argument, just
 * kill one word and no whitespace. Bound to "M-D".
 * GGR - forw_grapheme()/back_grapheme() now move by graphemes, so we need
 *       to track the byte count (which they now return).
 */
int delbword(int, int);     /* Forward declaration */
int delfword(int f, int n) {
    struct line *dotp;      /* original cursor line */
    int doto;               /*      and row */
    ue64I_t size;           /* # of chars to delete */

/* Don't allow this command if we are in read only mode */
    if (curbp->b_mode & MDVIEW) return rdonly();

/* GGR - allow a -ve arg - use the backward function */
    if (n < 0) return(delbword(f, -n));

/* Clear the kill buffer if last command wasn't a kill */
    if ((com_flag & CFKILL) == 0) {
        kdelete();
        com_flag |= CFKILL; /* this command is a kill */
    }

/* Save the current cursor position */
    dotp = curwp->w.dotp;
    doto = curwp->w.doto;

/* Figure out how many characters to give the axe */
    size = 0;
    int moved = 0;

/* Get us into a word.... */
    while (inword(NULL) == FALSE) {
        moved = forw_grapheme(1);
        if (moved <= 0) return FALSE;
        size += moved;
    }

    if (n == 0) {       /* skip one word, no whitespace! */
        int prev_zw_break = 0;
        while ((inword(NULL) == TRUE) || prev_zw_break) {
            moved = forw_grapheme(1);
            if (moved <= 0) return FALSE;
            size += moved;
            prev_zw_break = zw_break;
        }
    } else {            /* skip n words.... */
        while (n--) {

/* If we are at EOL; skip to the beginning of the next */
            while (curwp->w.doto == lused(curwp->w.dotp)) {
                moved = forw_grapheme(1);
                if (moved <= 0) return FALSE;
                ++size;     /* Will move one to next line */
            }

/* Move forward till we are at the end of the word */
            int prev_zw_break = 0;
            while ((inword(NULL) == TRUE) || prev_zw_break) {
                moved = forw_grapheme(1);
                if (moved <= 0) return FALSE;
                size += moved;
                prev_zw_break = zw_break;
            }

/* If there are more words, skip the interword stuff */
            if (n != 0) while (inword(NULL) == FALSE) {
                moved = forw_grapheme(1);
                if (moved <= 0) return FALSE;
                size += moved;
            }
        }
    }
/* Restore the original position and delete the words */
    curwp->w.dotp = dotp;
    curwp->w.doto = doto;
    return ldelete(size, TRUE);
}

/* Kill backwards by "n" words. Move backwards by the desired number of words,
 * counting the characters. When dot is finally moved to its resting place,
 * fire off the kill command. Bound to "M-Rubout" and to "M-Backspace".
 * GGR - forw_grapheme()/back_grapheme() now move by graphemes, so we need
 *       to track the byte count (which they now return).
 */
int delbword(int f, int n) {
    ue64I_t size;

/* GGR - variables for kill-ring fix-up */
    int    i, status, ku;
    char   *sp;             /* pointer into string to insert */
    struct kill *kp;        /* pointer into kill buffer */
    struct kill *op;

/* Don't allow this command if we are in read only mode */
    if (curbp->b_mode & MDVIEW) return rdonly();

/* GGR - allow a -ve arg - use the forward function */
    if (n <= 0) return(delfword(f, -n));

/* Clear the kill buffer if last command wasn't a kill */
    if ((com_flag & CFKILL) == 0) {
        kdelete();
        com_flag |= CFKILL; /* this command is a kill */
    }

    int moved;
    moved = back_grapheme(1);
    if (moved <= 0) return FALSE;
    size = moved;
    while (n--) {
        while (inword(NULL) == FALSE) {
            moved = back_grapheme(1);
            if (moved <= 0) return FALSE;
            size += moved;
        }
        while ((inword(NULL) != FALSE) || zw_break) {
            moved = back_grapheme(1);
            if (moved <= 0) goto bckdel;
            size += moved;
        }
    }
    moved = forw_grapheme(1);
    if (moved <= 0) return FALSE;
    size -= moved;

/* GGR
 * We have to cater for the function being called multiple times in
 * succession. This should be the equivalent of Esc<n>delbword, i.e. the
 * text can be yanked back and still all be in the same order as it was
 * originally.
 * This means that we have to fiddle with the kill ring buffers.
 */
bckdel:
    if ((kp = kbufh[0]) != NULL) {
        kbufh[0] = kbufp = NULL;
        ku = kused[0];
        kused[0] = KBLOCK;
    }

    status = ldelete(size, TRUE);

    /* fudge back the rest of the kill ring */
    while (kp != NULL) {
        if (kp->d_next == NULL)
                i = ku;
        else
                i = KBLOCK;
        sp = kp->d_chunk;
        while (i--) {
            kinsert(*sp++);
        }
        op = kp;
        kp = kp->d_next;
        Xfree(op);          /* kinsert() Xmallocs, so we free */
    }
    return(status);
}

/* delete n paragraphs starting with the current one
 *
 * int f        default flag
 * int n        # of paras to delete
 */
int killpara(int f, int n) {
    UNUSED(f);
    int status;     /* returned status of functions */

/* Remember the current mark for later restore */
    sysmark.p = curwp->w.markp;
    sysmark.o = curwp->w.marko;

    while (n--) {           /* for each paragraph to delete */

/* Mark out the end and beginning of the para to delete */
        gotoeop(FALSE, 1);

/* Set the mark here */
        curwp->w.markp = curwp->w.dotp;
        curwp->w.marko = curwp->w.doto;

/* Go to the beginning of the paragraph */
        gotobop(FALSE, 1);
        curwp->w.doto = 0;  /* force us to the beginning of line */

/* And delete it */
        if ((status = killregion(FALSE, 1)) != TRUE) return status;

/* and clean up the 2 extra lines */
        ldelete(2L, TRUE);
    }

/* Restore the original mark */
    curwp->w.markp = sysmark.p;
    curwp->w.marko = sysmark.o;
    sysmark.p = NULL;       /* RESET! No longer valid */
    return TRUE;
}

/*      wordcount:      count the # of words in the marked region,
 *                      (or whole file - GGR)
 *                      along with average word sizes, # of chars, etc,
 *                      and report on them.
 *
 * int f, n;            force flag and numeric arguments
 */
int wordcount(int f, int n) {
    UNUSED(f);
    int orig_offset;        /* offset in line at start */
    ue64I_t size;           /* size of region left to count */
    int wordflag;           /* are we in a word now? */
    int lastword;           /* were we just in a word? */
    ue64I_t nwords;         /* total # of words */
    ue64I_t nchars;         /* total number of chars */
    int nlines;             /* total number of lines in region */
    int avgch;              /* average number of chars/word */
    int status;             /* status return code */
    struct region region;   /* region to look at */
    struct inwbuf mywb;     /* Tracking info for inword() */
    struct locs saved;

/* Make sure we have a region to count.
 * Although, if we've been given a +ve numeric arg, use the whole file.
 */
    if (n > 1) {
/* Save the current position and mark
 * This is OK as we're not going to insert/delete any text before
 * restoring them.
 */
        saved.dotp = curwp->w.dotp;
        saved.markp = curwp->w.markp;
        saved.doto = curwp->w.doto;
        saved.marko = curwp->w.marko;
/* Set the current position to BOF and mark to EOF */
        curwp->w.markp = curwp->w_bufp->b_linep;
        curwp->w.marko = lused(curwp->w.markp);
        curwp->w.dotp = lforw(curwp->w_bufp->b_linep);
        curwp->w.doto = 0;
    }
    status = getregion(&region);
/* Restore the real position and mark before checking the result */
    if (n > 1) {
        curwp->w.dotp = saved.dotp;
        curwp->w.markp = saved.markp;
        curwp->w.doto = saved.doto;
        curwp->w.marko = saved.marko;
    }
    if (!status) return status;
    mywb.lp = region.r_linep;
    orig_offset = region.r_offset;
    mywb.offs = region.r_offset;
    size = region.r_bytes;
/* Count up things */
    lastword = FALSE;
    nchars = 0L;
    nwords = 0L;
    nlines = 1;
    while (size--) {
        int start_offs = mywb.offs;
        wordflag = inword(&mywb);
/* Check for starting a newline.
 * inword() is advancing our position, including moving to the next line.
 */
        if (mywb.offs == 0) {
            ++nlines;
        }
        else {
            size -= (mywb.offs - start_offs) - 1; /* "Extra" bytes used */
        }
        if (wordflag & !lastword) ++nwords;
        ++nchars;
        lastword = wordflag;
    }
/* GGR - Increment line count if offset is now more than at the start
 * So line1 col3 -> line2 col55 is 2 lines,. not 1
 */
    if (mywb.offs > orig_offset) ++nlines;
/* And report on the info */
    if (nwords > 0L) avgch = (int) ((100L * nchars) / nwords);
    else             avgch = 0;

/* GGR - we don't need nlines+1 here now */
    mlwrite("Words %D Chars %D Lines %d Avg chars/word %f",
          nwords, nchars, nlines, avgch);
    return TRUE;
}

/* "Filling" (as in word wrap and fill/just para)  needs its own
 * version of "forw/backword" as it actually wants to split on
 * whitespace/zw_break *only*.
 */
static int filler_fword(void) {
    while (at_abreak(NULL) == TRUE)
        if (forw_grapheme(1) <= 0) return FALSE;
    do {
        if (forw_grapheme(1) <= 0) return FALSE;
    } while (at_abreak(NULL) != TRUE);
    return TRUE;
}
static int filler_bword(void) {
/* Have to go back one at the start it case we are looking at the
 * first char of a word.
 */
    if (back_grapheme(1) <= 0) return FALSE;
    while (at_abreak(NULL) == TRUE)
        if (back_grapheme(1) <= 0) return FALSE;
    do {
        if (back_grapheme(1) <= 0) {
/* If this is because we are at b-of-file then we are at the first
 * word and have got there successfully.
 * Probably can't happen here, but...
 */
            if ((curwp->w.doto == 0) &&
                (lback(curwp->w.dotp) == curwp->w_bufp->b_linep))
                return TRUE;
            else return FALSE;
        }
    } while (at_abreak(NULL) != TRUE);
/* We've gone back beyond the start, so adjust now */
    return (forw_grapheme(1) > 0);  /* Success count => T/F */
}

/* Word wrap on breaks.
 * Back-over whatever precedes the point on the current line and stop on
 * the first word-break or the beginning of the line.
 * If we reach the beginning of the line, jump back to where we started
 * (as there is no break) and insert a new line.
 * Otherwise, (optionally) remove all whitespace at the break, add a
 * newline and jump back to where we started.
 *
 * We start with the code that does the actual wrapping.
 * The callable routine follows.
 */
static int do_actual_wrap(int wdel) {
    if (lused(curwp->w.dotp) == 0) return FALSE;    /* Empty line */

/* Remember where we are and get to the start of the current/previous word.
 * If we are not at whitespace, go forw 1 first, just in case we are at
 * the start of a word.
 */
    int orig_offs = curwp->w.doto;
    if (!at_abreak(NULL)) forwchar(0, 1);
    if (!filler_bword()) return FALSE;
    if (curwp->w.doto == 0) {       /* No break */
        curwp->w.doto = orig_offs;
        if (!lnewline()) return FALSE;
        return TRUE;
    }

/* We have found a place to break...so do it.
 * Remove all whitespace here before wrapping.
 */
    int reset_cnt = orig_offs - curwp->w.doto;
    if (wdel) whitedelete(0, 0);
    if (!lnewline()) return FALSE;
    curwp->w.doto = reset_cnt;

    return TRUE;
}

/* Now the callable function.
 * If FullWrap mode is on (n > 1) the entire line preceding point is
 * wrapped (not just the final word) and whitespace is "eaten" on each wrap.
 *
 * Whitespace is not eaten in Original wrap mode, as that is how it worked.
 *
 * Returns TRUE on success, FALSE on errors.
 *
 * @f: default flag.
 * @n: numeric argument.
 *
 * This is usually (always?) called via the internal META|SPEC|'W' binding
 * in execute()[main.c] and insert_newline()[random.c]
 */
int wrapword(int f, int n) {
    UNUSED(f);

/* Have we been asked to do a full wrap? */

    if (n <= 1) {                   /* No. Just wrap final word */
        if (!do_actual_wrap(FALSE)) return FALSE;
    }
    else {
/* Set where we are. We need to get back here using a method that gets
 * updated on text updates.
 * Use the system mark, to preserve the user one.
 */
        sysmark.p = curwp->w.dotp;
        sysmark.o = curwp->w.doto;

        if (getccol() <= fillcol) return FALSE; /* Shouldn't be here! */
        while(1) {                  /* Go on until done */
            setccol(fillcol);
/* If we are now at whitespace wrapping is easy */
            if (at_abreak(NULL)) {
                whitedelete(0, 0);
                if (!lnewline()) return FALSE;
            }
/* Get to end of any word we are in */
            else {
                while (!at_abreak(NULL))
                     if (forw_grapheme(1) <= 0) return FALSE;
                if (!do_actual_wrap(TRUE)) return FALSE;
/* Now delete all whitespace for where we are now and if that is not at
 * end-of-line or start-of-line, put one space in.
 * This handles "mid-line" wraps and inability to wrap
 */
                whitedelete(0, 0);
                if ((curwp->w.doto != lused(curwp->w.dotp)) &&
                    (curwp->w.doto != 0)) linsert_byte(1, ' ');
            }
/* Back to where we were (which will have moved and been updated).
 * For a "normal" wrap the system mark will be on the current line, but
 * for an "over-long" wrap where we started at the end of the "over-long"
 * word the mark will now be on the line before us, and we must be done.
 */
            if (curwp->w.dotp != sysmark.p) break;
            curwp->w.doto = sysmark.o;
            if (getccol() <= fillcol) break;    /* Done */
        }
/* Now remove all whitespace at our final destination.
 * The space we typed to get here will be added later.
 */
        whitedelete(0, 0);
        sysmark.p = NULL;   /* RESET! No longer valid */
    }
/* Make sure the display is not horizontally scrolled */
    if (curwp->w.fcol != 0) {
        curwp->w.fcol = 0;
        curwp->w_flag |= WFHARD | WFMOVE | WFMODE;
    }
    return TRUE;
}

/* Set the GGR-added end-of-sentence list for use by fillpara and
 * justpara.
 * Allows utf8 punctuation, but does NOT cater for any zero-width
 * character usage (so no combining diacritical marks here...).
 * Mainly for macro usage setting.
 *
 * int f, n;            arguments ignored
 */
static int n_eos = 0;
static char eos_str[NLINE] = "";    /* String given by user */
int eos_chars(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;
    char prompt[NLINE+60];
    char buf[NLINE];

    if (n_eos == 0) strcpy(eos_str, "none");    /* Clearer for user? */
    sprintf(prompt,
          "End of sentence characters " MLbkt("currently %s") ":",
          eos_str);

    status = mlreply(prompt, buf, NLINE - 1, CMPLT_NONE);
    if (status == FALSE) {      /* Empty response - remove item */
        Xfree_setnull(eos_list);
        n_eos = 0;
    }
    else if (status == TRUE) {  /* Some response - set item */
/* We'll get the buffer length in characters, then allocate that number of
 * unicode chars. It might be more than we need (if there is a utf8
 * multi-byte character in there) but it's not going to be that big.
 * Actually we'll allocate one extra and put an illegal value at the end.
 */
        int len = strlen(buf);
        eos_list = Xrealloc(eos_list, sizeof(unicode_t)*(len + 1));
        int i = 0;
        n_eos = 0;
        while (i < len) {
            unicode_t c;
            i += utf8_to_unicode(buf, i, len, &c);
            eos_list[n_eos++] = c;
        }
        eos_list[n_eos] = UEM_NOCHAR;
        strcpy(eos_str, buf);
    }
/* Do nothing on anything else - allows you to abort once you've started. */
    return status;              /* Whatever mlreply returned */
}

/* Generic filling routine to be called by various front-ends.
 * The real worker function.
 * Fills the text between point and end of paragraph.
 * given parameters.
 *  indent      l/h indent to use (NOT on first line!!)
 *  width       column at which to wrap
 *  justify     do we add spaces to pad-out right margin.
 */
static int filler(int indent, int width, int justify) {

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
            return rdonly();        /* we are in read only mode     */

    if (width == 0) {             /* no fill column set */
        mlwrite_one("No fill column set");
        return FALSE;
    }
    int ltor = 0;           /* Direction of first padding pass */

/* We are at the beginning of the text to justify when we arrive.
 * This might NOT be in column 1!!
 *
 * Start by finding out where we will finish. Remember where we are
 * currently while doing this.
 */
    int start_offs = curwp->w.doto;
    struct line *start_line = curwp->w.dotp;
    if (!gotoeop(FALSE, 1)) return FALSE;           /* E-o-line in para */
    struct line *end_line = lforw(curwp->w.dotp);   /* Line after para */
    curwp->w.doto = start_offs;
    curwp->w.dotp = start_line;

/* Get rid of any leading space */
    while (at_wspace(NULL)) if (!ldelgrapheme(1, FALSE)) return FALSE;
    int all_done = 0;
    int words_to_wrap = 0;
    while(1) {
/* We skip over the next word, and wipe out all spaces (== known state)
 * If we've reached the end of the paragraph we note this, otherwise we
 * delete the newline and remove all spaces again.
 */
        if (!filler_fword()) return FALSE;          /* Next word */
        words_to_wrap++;
        whitedelete(0, 0);                          /* -> 0 spaces */
        if (curwp->w.doto == lused(curwp->w.dotp)) {    /* E-o-line */
            if (lforw(curwp->w.dotp) == end_line) {     /* At end of para? */
                all_done = 1;       /* Time to wrap up - no more spaces... */
            }
            else {
                if (!ldelgrapheme(1, FALSE)) return FALSE;  /* Delete newline */
                whitedelete(0, 0);          /* Remove any old leading space */
            }
        }
/* We are now at the end of a word, with no space before the next word
 * if there is one.
 */
        if (getccol() > width) {            /* Need to wrap? */
            if (words_to_wrap == 1) {       /* Nowhere to wrap */
                ;                           /* Put newline "here" */
            }
            else {
                if (!all_done && !zw_break)     /* No space at e-o-p */
                    if (!linsert_byte(1, ' '))  /* Interword space */
                        return FALSE;
                if (!filler_bword()) return FALSE;  /* To where we can wrap */
                whitedelete(0, 0);  /* Remove space there */
/* Do any justify before the move to next line.
 * Needs at least 2 words - see words_to_wrap check above.
 */
                if (justify) {
                    int needed = width - getccol();
                    int end_pad_at = curwp->w.doto; /* End of our line */
/* If needed is -ve there is nowhere to break - just live with the
 * over-long line.
 */
                    while (needed > 0) {
                        if (ltor) curwp->w.doto = start_offs;
                        else      curwp->w.doto = end_pad_at;
                        while(needed) {
                            if (ltor) { /* Check for beyond right margin */
                                if (!filler_fword()) return FALSE;
                                if (zw_break) continue;
                                if (curwp->w.doto >= end_pad_at) break;
                            }
                            else {      /* Check for beyond left margin? */
                                if (!filler_bword()) return FALSE;
                                if (zw_break) continue;
                                if (curwp->w.doto <= start_offs) break;
                            }
                            if (!linsert_byte(1, ' ')) return FALSE;
                            end_pad_at++;       /* One more to e-o-l pos */
                            needed--;           /* One less to do */
                        }
                        ltor = 1 - ltor;        /* Switch direction */
                    }
                    curwp->w.doto = end_pad_at; /* Back to wrap point */
                }
            }
/* Having sorted out the text for this line, insert a newline and indent */
            lnewline();
            words_to_wrap = 0;
            if (indent) if (!linsert_byte(indent, ' ')) return FALSE;
            start_offs = indent;        /* Spaces are 1-byte chars */
        }
        else if (!all_done) {                   /* Not wrapping */
/* Handle any defined punctuation characters.
 * Note that this does not need to be done for the last word on the line,
 * which happens at any wrap point and at end of paragraph.
 */
            int nsp = 1;
            if (eos_list) {     /* Some eos defined */
                struct grapheme gi;
/* Don't build any ex... */
                (void)build_prev_grapheme(ltext(curwp->w.dotp),
                 curwp->w.doto, lused(curwp->w.dotp), &gi, 1);
                for (unicode_t *eosch = eos_list;
                     *eosch != UEM_NOCHAR; eosch++) {
                    if (gi.cdm == 0 && gi.uc == *eosch) {
                        nsp = 2;
                        break;
                    }
                }
            }
            if (!zw_break && !linsert_byte(nsp, ' ')) return FALSE;
        }
        if (all_done) {                             /* Tidy up */
            curwp->w.dotp = lforw(curwp->w.dotp);   /* End at the start... */
            curwp->w.doto = 0;                      /* ...of next line */
            break;                                  /* All done */
        }
    }
    return TRUE;
}

/* Fill the current paragraph according to the current fill column.
 * Leave any leading whitespace in the paragraph in place.
 *
 * f and n - deFault flag and Numeric argument
 */
int fillpara(int f, int n) {
    UNUSED(f);
    int justify = 0;
    if (n < 0) {
        n = -n;
        justify = 1;
    }
    if (n == 0) n = 1;
    int status = FALSE;
    while (n--) {
        forwword(FALSE, 1);
        if (!gotobop(FALSE, 1)) return FALSE;
        status = filler(0, fillcol, justify);
        if (status != TRUE) break;
    }
    return status;
}

/* Fill the current paragraph using the current column as the indent.
 * Remove any existing leading whitespace in the paragraph.
 *
 * int f, n;            deFault flag and Numeric argument
 */
int justpara(int f, int n) {
    UNUSED(f);
    int justify = 0;
    if (n < 0) {
        n = -n;
        justify = 1;
    }
    if (n == 0) n = 1;

/* Have to cater for tabs and multibyte chars...can't use w.doto */
    int leftmarg = getccol();
    if (leftmarg + 10 > fillcol) {
        mlwrite_one("Column too narrow");
        return FALSE;
    }
    int status = FALSE;
    while (n--) {
/* We need to get rid of any leading whitespace, then pad first line
 * here, at bop */
        gotoeol(FALSE, 1);          /* In case we are already at bop */
        gotobop(FALSE, 1);
        (void)whitedelete(1, 1);    /* Don't care whether there was any */
        curwp->w.doto = 0;          /* Should be 0 anyway... */
        linsert_byte(leftmarg, ' ');
        status = filler(leftmarg, fillcol, justify);
        if (status != TRUE) break;
/* Position cursor at indent column in next non-blank line */
        while(forwline(0, 1)) if (lused(curbp->b_linep) == 0) break;
        setccol(leftmarg);
    }

    return status;
}

/* a GGR one.. */
int fillwhole(int f, int n) {
    int savline, thisline;
    int status = FALSE;

    gotobob(TRUE, 1);
    savline = 0;
    mlwrite_one(MLbkt("Filling all paragraphs in buffer.."));
    while ((thisline = getcline()) > savline) {
        status = fillpara(f, n);
        savline = thisline;
    }
    mlwrite_one("");
    return(status);
}

/* Reformat the paragraphs containing the region into a list.
 * This is actually called via front-ends which set the format
 * to use.
 */
static int region_listmaker(const char *lbl_fmt, int n) {
    char label[80];
    struct region f_region; /* Formatting region */
    int justify = (n < 0);  /* The only significance of n. Is it -ve? */

/* We will be working on the defined region (mark to/from point) so
 * get that.
 */
    if (!getregion(&f_region)) return FALSE;

/* The region must not be empty. This also caters for an empty buffer. */

    if (f_region.r_bytes <= 0) return FALSE;

/* We have to figure out where to stop.
 * The buffer lines may be re-allocated during the reformat, so we
 * count the paragraphs now and loop that many times.
 *
 * Start by getting to the end of the paragraph containing the end of
 * the region.
 */
    struct line *flp = f_region.r_linep;
    ue64I_t togo = f_region.r_bytes + f_region.r_offset;
    while (1) {
        ue64I_t left = togo - (lused(flp) + 1); /* Incl newline */
        if (left < 0) break;
        togo = left;
        flp = lforw(flp);
    }
    curwp->w.dotp = flp;                /* Fix up line and ... */
    curwp->w.doto = togo;               /* ... offset for end-of-range */

/* We want to get to EOP, but if we are at the end of the last line
 * (which is a likely marking scenario for a region) we don't want to go
 * to the next paragraph!
 * So we first go back a word.
 * This also means we can put the mark *between* paragraphs.....
 */
    backword(FALSE, 1);
    gotoeop(FALSE, 1);
    struct line *eopline = curwp->w.dotp;

/* Now we go back to the beginning and advance by end of paragraphs
 * until we find the one we just found. This gives us the loop count.
 */
    curwp->w.dotp = f_region.r_linep;
    curwp->w.doto = f_region.r_offset;
    int pc = 0;
    while(1) {
        pc++;
        gotoeop(FALSE, 1);
        if (eopline == curwp->w.dotp) break;
    }

/* Back to the start and get to start of the paragraph.
 * To handle already being at the bop, or on an empty line, we
 * actually go to eop first.
 */
    curwp->w.dotp = f_region.r_linep;
    curwp->w.doto = f_region.r_offset;
    gotoeop(FALSE, 1);              /* In case we are already at bop */
    gotobop(FALSE, 1);

    int status = FALSE;
    int ix = 0;
    while (pc--) {                  /* Loop for paragraph count */
        (void)whitedelete(0, 0);    /* Don't care whether there was any */
        ix++;                       /* Insert the counter */
        int cc = snprintf(label, sizeof(label)-1, lbl_fmt, ix);
        curwp->w.doto = 0;          /* Should be 0 anyway... */
        linstr(label);
        status = filler(cc, fillcol, justify);

/* Onto the next paragraph */

        if (forwword(FALSE, 1)) gotobol(FALSE, 1);
        if (status != TRUE) break;
    }

/* We've not lost mark by the time we get here, but we need to think about
 * what ctl-C (reexec) will do.
 * If we leave things as they are it could reformat the preceding
 * paragraph(s) - again. So we'll have two labels on it.
 * So we'll set the mark to be one-char ahead, which means that ctl-C
 * will reformat the next paragraph, which makes more sense.
 * But note that this will *not* propagate any current paragraph counter!
 */
    struct line *here_dotp = curwp->w.dotp;
    int here_doto = curwp->w.doto;
    forw_grapheme(1);
    curwp->w.markp = curwp->w.dotp;
    curwp->w.marko = curwp->w.doto;
    curwp->w.dotp = here_dotp;
    curwp->w.doto = here_doto;

    return status;
}

/* Reformat the paragraphs containing the region into a list.
 * This forces the user variable for the labelling to be " o "
 * (the number-formatting in region_listmaker() will have no effect).
 *
 * int f, n;            deFault flag and Numeric argument
 */
int makelist_region(int f, int n) {
    UNUSED(f);
    int status = region_listmaker(regionlist_text, n);
    return status;
}

/* Reformat the paragraphs containing the region into a list.
 * This uses the user variable %list-indent-text for the label.
 * This may have AT MOST one numeric variable template.
 *
 * int f, n;            deFault flag and Numeric argument
 */
int numberlist_region(int f, int n) {
    UNUSED(f);
    int status = region_listmaker(regionlist_number, n);
    return status;
}
