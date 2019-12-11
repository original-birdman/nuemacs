/*      basic.c
 *
 * The routines in this file move the cursor around on the screen. They
 * compute a new value for the cursor, then adjust ".". The display code
 * always updates the cursor location, so only moves between lines, or
 * functions that adjust the top line in the window and invalidate the
 * framing, are hard.
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "utf8.h"

#include "utf8proc.h"

/* GGR - This was ctrulen - get a consistent "real" line length.
 * However, it was only ever use to check for an empty line.
 * So it's been renamed and simplified.
 * It's not empty if we have any *byte* other than a space or tab
 */
static int curline_empty(void) {
    char c;
    int end = llength(curwp->w_dotp);
    for (int ci = 0; ci < end; ci++) {
        c = lgetc(curwp->w_dotp, ci);
        if (c != ' ' || c != '\t') return FALSE;
    }
    return TRUE;
}

/*
 * This routine, given a pointer to a struct line, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
static int getgoal(struct line *dlp) {
    int col = 0;
    int dbo = 0;
    int len = llength(dlp);

    while (dbo < len) {
        unicode_t c;
        int width = utf8_to_unicode(dlp->l_text, dbo, len, &c);
        update_screenpos_for_char(col, c);
        if (col > curgoal) break;
        dbo += width;
    }
    return dbo;
}

/*
 * Move the cursor to the beginning of the current line.
 */
int gotobol(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w_doto = 0;
    return TRUE;
}

/*
 * Move the cursor backwards by "n" characters.
 * GGR - we now move by grapheme - or actual *display* character - rather
 * than by byte or utf8 character.
 * SO THIS NOW RETURNS THE bytes MOVED!!! Although any failure along
 * the way results in it returning minus the actual move (NEW!).
 * If "n" is less than zero call "forw_grapheme" to actually do the move.
 * Otherwise compute the new cursor location.
 * Error if you try and move out of the buffer.
 * Set the flag if the line pointer for dot changes.
 * The macro-callable backchar() (as macros need something that returns
 * TRUE/FALSE) is a write-through to this, with the "f" arg ignored (it
 * has never done anything).
 * Internal calls should be to back_grapheme() *not* backchar().
 */
int back_grapheme(int n) {
    struct line *lp;

    if (n < 0) return forw_grapheme(-n);
    int moved = 0;
    while (n--) {
        if (curwp->w_doto == 0) {
            if ((lp = lback(curwp->w_dotp)) == curbp->b_linep) return -moved;
            curwp->w_dotp = lp;
            curwp->w_doto = llength(lp);
            curwp->w_flag |= WFMOVE;
            moved++;
        }
        else {
/* GGR - We must cater for utf8 characters in the same way
 * as the rest of the utf8 code does.
 */
            int saved_doto = curwp->w_doto;
            curwp->w_doto =
                 prev_utf8_offset(curwp->w_dotp->l_text, curwp->w_doto, TRUE);
            moved += saved_doto - curwp->w_doto;
        }
    }
    return moved;
}

/* We still need a bindable function that returns TRUE/FALSE */
int backchar(int f, int n) {
    UNUSED(f);
    return (back_grapheme(n) > 0);
}

/* Move the cursor to the end of the current line. Trivial. No errors.
 */
int gotoeol(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w_doto = llength(curwp->w_dotp);
    return TRUE;
}

/* Move the cursor forwards by "n" characters.
 * GGR - we now move by grapheme - or actual *display* character - rather
 * than by byte or utf8 character.
 * SO THIS NOW RETURNS THE bytes MOVED!!! Although any failure along
 * the way results in it returning minus the actual move (NEW!).
 * If "n" is less than zero call "back_grapheme" to actually do the move.
 * Otherwise compute the new cursor location, and move ".".
 * Error if you try and move off the end of the buffer.
 * Set the flag if the line pointer for dot changes.
 * The macro-callable forwchar() (as macros need something that returns
 * TRUE/FALSE) is a write-through to this, with the "f" arg ignored (it
 * has never done anything).
 * Internal calls should be to forw_grapheme() *not* forwchar().
 */
int forw_grapheme(int n) {
    if (n < 0) return back_grapheme(-n);
    int moved = 0;
    while (n--) {
        int len = llength(curwp->w_dotp);
        if (curwp->w_doto == len) {
            if (curwp->w_dotp == curbp->b_linep) return -moved;
            curwp->w_dotp = lforw(curwp->w_dotp);
            curwp->w_doto = 0;
            curwp->w_flag |= WFMOVE;
            moved++;
        }
        else {
/* GGR - We must cater for utf8 characters in the same way
 * as the rest of the utf8 code does.
 */
            int saved_doto = curwp->w_doto;
            curwp->w_doto = 
                 next_utf8_offset(curwp->w_dotp->l_text, curwp->w_doto,
                                  llength(curwp->w_dotp), TRUE);
            moved += curwp->w_doto - saved_doto;
        }
    }
    return moved;
}

/* We still need a bindable/macro function that returns TRUE/FALSE */
int forwchar(int f, int n) {
    UNUSED(f);
    return (forw_grapheme(n) > 0);
}

/* Move to a particular line.
 *
 * @n: The specified line position at the current buffer.
 */
int gotoline(int f, int n) {
    int status;
    char arg[NSTRING]; /* Buffer to hold argument. */

/* Get an argument if one doesnt exist. */
    if (f == FALSE) {
        if ((status =
          mlreply("Line to GOTO: ", arg, NSTRING, EXPNONE)) != TRUE) {
            mlwrite(MLbkt("Aborted"));
            return status;
        }
        n = atoi(arg);
    }
/* Handle the case where the user may be passed something like this:
 * em filename +
 * In this case we just go to the end of the buffer.
 */
    if (n == 0) return gotoeob(f, n);

/* If a bogus argument was passed, then returns false. */
    if (n < 0) return FALSE;

/* First, we go to the begin of the buffer. */
    gotobob(f, n);
    return forwline(f, n - 1);
}

/* Goto the beginning of the buffer. Massive adjustment of dot. This is
 * considered to be hard motion; it really isn't if the original value of dot
 * is the same as the new value of dot. Normally bound to "M-<".
 */
int gotobob(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w_dotp = lforw(curbp->b_linep);
    curwp->w_doto = 0;
    curwp->w_flag |= WFHARD;
    return TRUE;
}

/* Move to the end of the buffer. Dot is always put at the end of the file
 * (ZJ). The standard screen code does most of the hard parts of update.
 * Bound to "M->".
 */
int gotoeob(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w_dotp = curbp->b_linep;
    curwp->w_doto = 0;
    curwp->w_flag |= WFHARD;
    return TRUE;
}

/* Move forward by full lines. If the number of lines to move is less than
 * zero, call the backward line function to actually do it. The last command
 * controls how the goal column is set. Bound to "C-N". No errors are
 * possible.
 */
int forwline(int f, int n) {
    struct line *dlp;

    if (n < 0) return backline(f, -n);

/* If we are on the last line as we start....fail the command */

    if (curwp->w_dotp == curbp->b_linep) return FALSE;

/* If the last command was not a line move, reset the goal column */

    if ((lastflag & CFCPCN) == 0) curgoal = getccol(FALSE);

/* Flag this command as a line move */

    thisflag |= CFCPCN;

/* And move the point down */

    dlp = curwp->w_dotp;
    while (n-- && dlp != curbp->b_linep) dlp = lforw(dlp);

/* Reseting the current position */

    curwp->w_dotp = dlp;
    curwp->w_doto = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return TRUE;
}

/* This function is like "forwline", but goes backwards. The scheme is exactly
 * the same. Check for arguments that are less than zero and call your
 * alternate. Figure out the new line and call "movedot" to perform the
 * motion. No errors are possible. Bound to "C-P".
 */
int backline(int f, int n) {
    struct line *dlp;

    if (n < 0) return forwline(f, -n);

/* If we are on the first line as we start....fail the command */

    if (lback(curwp->w_dotp) == curbp->b_linep) return FALSE;

/* If the last command was not a line move, reset the goal column */

    if ((lastflag & CFCPCN) == 0) curgoal = getccol(FALSE);

/* Flag this command as a line move */

    thisflag |= CFCPCN;

/* And move the point up */

    dlp = curwp->w_dotp;
    while (n-- && lback(dlp) != curbp->b_linep) dlp = lback(dlp);

/* Reseting the current position */

    curwp->w_dotp = dlp;
    curwp->w_doto = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return TRUE;
}

/* The inword() test has been replaced with this, as we really want to
 * be skipping whitespace, not skipping over words. Things like
 * punctuation need to be counted too.
 */
static int at_whitespace(void) {
    struct grapheme gi;

    (void)lgetgrapheme(&gi, 1);         /* Don't get extras */
    if (gi.cdm != 0) return FALSE;
    switch (gi.uc) {
    case ' ':
    case '\t':
    case '\n':
        return TRUE;
    }
    return FALSE;
}

/* Go back to the beginning of the current paragraph.
 * Here we look for an empty line to delimit the beginning of a paragraph.
 *
 * int f, n;            default Flag & Numeric argument
 */
int gotobop(int f, int n) {
    int suc;        /* Bytes moved by last back_grapheme() */

    if (n < 0)      /* the other way... */
        return gotoeop(f, -n);

    while (n-- > 0) {   /* For each one asked for */

/* First scan back until we are in a word... */
        suc = back_grapheme(1);
        while ((suc > 0) && at_whitespace()) suc = back_grapheme(1);
        curwp->w_doto = 0;          /* ...and go to the B-O-Line */

/* Then scan back until we hit an empty line or B-O-buffer... */
        while (lback(curwp->w_dotp) != curbp->b_linep) {
            if (!curline_empty())
                curwp->w_dotp = lback(curwp->w_dotp);
            else
                break;
        }

/* ...and then forward until we are looking at a word */
        suc = 1;
        while ((suc > 0) && at_whitespace()) suc = forw_grapheme(1);
    }
    curwp->w_flag |= WFMOVE;        /* Force screen update */
    return TRUE;
}

/* Go forword to the end of the current paragraph
 * Here we look for an empty line to delimit the beginning of a paragraph.
 *
 * int f, n;            default Flag & Numeric argument
 */
int gotoeop(int f, int n) {
    int suc;        /* Bytes moved by last back_grapheme() */

    if (n < 0)      /* the other way... */
        return gotobop(f, -n);

    while (n-- > 0) {  /* for each one asked for */
/* First scan forward until we are in/looking at a word... */
        suc = 1;
        while ((suc > 0) && at_whitespace()) suc = forw_grapheme(1);
        curwp->w_doto = 0;          /* ...and go to the B-O-Line */
        if (suc)                    /* of next line if not at EOF */
            curwp->w_dotp = lforw(curwp->w_dotp);

/* Then scan forword until we hit an empty line or E-O-Buffer... */
        while (curwp->w_dotp != curbp->b_linep) {
            if (!curline_empty())   /* GGR */
                curwp->w_dotp = lforw(curwp->w_dotp);
            else
                break;
        }

/* ...and then backward until we are in a word */
        suc = back_grapheme(1);
        while ((suc > 0) && at_whitespace()) suc = back_grapheme(1);
        curwp->w_doto = llength(curwp->w_dotp); /* and to the EOL */
    }
    curwp->w_flag |= WFMOVE;  /* force screen update */
    return TRUE;
}

/* Scroll forward by a specified number of lines, or by a full page if no
 * argument. Bound to "C-V". The "2" in the arithmetic on the window size is
 * the overlap; this value is the default overlap value in ITS EMACS. Because
 * this zaps the top line in the display window, we have to do a hard update.
 */
int forwpage(int f, int n) {
    struct line *lp;

    if (f == FALSE) {
        if (term.t_scroll != NULL)
            if (overlap == 0) n = curwp->w_ntrows / 3 * 2;
            else              n = curwp->w_ntrows - overlap;
        else
            n = curwp->w_ntrows - 2;   /* Default scroll. */
        if (n <= 0)         /* Forget the overlap. */
            n = 1;          /* If tiny window. */
    } else if (n < 0)
        return backpage(f, -n);
    else                        /* Convert from pages. */
        n *= curwp->w_ntrows;   /* To lines. */
    lp = curwp->w_linep;

/* GGR
 * Make the check stop *before* we loop round from end to start of file
 * buffer, so we always leave a line on screen (but it may be empty...)
 * (replace "lp !=..." with "lforw(lp) !=...").
 * So we need to make 1 specific check first
 */
    if (lp != curbp->b_linep) {
        while (n-- && lforw(lp) != curbp->b_linep) lp = lforw(lp);
    }
    curwp->w_linep = lp;
    curwp->w_dotp = lp;
    curwp->w_doto = 0;
    curwp->w_flag |= WFHARD | WFKILLS;
    return TRUE;
}

/* This command is like "forwpage", but it goes backwards. The "2", like
 * above, is the overlap between the two windows. The value is from the ITS
 * EMACS manual. Bound to "M-V". We do a hard update for exactly the same
 * reason.
 */
int backpage(int f, int n) {
    struct line *lp;

    if (f == FALSE) {
        if (term.t_scroll != NULL)
            if (overlap == 0) n = curwp->w_ntrows / 3 * 2;
            else              n = curwp->w_ntrows - overlap;
        else
            n = curwp->w_ntrows - 2; /* Default scroll. */
        if (n <= 0)     /* Don't blow up if the. */
            n = 1;  /* Window is tiny. */
    }
    else if (n < 0)
        return forwpage(f, -n);
    else  /* Convert from pages. */
        n *= curwp->w_ntrows;  /* To lines. */

    lp = curwp->w_linep;
    while (n-- && lback(lp) != curbp->b_linep) lp = lback(lp);
    curwp->w_linep = lp;
    curwp->w_dotp = lp;
    curwp->w_doto = 0;
    curwp->w_flag |= WFHARD | WFINS;
    return TRUE;
}

/* Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".
 */
int setmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;
    mlwrite(MLbkt("Mark set"));
    return TRUE;
}

/* Swap the values of "." and "mark" in the current window. This is pretty
 * easy, bacause all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
int swapmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *odotp;
    int odoto;

    if (curwp->w_markp == NULL) {
        mlwrite("No mark in this window");
        return FALSE;
    }
    odotp = curwp->w_dotp;
    odoto = curwp->w_doto;
    curwp->w_dotp = curwp->w_markp;
    curwp->w_doto = curwp->w_marko;
    curwp->w_markp = odotp;
    curwp->w_marko = odoto;
    curwp->w_flag |= WFMOVE;
    return TRUE;
}
