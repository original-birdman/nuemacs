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

#define BASIC_C

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
    const char *cp = ltext(curwp->w.dotp);
    const char *ep = cp + lused(curwp->w.dotp);
    while (cp < ep) {
        if ((*cp != ' ') && (*cp != '\t')) return FALSE;
        cp++;
    }
    return TRUE;
}

/* This routine, given a pointer to a struct line, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
static int offset_for_curgoal(struct line *dlp) {
    int col = 0;        /* Cursor display column */
    int dbo = 0;        /* Byte offset within dlp */
    int len = lused(dlp);

    while (dbo < len) {
        unicode_t c;
        int bytes_used = utf8_to_unicode(ltext(dlp), dbo, len, &c);
        update_screenpos_for_char(col, c);
        if (col > curgoal) break;
        dbo += bytes_used;
    }
    return dbo;
}

/* Move the cursor to the beginning of the current line.
 */
int gotobol(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w.doto = 0;
    return TRUE;
}

/* Move the cursor forwards by "n" characters.
 * GGR - we now move by grapheme - or actual *display* character - rather
 * than by byte or utf8 character.
 * SO THIS NOW RETURNS THE bytes MOVED!!! Although any failure along
 * the way results in it returning minus the actual move (NEW!).
 * If "n" is less than zero call "back_grapheme" to actually do the move.
 * Otherwise compute the new cursor location, and move ".".
 * Error if you try to move off the end of the buffer.
 * Set the flag if the line pointer for dot changes.
 * The macro-callable forwchar() (as macros need something that returns
 * TRUE/FALSE) is a write-through to this, with the "f" arg ignored (it
 * has never done anything).
 * Internal calls should be to forw_grapheme() *not* forwchar().
 */
int back_grapheme(int);     /* Forward declaration */
int forw_grapheme(int n) {
    if (n < 0) return back_grapheme(-n);
    int moved = 0;
    int clen = lused(curwp->w.dotp);
    while (n--) {
        if (curwp->w.doto == clen) {
            if (curwp->w.dotp == curbp->b_linep) return -moved;
            curwp->w.dotp = lforw(curwp->w.dotp);
            curwp->w.doto = 0;
            curwp->w_flag |= WFMOVE;
            clen = lused(curwp->w.dotp);
            moved++;
        }
        else {
/* GGR - We must cater for utf8 characters in the same way
 * as the rest of the utf8 code does.
 */
            int saved_doto = curwp->w.doto;
            curwp->w.doto =
                 next_utf8_offset(ltext(curwp->w.dotp), curwp->w.doto,
                                  clen, TRUE);
            moved += curwp->w.doto - saved_doto;
        }
    }
    return moved;
}

/* Move the cursor backwards by "n" characters.
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
        if (curwp->w.doto == 0) {
            if ((lp = lback(curwp->w.dotp)) == curbp->b_linep) return -moved;
            curwp->w.dotp = lp;
            curwp->w.doto = lused(lp);
            curwp->w_flag |= WFMOVE;
            moved++;
        }
        else {
/* GGR - We must cater for utf8 characters in the same way
 * as the rest of the utf8 code does.
 */
            int saved_doto = curwp->w.doto;
            curwp->w.doto =
                 prev_utf8_offset(ltext(curwp->w.dotp), curwp->w.doto, TRUE);
            moved += saved_doto - curwp->w.doto;
        }
    }
    return moved;
}

/* We still need a bindable function that returns TRUE/FALSE */
int backchar(int f, int n) {
    UNUSED(f);
    return (back_grapheme(n) > 0);
}

/* Move the cursor to the end of the current line. Trivial. No errors. */
int gotoeol(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w.doto = lused(curwp->w.dotp);
    return TRUE;
}

/* We still need a bindable/macro function that returns TRUE/FALSE */
int forwchar(int f, int n) {
    UNUSED(f);
    return (forw_grapheme(n) > 0);
}

/* Goto the beginning of the buffer. Massive adjustment of dot. This is
 * considered to be hard motion; it really isn't if the original value of dot
 * is the same as the new value of dot. Normally bound to "M-<".
 */
int gotobob(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w.dotp = lforw(curbp->b_linep);
    curwp->w.doto = 0;
    curwp->w_flag |= WFHARD;
    return TRUE;
}

/* Move to the end of the buffer. Dot is always put at the end of the file
 * (ZJ). The standard screen code does most of the hard parts of update.
 * Bound to "M->".
 */
int gotoeob(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w.dotp = curbp->b_linep;
    curwp->w.doto = 0;
    curwp->w_flag |= WFHARD;
    return TRUE;
}

/* Internal routine to move by n full lines.
 * boundary conditions vary slightly according to the direction.
 * We remember that the last command was a line move so that we may
 * preserve the cursor in the starting column of consecutive line-move
 * command even if we pass through a short one on the way.
 */
static int move_n_lines(int n) {
    struct line *dlp;

/* If we are on the first(back) or last(forw) line as we start,
 * fail the command.
 */
    dlp = (n < 0)? lback(curwp->w.dotp): curwp->w.dotp;
    if (dlp == curbp->b_linep) return FALSE;

/* If the last command was not a line move, reset the goal column.
 * Always flag this command as a line move.
 */
    if ((com_flag & CFCPCN) == 0) {
        curgoal = getccol();
        com_flag |= CFCPCN;
    }

/* Move the point up/down */

    dlp = curwp->w.dotp;
    if (n < 0) while (n++ && lback(dlp) != curbp->b_linep) dlp = lback(dlp);
    else       while (n-- && dlp != curbp->b_linep) dlp = lforw(dlp);

/* Resetting the current position */

    curwp->w.dotp = dlp;
    curwp->w.doto = offset_for_curgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return TRUE;
}

/* The user-callable functions, bound to ctlP/Z (backline) and ctlN
 * (forwline) as well as the relevant cursor keys.
 * Just pass-on the numeric arg, negating it for backline().
 */

int forwline(int f, int n) {
    UNUSED(f);
    return move_n_lines(n);
}
int backline(int f, int n) {
    UNUSED(f);
    return move_n_lines(-n);
}

/* Move to a particular line.
 *
 * n: The specified line position in the current buffer.
 */
int gotoline(int f, int n) {
    int status;
    db_strdef(arg);  /* Buffer to hold argument. */

/* Get an argument if one doesn't exist. */
    if (f == FALSE) {
        if ((status =
          mlreply("Line to GOTO: ", &arg, CMPLT_NONE)) != TRUE) {
            mlwrite_one(MLbkt("Aborted"));
            return status;
        }
        n = atoi(db_val(arg));
    }
    db_free(arg);

/* Handle the case where the user may be passed something like this:
 * uemacs filename +
 * In this case we just go to the end of the buffer.
 */
    if (n == 0) return gotoeob(f, n);

/* If a bogus argument was passed, then return False. */
    if (n < 0) return FALSE;

/* First, we go to the begin of the buffer. */
    gotobob(0, 0);
    return forwline(0, n - 1);
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
int gotoeop(int, int);      /* Forward declaration */
int gotobop(int f, int n) {
    int suc;                /* Bytes moved by last back_grapheme() */

    if (n < 0)              /* the other way... */
        return gotoeop(f, -n);

    while (n-- > 0) {       /* For each one asked for */

/* First scan back until we are in a word... */
        suc = back_grapheme(1);
        while ((suc > 0) && at_whitespace()) suc = back_grapheme(1);
        curwp->w.doto = 0;  /* ...and go to the B-O-Line */

/* Then scan back until we hit an empty line or B-O-buffer... */
        while (lback(curwp->w.dotp) != curbp->b_linep) {
            if (!curline_empty())
                curwp->w.dotp = lback(curwp->w.dotp);
            else
                break;
        }

/* ...and then forward until we are looking at a word */
        suc = 1;
        while ((suc > 0) && at_whitespace()) suc = forw_grapheme(1);
    }
    curwp->w_flag |= WFMOVE;    /* Force screen update */
    return TRUE;
}

/* Go forward to the end of the current paragraph
 * Here we look for an empty line to delimit the beginning of a paragraph.
 *
 * int f, n;            default Flag & Numeric argument
 */
int gotoeop(int f, int n) {
    int suc;            /* Bytes moved by last back_grapheme() */

    if (n < 0)          /* the other way... */
        return gotobop(f, -n);

    while (n-- > 0) {   /* for each one asked for */
/* First scan forward until we are in/looking at a word... */
        suc = 1;
        while ((suc > 0) && at_whitespace()) suc = forw_grapheme(1);
        curwp->w.doto = 0;  /* ...and go to the B-O-Line */
        if (suc)            /* in next line if not at EOF */
            curwp->w.dotp = lforw(curwp->w.dotp);

/* Then scan forward until we hit an empty line or E-O-Buffer... */
        while (curwp->w.dotp != curbp->b_linep) {
            if (!curline_empty())   /* GGR */
                curwp->w.dotp = lforw(curwp->w.dotp);
            else
                break;
        }

/* ...and then backward until we are in a word */
        suc = back_grapheme(1);
        while ((suc > 0) && at_whitespace()) suc = back_grapheme(1);
        curwp->w.doto = lused(curwp->w.dotp);   /* and to the EOL */
    }
    curwp->w_flag |= WFMOVE;    /* Force screen update */
    return TRUE;
}

/* Scroll forward by a specified number of lines, or by a full page if no
 * argument. Bound to "C-V". The "2" in the arithmetic on the window size is
 * the overlap; this value is the default overlap value in ITS EMACS. Because
 * this zaps the top line in the display window, we have to do a hard update.
 */
int backpage(int, int);     /* Forward declaration */
int forwpage(int f, int n) {
    struct line *lp;

    if (f == FALSE) {
        if (term.t_scroll != NULL)
            if (overlap == 0) n = curwp->w_ntrows / 3 * 2;
            else              n = curwp->w_ntrows - overlap;
        else
            n = curwp->w_ntrows - 2;    /* Default scroll. */
        if (n <= 0)                     /* Forget the overlap */
            n = 1;                      /* if tiny window. */
    } else if (n < 0)
        return backpage(f, -n);
    else                                /* Convert from pages */
        n *= curwp->w_ntrows;           /* to lines. */
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
    curwp->w.dotp = lp;
    curwp->w.doto = 0;
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
        if (n <= 0)     /* Don't blow up if the */
            n = 1;      /* window is tiny. */
    }
    else if (n < 0)
        return forwpage(f, -n);
    else                        /* Convert from pages */
        n *= curwp->w_ntrows;   /* to lines. */

    lp = curwp->w_linep;
    while (n-- && lback(lp) != curbp->b_linep) lp = lback(lp);
    curwp->w_linep = lp;
    curwp->w.dotp = lp;
    curwp->w.doto = 0;
    curwp->w_flag |= WFHARD | WFINS;
    return TRUE;
}

/* Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".
 */
int setmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    curwp->w.markp = curwp->w.dotp;
    curwp->w.marko = curwp->w.doto;
    mlwrite_one(MLbkt("Mark set"));
    return TRUE;
}

/* Swap the values of "." and "mark" in the current window. This is pretty
 * easy, because all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
int swapmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *odotp;
    int odoto;

    if (curwp->w.markp == NULL) {
        mlwrite_one("No mark in this window");
        return FALSE;
    }
    odotp = curwp->w.dotp;
    odoto = curwp->w.doto;
    curwp->w.dotp = curwp->w.markp;
    curwp->w.doto = curwp->w.marko;
    curwp->w.markp = odotp;
    curwp->w.marko = odoto;
    curwp->w_flag |= WFMOVE;
    return TRUE;
}
