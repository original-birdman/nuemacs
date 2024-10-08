/*      window.c
 *
 *      Window management. Some of the functions are internal, and some are
 *      attached to keys that the user actually types.
 *
 */

#include <stdio.h>

#define WINDOW_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Reposition dot in the current window to line "n". If the argument is
 * positive, it is that line. If it is negative it is that line from the
 * bottom. If it is 0 the window is centered (this is what the standard
 * redisplay code does). With no argument it defaults to 0. Bound to M-!.
 */
int reposition(int f, int n) {
    if (f == FALSE)         /* default to 0 to center screen */
        n = 0;
    curwp->w_force = n;
    curwp->w_flag |= WFFORCE;
    return TRUE;
}

/* Refresh the screen. With no argument, it just does the refresh. With an
 * argument it recenters "." in the current window. Bound to "C-L".
 */
int redraw(int f, int n) {
    UNUSED(n);
    if (f == FALSE)
        sgarbf = TRUE;
    else {
        curwp->w_force = 0; /* Center dot. */
        curwp->w_flag |= WFFORCE;
    }

    return TRUE;
}

void cknewwindow(void) {
/* Don't start the handler when it is already running as that might
 * just get into a loop...
 * Also, don't do this if a macro is being generated (as it switches
 * to/from the keyboard macro buffer to log commands and may complain
 * about getting there from any user-proc buffer used here).
 * It's OK if the macro is being executed.
 */
    if (!meta_spec_active.X && !(kbdmode == RECORD)) {
        meta_spec_active.X = 1;
        execute(META|SPEC|'X', FALSE, 1);
        meta_spec_active.X = 0;
    }
}

/* The command make the next window (next => down the screen) the current
 * window. There are no real errors, although the command does nothing if
 * there is only 1 window on the screen. Bound to "C-X C-N".
 *
 * with an argument this command finds the <n>th window from the top
 *
 * int f, n;            default flag and numeric argument
 *
 */
int nextwind(int f, int n) {
    struct window *wp;
    int nwindows;   /* total number of windows */

    if (f) {
/* First count the # of windows */
        wp = wheadp;
        nwindows = 1;
        while (wp->w_wndp != NULL) {
            nwindows++;
            wp = wp->w_wndp;
        }

/* If the argument is negative, it is the nth window from the bottom of
 * the screen
 */
        if (n < 0) n = nwindows + n + 1;

/* If an argument, give them that window from the top */
        if (n > 0 && n <= nwindows) {
            wp = wheadp;
            while (--n) wp = wp->w_wndp;
        }
        else {
            mlwrite_one("Window number out of range");
            return FALSE;
        }
    }
    else
        if ((wp = curwp->w_wndp) == NULL) wp = wheadp;

    curwp = wp;
    curbp = wp->w_bufp;
    cknewwindow();
    upmode(NULL);
    return TRUE;
}

/* This command makes the previous window (previous => up the screen) the
 * current window. There aren't any errors, although the command does not do a
 * lot if there is 1 window.
 */
int prevwind(int f, int n) {
    struct window *wp1;
    struct window *wp2;

/* If we have an argument, we mean the nth window from the bottom */
    if (f) return nextwind(f, -n);

    wp1 = wheadp;
    wp2 = curwp;

    if (wp1 == wp2) wp2 = NULL;
    while (wp1->w_wndp != wp2) wp1 = wp1->w_wndp;
    curwp = wp1;
    curbp = wp1->w_bufp;
    cknewwindow();
    upmode(NULL);
    return TRUE;
}

/* This command moves the current window down by "arg" lines. Recompute the
 * top line in the window. The move up and move down code is almost completely
 * the same; most of the work has to do with reframing the window, and picking
 * a new dot. We share the code by having "move down" just be an interface to
 * "move up". Magic. Bound to "C-X C-N".
 */
int mvupwind(int, int);     /* Forward declaration */
int mvdnwind(int f, int n) {
    return mvupwind(f, -n);
}

/* Move the current window up by "arg" lines. Recompute the new top line of
 * the window. Look to see if "." is still on the screen. If it is, you win.
 * If it isn't, then move "." to center it in the new framing of the window
 * (this command does not really move "."; it moves the frame). Bound to
 * "C-X C-P".
 */
int mvupwind(int f, int n) {
    UNUSED(f);
    struct line *lp;
    int i;

    lp = curwp->w_linep;

    if (n < 0) {
        while (n++ && lp != curbp->b_linep) lp = lforw(lp);
    }
    else {
        while (n-- && lback(lp) != curbp->b_linep) lp = lback(lp);
    }

    curwp->w_linep = lp;
    curwp->w_flag |= WFHARD;    /* Mode line is OK. */

    for (i = 0; i < curwp->w_ntrows; ++i) {
        if (lp == curwp->w.dotp) return TRUE;
        if (lp == curbp->b_linep) break;
        lp = lforw(lp);
    }

    lp = curwp->w_linep;
    i = curwp->w_ntrows/2;
    while (i-- && lp != curbp->b_linep) lp = lforw(lp);
    curwp->w.dotp = lp;
    curwp->w.doto = 0;
    return TRUE;
}

/* This command makes the current window the only window on the screen. Bound
 * to "C-X 1". Try to set the framing so that "." does not have to move on the
 * display. Some care has to be taken to keep the values of dot and mark in
 * the buffer structures right if the destruction of a window makes a buffer
 * become undisplayed.
 */
int onlywind(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;
    struct line *lp;
    int i;

    while (wheadp != curwp) {
        wp = wheadp;
        wheadp = wp->w_wndp;
        if (--wp->w_bufp->b_nwnd == 0) wp->w_bufp->b = wp->w;
        Xfree(wp);
    }
    while (curwp->w_wndp != NULL) {
        wp = curwp->w_wndp;
        curwp->w_wndp = wp->w_wndp;
        if (--wp->w_bufp->b_nwnd == 0) wp->w_bufp->b = wp->w;
        Xfree(wp);
    }
    lp = curwp->w_linep;
    i = curwp->w_toprow;
    while (i != 0 && lback(lp) != curbp->b_linep) {
        --i;
        lp = lback(lp);
    }
    curwp->w_toprow = 0;
    curwp->w_ntrows = term.t_vscreen;   /* Ignoring mode-line */
    curwp->w_linep = lp;
    curwp->w_flag |= WFMODE | WFHARD;
    return TRUE;
}

/* Delete the current window, placing its space in the window above,
 * or, if it is the top window, the window below. Bound to C-X 0.
 *
 * int f, n;    arguments are ignored for this command
 */
int delwind(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;      /* window to recieve deleted space */
    struct window *lwp;     /* ptr window before curwp */
    int target;             /* target line to search for */

/* If there is only one window, don't delete it */
    if (wheadp->w_wndp == NULL) {
        mlwrite_one("Cannot delete this window");
        return FALSE;
    }

/* Find window before curwp in linked list */
    wp = wheadp;
    lwp = NULL;
    while (wp != NULL) {
        if (wp == curwp) break;
        lwp = wp;
        wp = wp->w_wndp;
    }

/* Find receiving window and give up our space */
    wp = wheadp;
    if (curwp->w_toprow == 0) {
/* Find the next window down */
        target = curwp->w_ntrows + 1;
        while (wp != NULL) {
            if (wp->w_toprow == target) break;
            wp = wp->w_wndp;
        }
        if (wp == NULL) return FALSE;
        wp->w_toprow = 0;
        wp->w_ntrows += target;
    } else {
/* Find the next window up */
        target = curwp->w_toprow - 1;
        while (wp != NULL) {
            if ((wp->w_toprow + wp->w_ntrows) == target) break;
            wp = wp->w_wndp;
        }
        if (wp == NULL) return FALSE;
        wp->w_ntrows += 1 + curwp->w_ntrows;
    }

/* Get rid of the current window */
    if (--curwp->w_bufp->b_nwnd == 0) curwp->w_bufp->b = curwp->w;
    if (lwp == NULL) wheadp = curwp->w_wndp;
    else             lwp->w_wndp = curwp->w_wndp;
    Xfree(curwp);
    curwp = wp;
    wp->w_flag |= WFHARD;
    curbp = wp->w_bufp;
    cknewwindow();
    upmode(NULL);
    return TRUE;
}

/* Split the current window.  A window smaller than 3 lines cannot be
 * split.  An argument of 1 forces the cursor into the upper window, an
 * argument of two forces the cursor to the lower window.  The only
 * other error that is possible is a "Xmalloc" failure allocating the
 * structure for the new window (whereupon the interlude in wrapper.c
 * will exit the program).
 * Bound to "C-X 2".
 *
 * int f, n;    default flag and numeric argument
 */
int splitwind(int f, int n) {
    struct window *wp;
    struct line *lp;
    int ntru;
    int ntrl;
    int ntrd;
    struct window *wp1;
    struct window *wp2;

    if (curwp->w_ntrows < 3) {
        mlwrite("Cannot split a %d line window", curwp->w_ntrows);
        return FALSE;
    }
    wp = Xmalloc(sizeof(struct window));
    ++curbp->b_nwnd;    /* Displayed twice.     */
    wp->w_bufp = curbp;
    wp->w = curwp->w;
    wp->w_flag = 0;
    wp->w_force = 0;
#if     COLOR
/* Set the colors of the new window */
    wp->w_fcolor = gfcolor;
    wp->w_bcolor = gbcolor;
#endif
    ntru = (curwp->w_ntrows - 1)/2;         /* Upper size */
    ntrl = (curwp->w_ntrows - 1) - ntru;    /* Lower size */
    lp = curwp->w_linep;
    ntrd = 0;
    while (lp != curwp->w.dotp) {
        ++ntrd;
        lp = lforw(lp);
    }
    lp = curwp->w_linep;
    if (((f == FALSE) && (ntrd <= ntru)) || ((f == TRUE) && (n == 1))) {
/* Old is upper window. */
        if (ntrd == ntru) lp = lforw(lp);   /* Hit mode line. */
        curwp->w_ntrows = ntru;
        wp->w_wndp = curwp->w_wndp;
        curwp->w_wndp = wp;
        wp->w_toprow = curwp->w_toprow + ntru + 1;
        wp->w_ntrows = ntrl;
    } else {
/* Old is lower window  */
        wp1 = NULL;
        wp2 = wheadp;
        while (wp2 != curwp) {
            wp1 = wp2;
            wp2 = wp2->w_wndp;
        }
        if (wp1 == NULL) wheadp = wp;
        else             wp1->w_wndp = wp;
        wp->w_wndp = curwp;
        wp->w_toprow = curwp->w_toprow;
        wp->w_ntrows = ntru;
        ++ntru;          /* Mode line. */
        curwp->w_toprow += ntru;
        curwp->w_ntrows = ntrl;
        while (ntru--)
        lp = lforw(lp);
    }
    curwp->w_linep = lp; /* Adjust the top lines */
    wp->w_linep = lp;    /* if necessary.        */
    curwp->w_flag |= WFMODE | WFHARD;
    wp->w_flag |= WFMODE | WFHARD;
    return TRUE;
}

/* Shrink the current window. Find the window that gains space. Hack at the
 * window descriptions. Ask the redisplay to do all the hard work. Bound to
 * "C-X C-Z".
 */
int enlargewind(int, int);  /* Forward declaration */
int shrinkwind(int f, int n) {
    struct window *adjwp;
    struct line *lp;
    int i;

    if (n < 0) return enlargewind(f, -n);
    if (wheadp->w_wndp == NULL) {
        mlwrite_one("Only one window");
        return FALSE;
    }
    if ((adjwp = curwp->w_wndp) == NULL) {
        adjwp = wheadp;
        while (adjwp->w_wndp != curwp) adjwp = adjwp->w_wndp;
    }
    if (curwp->w_ntrows <= n) {
        mlwrite_one("Impossible change");
        return FALSE;
    }
    if (curwp->w_wndp == adjwp) {   /* Grow below */
        lp = adjwp->w_linep;
        for (i = 0; i < n && lback(lp) != adjwp->w_bufp->b_linep; ++i)
            lp = lback(lp);
        adjwp->w_linep = lp;
        adjwp->w_toprow -= n;
    }
    else {                          /* Grow above */
        lp = curwp->w_linep;
        for (i = 0; i < n && lp != curbp->b_linep; ++i)
            lp = lforw(lp);
        curwp->w_linep = lp;
        curwp->w_toprow += n;
    }
    curwp->w_ntrows -= n;
    adjwp->w_ntrows += n;
    curwp->w_flag |= WFMODE | WFHARD | WFKILLS;
    adjwp->w_flag |= WFMODE | WFHARD | WFINS;
    return TRUE;
}

/* Enlarge the current window. Find the window that loses space. Make sure it
 * is big enough. If so, hack the window descriptions, and ask redisplay to do
 * all the hard work. You don't just set "force reframe" because dot would
 * move. Bound to "C-X Z".
 */
int enlargewind(int f, int n) {
    struct window *adjwp;
    struct line *lp;
    int i;

    if (n < 0) return shrinkwind(f, -n);
    if (wheadp->w_wndp == NULL) {
        mlwrite_one("Only one window");
        return FALSE;
    }
    if ((adjwp = curwp->w_wndp) == NULL) {
        adjwp = wheadp;
        while (adjwp->w_wndp != curwp) adjwp = adjwp->w_wndp;
    }
    if (adjwp->w_ntrows <= n) {
        mlwrite_one("Impossible change");
        return FALSE;
    }
    if (curwp->w_wndp == adjwp) {   /* Shrink below.        */
        lp = adjwp->w_linep;
        for (i = 0; i < n && lp != adjwp->w_bufp->b_linep; ++i)
            lp = lforw(lp);
        adjwp->w_linep = lp;
        adjwp->w_toprow += n;
    }
    else {                          /* Shrink above.        */
        lp = curwp->w_linep;
        for (i = 0; i < n && lback(lp) != curbp->b_linep; ++i)
            lp = lback(lp);
        curwp->w_linep = lp;
        curwp->w_toprow -= n;
    }
    curwp->w_ntrows += n;
    adjwp->w_ntrows -= n;
    curwp->w_flag |= WFMODE | WFHARD | WFINS;
    adjwp->w_flag |= WFMODE | WFHARD | WFKILLS;
    return TRUE;
}

/* Resize the current window to the requested size
 *
 * int f, n;            default flag and numeric argument
 */
int resize(int f, int n) {
    int clines;     /* current # of lines in window */

/* Must have a non-default argument, else ignore call */
    if (f == FALSE) return TRUE;

/* Find out what to do */
    clines = curwp->w_ntrows;

/* Already the right size? */
    if (clines == n) return TRUE;

    return enlargewind(TRUE, n - clines);
}

/* Pick a window for a pop-up. Split the screen if there is only one window.
 * Pick the uppermost window that isn't the current window. An LRU algorithm
 * might be better. Return a pointer, or NULL on error.
 */
struct window *wpopup(void) {
    struct window *wp;

    if (wheadp->w_wndp == NULL              /* Only 1 window... */
          && splitwind(FALSE, 0) == FALSE)  /* and it won't split */
        return NULL;
    wp = wheadp;    /* Find window to use */
    while (wp != NULL && wp == curwp) wp = wp->w_wndp;
    return wp;
}

/* Scroll the next window up (back) a page */
int scrnextup(int f, int n) {
    nextwind(FALSE, 1);
    backpage(f, n);
    prevwind(FALSE, 1);
    return TRUE;
}

/* Scroll the next window down (forward) a page */
int scrnextdw(int f, int n) {
    nextwind(FALSE, 1);
    forwpage(f, n);
    prevwind(FALSE, 1);
    return TRUE;
}

/* save ptr to current window */
int savewnd(int f, int n) {
    UNUSED(f); UNUSED(n);
    swindow = curwp;
    return TRUE;
}

/* Restore the saved screen */
int restwnd(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;

/* Find the window */
    wp = wheadp;
    while (wp != NULL) {
        if (wp == swindow) {
            curwp = wp;
            curbp = wp->w_bufp;
            upmode(NULL);
            return TRUE;
        }
        wp = wp->w_wndp;
    }

    mlwrite_one(MLbkt("No such window exists"));
    return FALSE;
}

/* Resize the screen height, re-writing the screen
 *
 * int n;       new screen height to set
 */
int newsize(int n) {
    struct window *wp;      /* current window being examined */
    struct window *nextwp;  /* next window to scan */
    struct window *lastwp;  /* last window scanned */
    int lastline;           /* screen line of last line of current window */

/* make sure it's reasonable */
    if (n < 3 ) {
        mlwrite_one("Screen size too small");
        return FALSE;
    }

    if (term.t_nrow == n)
        return TRUE;
    else if (term.t_nrow < n) {

/* Find the bottom window... */
        wp = wheadp;
        while (wp->w_wndp != NULL) wp = wp->w_wndp;

/* Ensure we have sufficient v/pscreen space.
 * vtinit needs term.t_mcol/term.t_mrow set first.
 */
        if (term.t_mrow < n) {
            set_scrarray_size(n, term.t_ncol);
            vtinit();       /* Sets WFHARD and WFMODE flags on windows */
        }
        else                /* Nust Force redraw bottom window */
            wp->w_flag |= WFHARD | WFMODE;
/* Now enlarge the bottom window */
        wp->w_ntrows = n - wp->w_toprow - 2;
    } else {
/* Rebuild the window structure */
        nextwp = wheadp;
        wp = NULL;
        lastwp = NULL;
        while (nextwp != NULL) {
            wp = nextwp;
            nextwp = wp->w_wndp;

/* Get rid of it if it is too low */
            if (wp->w_toprow > n - 2) {

/* Save the point/mark if needed */
                if (--wp->w_bufp->b_nwnd == 0) wp->w_bufp->b = wp->w;

/* Update curwp and lastwp if needed */
                if (wp == curwp) curwp = wheadp;
                curbp = curwp->w_bufp;
                if (lastwp != NULL) lastwp->w_wndp = NULL;

/* Free the structure */
                Xfree_setnull(wp);
            } else {
/* Need to change this window size? */
                lastline = wp->w_toprow + wp->w_ntrows - 1;
                if (lastline >= n - 2) {
                    wp->w_ntrows = n - wp->w_toprow - 2;
                    wp->w_flag |= WFHARD | WFMODE;
                }
            }
            lastwp = wp;
        }
    }

/* Set term.t_nrow and all related vars now */
    SET_t_nrow(n);

/* screen is garbage */
    sgarbf = TRUE;
    return TRUE;
}

/* Resize the screen width, re-writing the screen
 *
 * int n;           new width to set
 */
int newwidth(int n) {
    struct window *wp;

/* Make sure it's in reasonable */
    if (n < 10) {
        mlwrite_one("Screen width too small");
        return FALSE;
    }

/* Ensure we have sufficient v/pscreen space.
 * vtinit needs term.t_mcol/term.t_mrow set first.
 */
    if (term.t_mrow < n) {
        set_scrarray_size(term.t_nrow, n);
        vtinit();
    }

/* Otherwise, just re-width it (no big deal).
 * t_margin is just a hueristic. Nothing special...
 */
    term.t_ncol = n;
    term.t_margin = 2 + n/40;
    term.t_scrsiz = n - (2*term.t_margin);

/* Force all windows to redraw */
    wp = wheadp;
    while (wp) {
        wp->w_flag |= WFHARD | WFMOVE | WFMODE;
        wp = wp->w_wndp;
    }
    sgarbf = TRUE;

    return TRUE;
}

/* Get screen offset of current line in current window */
int getwpos(void) {
    int sline;          /* screen line from top of window */
    struct line *lp;    /* scannile line pointer */

/* Search down the line we want */
    lp = curwp->w_linep;
    sline = 1;
    while (lp != curwp->w.dotp) {
        ++sline;
        lp = lforw(lp);
    }

/* And return the value */
    return sline;
}
