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

/* Two potential methods.
 * The old way, which modifies the bottom window(s) size for the change.
 * The new way, which spreads the change across all windows.
 */
static void old_sizer(int n) {
    struct window *wp;      /* current window being examined */
    struct window *nextwp;  /* next window to scan */
    struct window *lastwp;  /* last window scanned */
    int lastline;           /* screen line of last line of current window */

    if (term.t_nrow < n) {

/* Find the bottom window... */
        wp = wheadp;
        while (wp->w_wndp != NULL) wp = wp->w_wndp;

/* Now enlarge the bottom window and force a redraw */
        wp->w_flag |= WFHARD | WFMODE;
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
    return;
}

/* The new method of screen height changing.
 * If we grap a border and drag it we'll get consecutive
 * in/decrements by 1. Which would all be applied to the first
 * window in the list. So remember increments-by-1 and add a skip if
 * we're still going in the same direction.
 */
static int skip = 0;
static int nsdir = 0;   /* Unknown */
static int last_count = 0;
static void new_sizer(int to_add) {
    struct window *wp;      /* current window being examined */

/* If the direction has changed, or the user has typed more input then
 * has changed we reset skip. Otherwise we remember the current skip
 * to work with and set skip to 1 (ie. non-zero).
 * Then, at the end of each while (wp != NULL) pass through the windows we
 * update skip to this_skip if both are non-zero
 * This means we end up with skip set to this_skip%n_of_windows
 * (without having to count the windows)
 */
    int thisdir = (to_add > 0)? 1: -1;
    if (nsdir == 0) nsdir = thisdir;            /* Initialize */
    if (skip == 0) last_count = inkey.count;    /* First skip in series? */
/* More in same direction? */
    int skip_more = 0;
    if ((last_count == inkey.count) && (nsdir == thisdir)) skip_more = 1;
    nsdir = thisdir;

    int this_skip;
    if (skip_more) {
        this_skip = skip;                       /* Starts at 0 */
        skip += (to_add > 0)? to_add: -to_add;  /* Increment for next call */
    }
    else {
        this_skip = 0;
        skip = 0;
    }

    if (to_add > 0) {
/* Add a line to each window from the top down, and keep going
 * back to the top, if required) until we have added to_add lines.
 */

/* Loop through windows adding 1 line to each until we've added
 * the requested number of lines.
 */
        while (to_add > 0) {
            wp = wheadp;
            int w_resized = 0;
            while (wp != NULL) {
                if (this_skip > 0) {
                    this_skip--;
                }
                else {
/* Enlarge this window by 1 line and push the top line down by
 * the number of windows above that have had a line added
 */
                    wp->w_toprow += w_resized;
                    if (to_add) {
                        wp->w_ntrows++;
                        to_add--;
                        w_resized++;
                    }
                    wp->w_flag |= WFHARD | WFMODE;
                }
                wp = wp->w_wndp;
            }
/* If a loop-pass leaves us with something to do, update next calls skip */
            if ((to_add > 0) && (this_skip > 0)) skip = this_skip + 1;
        }
    }
    else {
/* Have to remove lines (to_add is -ve). Harder....
 * We'll loop through windows removing a line from each if it has
 * at least 2 lines (so we always leave 1 buffer line + a modeline).
 * If that doesn't finish the job we'll then remove windows from the
 * bottom up.
 * No need for set_scrarray_size()/vtinit() as we're getting smaller.
 */
        int to_go = -to_add;        /* Just reads better... */
/* Loop through windows removing 1 line from each */

        while (to_go > 0) {
            wp = wheadp;
            int w_resized = 0;
            int some_skips = (this_skip > 0);
            while (wp != NULL) {
                if (this_skip > 0) {
                    this_skip--;
                }
                else {
/* Shrink this window by 1 line and raise the top line up by
 * the number of windows above which have had a line removed.
 */
                    wp->w_toprow -= w_resized;
                    if (to_go && wp->w_ntrows >= 2) { /* 1 buffer + modeline */
                        wp->w_ntrows--;
                        w_resized++;
                        to_go--;
                    }
                    wp->w_flag |= WFHARD | WFMODE;
                }
                wp = wp->w_wndp;
            }
/* If a loop-pass leaves us with something to do, update next calls skip */
            if ((to_go > 0) && (this_skip > 0)) skip = this_skip + 1;
            if (some_skips) continue;   /* Avoid next exit too early */
            if (w_resized == 0) break;  /* Unable to remove lines in pass */
        }

/* Did we handle all of to_go?
 * If not then we've shrunk all windows as far as possible, but that
 * still wasn't sufficient.
 * Now start removing windows from the end of the list.
 * NOTE that for the removal above to fail we must have at least 2
 * windows visible, so the wp->w_wndp loop will set a valid pwp and wp.
 */
        if (to_go > 0) skip = 0;    /* Top windows at min size anyway */
        while (to_go > 0) {         /* Might need multiple removals */
            struct window *pwp = NULL;
            wp = wheadp;
            while (wp->w_wndp) {    /* This loop *will* run */
                pwp = wp;
                wp = wp->w_wndp;
            }
            if (pwp == NULL) break; /* Shouldn't happen, but... */

/* Now wp is the last window and pwp the penultimate one.
 * We'll remove wp
 */

/* Save the point/mark if needed */
            if (--wp->w_bufp->b_nwnd == 0) wp->w_bufp->b = wp->w;

/* Update curwp and fix the penultimate pointer to be the last */
            if (wp == curwp) curwp = pwp;
            curbp = curwp->w_bufp;
            pwp->w_wndp = NULL;

/* Update the lines gone count to include this removed  window
 * and free the window structure.
 */
            to_go -= wp->w_ntrows + 1;  /* +1 for modeline */
            Xfree_setnull(wp);

/* Now, this means we may have removed too many lines.
 * If so, add this to the final window (which is pwp)
 */
            if (to_go < 0) {
                pwp->w_ntrows -= to_go; /* -, since to_go -s -ve */
                to_go = 0;
                pwp->w_flag |= WFHARD | WFMODE;
            }
        }
    }
    return;
}

/* The common entry point to old_sizer() and new_sizer() */

int newheight(int n) {

/* Make sure it's reasonable */
    if (n < 3 ) {
        mlwrite_one("Screen size too small");
        return FALSE;
    }

    int to_add = n - term.t_nrow;
    if (to_add == 0) return TRUE;   /* No change */

/* If we're growing we need to ensure we have sufficient v/pscreen space.
 * vtinit needs term.t_mcol/term.t_mrow set first.
 */
    if (n > term.t_mrow) {
        set_scrarray_size(n, term.t_ncol);
        vtinit();       /* Sets WFHARD and WFMODE flags on windows */
    }

    if (wheadp) {       /* Only if windows exist (so not at TTinit) */
/* NOTE that the methods take different args */
        if (ggr_opts&GGR_NEWHEIGHT) {
            new_sizer(to_add);
        }
        else {
            old_sizer(n);
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

/* Make sure it's reasonable.
 * We can't actually stop the user dragging the window smaller.
 * But we can warn, with a sufficiently short message
 */
    if (n < 10) {
        mlwrite_one("TOO SMALL");
        return FALSE;
    }

/* Ensure we have sufficient v/pscreen space.
 * vtinit needs term.t_mcol/term.t_mrow set first.
 */
    if (term.t_mcol < n) {
        set_scrarray_size(term.t_nrow, n);
        vtinit();
    }

/* Otherwise, just re-width it (no big deal).
 * t_margin is just a hueristic. Nothing special...
 */
    term.t_ncol = n;
    term.t_margin = 2 + n/40;
    term.t_scrsiz = n - (2*term.t_margin);

/* If the //List buffer is being shown, recalculate it for the new width */

    int update_blistp = 0;
    if (blistp && (blistp->b_nwnd > 0)) {
        makelist(-1); /* -1 == use last iflag */
        blistp->b_flag |= BFCHG;
        update_blistp = 1;
    }

/* Force all windows to redraw. Update blistp when we hit it */
    wp = wheadp;
    while (wp) {
        if (update_blistp && (wp->w_bufp == blistp)) {
            wp->w_linep = lforw(blistp->b_linep);
            wp->w.dotp = lforw(blistp->b_linep);
            wp->w.doto = 0;
            wp->w.markp = NULL;
            wp->w.marko = 0;
        }
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
