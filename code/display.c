/*      display.c
 *
 *      The functions in this file handle redisplay. There are two halves, the
 *      ones that update the virtual display screen, and the ones that make the
 *      physical display screen the same as the virtual display screen. These
 *      functions use hints that are left in the windows by the commands.
 *
 *      Modified by Petri Kutvonen
 */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#define DISPLAY_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "version.h"
#include "utf8.h"

#include "utf8proc.h"

static int mbonly = FALSE;      /* GGR - minibuffer only */
static int taboff = 0;          /* tab offset for display       */

/* The display information is buffered.
 * We fill in vscreen as we go (with vtrow and vtcol) then this
 * is copied (in update()) to pscreen, which is sent to the
 * display with TTputc calls, with ttcol tracked by other TTput* calls.
 * Only this source file needs to know about the data.
 *
 * Any malloc for extended grapheme data are dealt with as entries
 * are put into (malloc) or removed from (free) vscreen ONLY!
 * This means we can just copy (clone) entries ffrom vscreen to pscreen
 * overwriting (discarding) anything that is there. No mallocs or frees
 * are needed for pscreen entries.
 */
static int vtrow = 0;                  /* Row location of SW cursor */
static int vtcol = 0;                  /* Column location of SW cursor */

struct video {
    int v_flag;                 /* Flags */
    struct grapheme v_text[0];  /* Screen data - dynamic    */
};

#define VFCHG   0x0001          /* Changed flag                 */
#define VFEXT   0x0002          /* extended (beyond column 80)  */
#define VFHIL   0x0004          /* highlight status             */
#define VFHIQ   0x0008          /* highlight request            */

static struct video **vscreen;          /* Virtual screen. */
static struct video **pscreen;          /* Physical screen. */
static void *vdata;                     /* Where we've stored it all */

static struct grapheme blank_gph = { ' ', 0, NULL };

static int displaying = FALSE;
static int delayed_update = FALSE;

#include <signal.h>

#ifdef SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#include <sys/ioctl.h>

/* Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void movecursor(int row, int col) {
    if (row != ttrow || col != ttcol) {
        ttrow = row;
        ttcol = col;
        TTmove(row, col);
    }
}
void force_movecursor(int row, int col) {
    ttrow = -1;         /* Force the optimizing test to fail */
    movecursor(row, col);
}

/* Set the entry to an Unicode character.
 * Checks for previous extended cdm usage and frees any such found
 * (no longer has a "no_free" flag, as no such callers remain).
 * Internal to this file.
 */
static void update_grapheme(struct grapheme *gp, unicode_t uc) {
    gp->uc = uc;
    gp->cdm = 0;
    if (gp->ex != NULL) {
        Xfree(gp->ex);
        gp->ex = NULL;
    }
    return;
}

/* Add a unicode character as a cdm or dynamic ex entry
 */
static void extend_grapheme(struct grapheme *gp, unicode_t uc) {

    if (gp->cdm == 0) {         /* None there yet - simple */
        gp->cdm = (unicode_t)uc;
        return;
    }
/* Need to create or extend an ex section */
    int xc = 0;
    if (gp->ex != NULL) {
        while(gp->ex[xc] != UEM_NOCHAR) xc++;
    }
    gp->ex = Xreallocarray(gp->ex, xc+2, sizeof(unicode_t));
    gp->ex[xc] = uc;
    gp->ex[xc+1] = UEM_NOCHAR;
    return;
}

/* Copy grapheme data between structures.
 * HOWEVER, since this *always* copies from vscreen to pscreen we DO NOT
 * NEED to allocate any ex section in the target, as we never free
 * anything from pscreen - it will be freed (by update_grapheme) when the
 * vscreen is set! We just copy the pointer, so copy the whole structure.
 * Is now just be defined as:
 *      #define clone_grapheme(to, from)   *to = *from
 * but we could inline it instead.
 * The code is left here just in case we ever need to do something different...
 */
#if 0
static inline void
  clone_grapheme(struct grapheme *gtarget, struct grapheme *gsource) {
    *gtarget = *gsource;
    return;
}
#else
#define clone_grapheme(to, from)   *to = *from
#endif

/* This now does a case-sensitive check in same_grapheme() */
static int
  same_grapheme_array(struct grapheme *gp1, struct grapheme *gp2, int nelem) {
    for (int i = 0; i < nelem; i++) {
        if (!same_grapheme(gp1+i, gp2+i, 0)) return FALSE;
    }
    return TRUE;
}

/* #define to check whether we have a space */
#define is_space(gp) (((gp)->uc == ' ') && ((gp)->cdm == 0))

/* Output a grapheme - which is in one column.
 * Handle remapping on the main character.
 */
static void TTputgrapheme(struct grapheme *gp) {
    if (gp->uc) {   /* NUL bytes indicate no output, but do increment ttcol */
        TTputc(display_for(gp->uc));
        if (gp->cdm) TTputc(gp->cdm);   /* Might add display_for here too */
        if (gp->ex != NULL) {
            for(unicode_t *zw = gp->ex; *zw != UEM_NOCHAR; zw++) {
                TTputc(*zw);            /* Might add display_for here too */
            }
        }
    }
    ttcol++;
}

/* Routine for use by mlwrite*+mlput* routines so that nothing
 * is printed beyond the last column, to prevent the message line
 * wrapping and messing up the display.
 * It also keeps a full record of what is there, so it can be re-written
 * (by mlrewrite()) after a SIGWINCH.
 * NOTE that it takes a unicode_t arg, but calling it with a normal
 * ASCII char will work, as that gets promoted to an int (and we have
 * unsigned chars).
 * This routine MUST be used for all message line output (because of
 * the rewrite-on-SIGWINCH ability).
 *
 * The count of valid chars in ml_text (ml_text_offset) is a global,
 * so that it can be checked for whether there is any text available
 * (which will be redrawn on a SIGWINCH, even if not currently displayed).
 */
#define ML_TEXT_INCR 128
static int *ml_text = NULL;     /* Save the message line contents here */
static int ml_text_alloc = 0;   /* Current allocated size */
static int ml_rewriting = 0;    /* 1 when re-writing the buffer */
static int ml_prompt = 0;       /* 1 for a query (stay in message line) */

static void mlout_uc(unicode_t uc) {
    if (!ml_rewriting) {        /* No recording on a rewrite */
        if (ml_text_offset >= ml_text_alloc) {
            ml_text_alloc += ML_TEXT_INCR;
            ml_text = Xreallocarray(ml_text, ml_text_alloc, sizeof(*ml_text));
        }
        ml_text[ml_text_offset++] = uc;
    }
/* Do we need to replace this char for output? */
    unicode_t act_uc = display_for(uc);
    int cw = (combining_type(act_uc))? 0: utf8char_width(act_uc);
/* Do we have space for this */
    if (ttcol < term.t_ncol) {  /* No output if > R/h column */
        if ((ttcol + cw) >= term.t_ncol) {  /* No */
            act_uc = ovflw;
            cw = 1;     /* Width of overflow char */
        }
        TTputc(act_uc);
    }
    ttcol += cw;
}

/* Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up. The operating system's terminal I/O
 * channel is set up. All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get completely
 * redrawn on the first call to "update".
 */
static int prev_mrow = 0;
static int prev_mcol = 0;
static int prev_size = 0;
void vtinit(void) {
    int i, xi;
    struct video *vp;
    struct video **new_vscreen;     /* Virtual screen. */
    struct video **new_pscreen;     /* Physical screen. */
    void *new_vdata;                /* Where we've stored it all */

    if (prev_mrow == 0) {
        TTopen();               /* open the screen */
        TThilite(FALSE);
    }

/* Is this for a larger request? If not, nothing to do.
 * NOTE that we never make a change if the current allocation is
 * sufficient in both directions, but that it IS possible for one dimension
 * to shrink if other one rises.
 * The code allows for this (it frees from the previous size then
 * initializes the new sizes).
 */
    if ((term.t_mrow <= prev_mrow) && (term.t_mcol <= prev_mcol)) return;

/* Allocate the 2 screen arrays */
    new_vscreen = Xmalloc((unsigned)term.t_mrow*sizeof(struct video *));
    new_pscreen = Xmalloc((unsigned)term.t_mrow*sizeof(struct video *));

/* Allocate the data for these 2 arrays in one go and
 * assign the array elements in loops.
 */
    size_t row_size =
         sizeof(struct video) + (unsigned)term.t_mcol*sizeof(struct grapheme);
    new_vdata = Xmalloc(2 * (unsigned)term.t_mrow*row_size);
    char *vdp = new_vdata;
    char *pdp = new_vdata + ((unsigned)term.t_mrow*row_size);

    for (i = 0; i < term.t_mrow; i++) {
        new_vscreen[i] = (void *)vdp;
        vdp += row_size;
        new_pscreen[i] = (void *)pdp;
        pdp += row_size;
    }

/* We set any visible windows to be marked for a total redraw after
 * a call (except at startup).
 * So "all" we need to do is:
 *  free any grapheme ex parts in the current vscreen
 *  set new_vscreen to space graphemes
 *  set new_pscreen to zeroes
 *
 * So:
 * Free any grapheme ex parts in the current vscreen
 */
    for (i = 0; i < prev_mrow; i++) {
        vp = vscreen[i];
        for (xi = 0; xi < prev_mcol; xi++) {
            Xfree(vp->v_text[xi].ex);
        }
    }
/* Set new_vscreen to space graphemes by creating the first line then
 * copying this into all succeeding lines.
 */
    vp = new_vscreen[0];
    vp->v_flag = 0;
    for (i = 0; i < term.t_mcol; i++) vp->v_text[i] = blank_gph;
    for (i = 1; i < term.t_mrow; i++) {
        memcpy(new_vscreen[i], vp, row_size);
    }
/* Set new_pscreen to zeroes */
    memset(new_pscreen[0], 0, (unsigned)term.t_mrow*row_size);

/* Now free any previous data (NOTE that any gc->ex parts have already been
 * freed) and move the new allocations to the live ones.
 */
    if (prev_mrow) {    /* We have previous data to free */
        Xfree(vscreen);
        Xfree(pscreen);
        Xfree(vdata);
    }
    vdata = new_vdata;
    vscreen = new_vscreen;
    pscreen = new_pscreen;
/* The current set will be the previous values if we return here. */
    prev_mrow = term.t_mrow;
    prev_mcol = term.t_mcol;
    prev_size = (int)row_size;

/* Ensure any visible windows get redrawn from scratch.... */

    if (wheadp) {       /* Only if it's been set-up! */
        struct window *wp = wheadp;
        while (wp->w_wndp != NULL) {
            wp->w_flag |= WFHARD | WFMODE;
            wp = wp->w_wndp;
        }
    }
    return;
}

/* Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 * The ml_text buffer is cleared unless we are currently handling
 * SIGWINCH (when updates will call mlerase(), but we want to reshow it)
 * and when rewriting itself (when we call mlerase() to start with an
 * empty message line on the display).
 */
void mlerase(void) {

/* Reset current ml_text? */

    if (!handling_sigwinch && !ml_rewriting) ml_text_offset = 0;

    movecursor(term.t_mbline, 0);
    if (discmd == FALSE) return;

    TTeeol();   /* ttcol is already 0 from movecursor call */
    TTflush();
}

/* Clean up the virtual terminal system, in anticipation for a return to the
 * operating system. Move down to the last line and clear it out (the next
 * system prompt will be written in the line). Shut down the channel to the
 * terminal.
 */
void vttidy(void) {
    mlerase();      /* Moves cursor to (term.t_mbline, 0) */
    TTputc('\r');
    TTclose();
}

/* Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values; this might be a good
 * idea during the early stages.
 * Now just a simple #define.
 */
#define vtmove(row, col) {vtrow = row; vtcol = col;}

/* Write a character to the virtual screen. The virtual row and
 * column are updated. If we are not yet on left edge, don't print
 * it yet. If the line is too long put a "$" in the last column.
 *
 * This routine only puts printing characters into the virtual
 * terminal buffers. Only column overflow is checked.
 */
static void vtputc(unicode_t c) {
    struct video *vp;   /* ptr to line being updated */

/* If we are already past the R/h edge then we don't care, so do nothing */

    if (vtcol > term.t_ncol) return;

    if (c > MAX_UNICODE_CHAR) c = display_for(c);

    vp = vscreen[vtrow];

    if (combining_type((unicode_t)c)) {
/* Only extend a grapheme if we have a prev-char within screen width */
        if (vtcol > 0 && (vtcol <= term.t_ncol)) {
            extend_grapheme(&(vp->v_text[vtcol-1]), c);
        }
/* If we have a combining char as the first on a line (!?!) then
 * "pretend" there is a space there for it to combine with, but only
 * for display. It's not in the line buffer (so won't be written out)
 * although the update_screenpos_for_char() macro in line.h also needs
 * to handle it.
 */
        if (vtcol == 0) {
            update_grapheme(&(vp->v_text[0]), ' ');
            extend_grapheme(&(vp->v_text[0]), c);
            ++vtcol;
        }
        return;     /* Nothing else to do... no vtcol update */
    }

    if (c == '\t') {
        do {
            vtputc(' ');
        } while (((vtcol + taboff) & tabmask) != 0);
        return;
    }

/* NOTE: Unicode has characters for displaying Control Characters
 *  U+2400 to U+241F (so 2400+c)
 */
    if (c < 0x20) {
        if (nodisplay == 0) {
            vtputc(0x2400 + c);
        }
        else if (nodisplay == -1) {
            vtputc('^');
            vtputc(c ^ 0x40);
        }
        else vtputc(nodisplay);
        return;
    }

/* NOTE: Unicode has a character for displaying Delete.
 *  U+2421
 */
    if (c == 0x7f) {
        if (nodisplay == 0) {
            vtputc(0x2421);
        }
        else if (nodisplay == -1) {
            vtputc('^');
            vtputc('?');
        }
        else vtputc(nodisplay);
        return;
    }

    if (c >= 0x80 && c <= 0xa0) {
        static const char hex[] = "0123456789abcdef";
        vtputc('\\');
        vtputc(hex[c >> 4]);
        vtputc(hex[c & 15]);
        return;
    }

/* Get the character width. If it's > 1 we'll need to put NUL-byte padding
 * in so that the next character goes into the correct column.
 * If a multi-width char would extend beyond the current width, we don't
 * display it even though its start would be within range.
 * We'll display to the R/h column, but if we would display beyond that
 * not display this char, but instead update the preceding char to the
 * overflow char. We will still update vtcol so this routine does no
 * further work for any other chars on this line.
 */
    int cw = utf8char_width(c);
    if (vtcol >= 0) {
        if ((vtcol + cw) > term.t_ncol) { /* It won't fit - show overflow */

/* There are a few scenarios here.
 *  o we are adding simple 1-width chars beyond a 1-width char
 *      set previous char to simple ovflw
 *  o we are adding simple 1-width chars beyond a double-width chars
 *      set previous 2 chars to space + ovflw.
 *  o we are adding a double width char and have overflowed by 1
 *      just add a simple ovflw (no previous ovflw edit required)
 *  o we are adding a double-width char and have overflowed by 2
 *      we have to replace any preceding double width char with
 *      a space, then put an ovflw in the last column.
 */
            if (cw == 1) {

/* If the last-displayed char was a double width then it will have
 * been given a NUL-pad char (and we can't have a NUL for any other reason).
 * So then we put a space and overflow into the final 2 columns.
 */
                if (vp->v_text[vtcol-1].uc == 0) {
                    update_grapheme(&(vp->v_text[vtcol-2]), ' ');
                }
                update_grapheme(&(vp->v_text[vtcol-1]), ovflw);
            }
            else {  /* cw == 2 */
                int ovc = (vtcol + cw) - term.t_ncol;   /* Overflow amount */
/* If we've overflowed by 1 then the final column is available for us
 * to insert the overflow character.
 */
                if (ovc == 1) {
                    update_grapheme(&(vp->v_text[vtcol]), ovflw);
                }
/* If we've overflowed by 2 then if the last character was a single width
 * one we can just replace it with the overflow character.
 * BUT if it was a double width we have to replace that with a
 * space + overflow
 */
                else {  /* ovc == 2 */
                    if (vp->v_text[vtcol-1].uc == 0)
                        update_grapheme(&(vp->v_text[vtcol-2]), ' ');
                    update_grapheme(&(vp->v_text[vtcol-1]), ovflw);
                }
            }
        }
        else {
            update_grapheme(&(vp->v_text[vtcol]), c);
            if (cw > 1) {
/* This code assumes that a real NUL byte will not be displayed */
                int pvcol = vtcol;
                int nulpad = cw - 1;
                while(nulpad-- > 0) update_grapheme(&(vp->v_text[++pvcol]), 0);
            }
        }
    }
/* If vtcol is -ve, but will be +ve after the cw increment we need to space
 * pad the start of the display
 */
    else {
        for (int pcol = vtcol + cw; pcol > 0; pcol--) {
            update_grapheme(&(vp->v_text[pcol-1]), ' ');
        }
    }

    vtcol += cw;
}

/* Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 * Since this is a vscreen allocation we need to ensure we free any
 * pre-existing gc.ex entries.
 * vtcol can be -ve (horizontal scrolling), so check for that!
 */
static void vteeol(void) {
    struct grapheme *vcp = vscreen[vtrow]->v_text;
    if (vtcol < 0) vtcol = 0;
    while (vtcol < term.t_ncol) update_grapheme(&(vcp[vtcol++]), ' ');
}

void update(int);           /* Forward declaration */

/* upscreen:
 *      user routine to force a screen update
 *      always finishes complete update
 */
int upscreen(int f, int n) {
    UNUSED(f); UNUSED(n);
    update(TRUE);
    return TRUE;
}

/* updgar:
 *      if the screen is garbage, clear the physical screen and
 *      the virtual screen and force a full update
 */
static void updgar(void) {
    struct grapheme *txt;
    int i, j;

/* GGR - include the last row (for mini-buffer), so <=. */
    int lrow = inmb? term.t_mbline: term.t_vscreen;
    for (i = 0; i <= lrow; ++i) {
        vscreen[i]->v_flag |= VFCHG;
        vscreen[i]->v_flag &= ~VFHIL;

/* We only ever free the extended parts from the virtual screen info, not the
 * physical one.
 * This is a pscreen, no need to worry about freeing any ex field.
 */
        txt = pscreen[i]->v_text;
        for (j = 0; j < term.t_ncol; ++j) txt[j] = blank_gph;
    }

    movecursor(0, 0);       /* Erase the screen. */
    TTeeop();
    sgarbf = FALSE;         /* Erase-page clears */
    ml_rewriting = 1;       /* So we only wipe screen, not buffer */
    mlerase();              /* Ensure it is cleared */
    ml_rewriting = 0;
}

static int scrflags = 0;

/* reframe:
 *      check to see if the cursor is on in the window
 *      and re-frame it if needed or wanted
 */
static int reframe(struct window *wp) {
    struct line *lp;
    struct line *lp0;
    int i = 0;

/* If not a requested reframe, check for a needed one */
    if ((wp->w_flag & WFFORCE) == 0) {
/* Loop from one line above the window to one line after */
        lp = wp->w_linep;
        lp0 = lback(lp);
        if (lp0 == wp->w_bufp->b_linep)
            i = 0;
        else {
            i = -1;
            lp = lp0;
        }
        for (; i <= (int) (wp->w_ntrows); i++) {
/* If the line is in the window, no reframe */
            if (lp == wp->w.dotp) {
/* If not _quite_ in, we'll reframe gently */
                if (i < 0 || i == wp->w_ntrows) {
                    break;
                }
                return TRUE;
            }

/* If we are at the end of the file, reframe */
            if (lp == wp->w_bufp->b_linep) break;

/* On to the next line */
            lp = lforw(lp);
        }
    }
    if (i == -1) {                  /* we're just above the window */
        i = scrolljump;             /* put dot at first line */
        scrflags |= WFINS;
    }
    else if (i == wp->w_ntrows) {   /* we're just below the window */
        i = -scrolljump;            /* put dot at last line */
        scrflags |= WFKILLS;
    }
    else                            /* put dot where requested */
        i = wp->w_force;            /* (is 0 unless reposition() was called) */

    wp->w_flag |= WFMODE;

/* How far back to reframe? */
    if (i > 0) {            /* only one screen worth of lines max */
        if (--i >= wp->w_ntrows) i = wp->w_ntrows - 1;
    }
    else if (i < 0) {       /* negative update???? */
        i += wp->w_ntrows;
        if (i < 0) i = 0;
    }
    else
        i = wp->w_ntrows / 2;

/* Backup to new line at top of window */
    lp = wp->w.dotp;
    while (i != 0 && lback(lp) != wp->w_bufp->b_linep) {
        --i;
        lp = lback(lp);
    }

/* and reset the current line at top of window */
    wp->w_linep = lp;
    wp->w_flag |= WFHARD;
    wp->w_flag &= ~WFFORCE;
    return TRUE;
}

static void show_line(struct line *lp) {
    int i = 0, len = lused(lp);
/* Only runs through loop if there is text, so ltext() is OK. */
    while (i < len) {
        unicode_t c;
        i += utf8_to_unicode(ltext(lp), i, len, &c);
        vtputc(c);
    }
}

/* Map a char string with (possibly) utf8 sequences in it to unicode
 * for vtputc.
 */
static void show_utf8(const char *utf8p) {
    int i = 0, len = istrlen(utf8p);
    while (i < len) {
        unicode_t c;
        i += utf8_to_unicode(utf8p, i, len, &c);
        vtputc(c);
    }
}

/* updone:
 *      update the current line to the virtual screen
 *
 * struct window *wp;           window to update current line in
 */
static void updone(struct window *wp) {
    struct line *lp;        /* line to update */
    int sline;              /* physical screen line to update */

/* Search down the line we want */
    lp = wp->w_linep;
    sline = wp->w_toprow;
    while (lp != wp->w.dotp) {
        ++sline;
        lp = lforw(lp);
    }

/* And update the virtual line */
    vscreen[sline]->v_flag |= VFCHG;
    vscreen[sline]->v_flag &= ~VFHIQ;
    taboff = wp->w.fcol;
    vtmove(sline, -taboff);
    show_line(lp);
    vteeol();
    taboff = 0;
}

/* updall:
 *      update all the lines in a window on the virtual screen
 *
 * struct window *wp;           window to update lines in
 */
static void updall(struct window *wp) {
    struct line *lp;        /* line to update */
    int sline;              /* physical screen line to update */

/* Search down the lines, updating them */
    lp = wp->w_linep;
    sline = wp->w_toprow;
    taboff = wp->w.fcol;
    while (sline < wp->w_toprow + wp->w_ntrows) {

/* And update the virtual line */
        vscreen[sline]->v_flag |= VFCHG;
        vscreen[sline]->v_flag &= ~VFHIQ;
        vtmove(sline, -taboff);
        if (lp != wp->w_bufp->b_linep) {    /* if we are not at the end */
            show_line(lp);
            lp = lforw(lp);
        }

/* On to the next one
 * vteeol() makes sure we are on the screen
 */
        vteeol();
        ++sline;
    }
    taboff = 0;
}

/* Move the "count" lines starting at "from" to "to" */
static void scrscroll(int from, int to, int count) {
    ttrow = ttcol = -1;
    TTscroll(from, to, count);
}

/* return TRUE on text match
 *
 * int vrow, prow;              virtual, physical rows
 */
static int texttest(int vrow, int prow) {
    struct video *vpv = vscreen[vrow];      /* virtual screen image */
    struct video *vpp = pscreen[prow];      /* physical screen image */

    return same_grapheme_array(vpv->v_text, vpp->v_text, term.t_ncol);
}

/* return the index of the first blank of trailing whitespace
 */
static int endofline(struct grapheme *s, int n) {
    for (int i = n - 1; i >= 0; i--)
         if (!is_space(s+i)) return i + 1;
    return 0;
}

/* optimize out scrolls (line breaks, and newlines)
 * arg. chooses between looking for inserts or deletes
 */
static int scrolls(int inserts) {   /* returns true if it does something */
    struct video *vpv;              /* virtual screen image */
    struct video *vpp;              /* physical screen image */
    int i, j, k;
    int rows, cols;
    int first, match, count, target, end;
    int longmatch, longcount;
    int from, to;

    rows = term.t_mbline;           /* First line to ignore */
    cols = term.t_ncol;

    first = -1;
    for (i = 0; i < rows; i++) {    /* Find first wrong line */
        if (!texttest(i, i)) {
            first = i;
            break;
        }
    }
    if (first < 0) return FALSE;    /* No text changes */

    vpv = vscreen[first];
    vpp = pscreen[first];

    if (inserts) {          /* determine types of potential scrolls */
        end = endofline(vpv->v_text, cols);
        if (end == 0)
            target = first;         /* newlines */
        else if (same_grapheme_array(vpp->v_text, vpv->v_text, end))
            target = first + 1;     /* broken line newlines */
        else
            target = first;
    }
    else target = first + 1;

/* Find the matching shifted area */
    match = -1;
    longmatch = -1;
    longcount = 0;
    from = target;
    for (i = from + 1; i < rows - longcount /* P.K. */ ; i++) {
        if (inserts ? texttest(i, from) : texttest(from, i)) {
            match = i;
            count = 1;
            for (j = match + 1, k = from + 1;
                  j < rows && k < rows; j++, k++) {
                if (inserts? texttest(j, k): texttest(k, j)) count++;
                else break;
            }
            if (longcount < count) {
                longcount = count;
                longmatch = match;
            }
        }
    }
    match = longmatch;
    count = longcount;

    if (!inserts) {         /* Full kill case? */
        if (match > 0 && texttest(first, match - 1)) {
            target--;
            match--;
            count++;
        }
    }

/* Do the scroll */
    if (match > 0 && count > 2) {   /* got a scroll */
/* Move the count lines starting at target to match */
        if (inserts) {
            from = target;
            to = match;
        }
        else {
            from = match;
            to = target;
        }
        if (2 * count < abs(from - to)) return FALSE;
        scrscroll(from, to, count);
        for (i = 0; i < count; i++) {
            vpp = pscreen[to + i];
            vpv = vscreen[to + i];
            memcpy(vpp->v_text, vpv->v_text,
                 sizeof(struct grapheme)*(unsigned)cols);
            vpp->v_flag = vpv->v_flag;  /* XXX */
            if (vpp->v_flag & VFHIL) {
                vpp->v_flag &= ~(VFHIL | VFHIQ);
            }
        }
        if (inserts) {
            from = target;
            to = match;
        }
        else {
            from = target + count;
            to = match + count;
        }
        for (i = from; i < to; i++) {
            struct grapheme *txt;
            txt = pscreen[i]->v_text;
/* This is a pscreen, no need to worry about freeing any ex field */
            for (j = 0; j < term.t_ncol; ++j) txt[j] = blank_gph;
            vscreen[i]->v_flag |= VFCHG;
        }
        return TRUE;
    }
    return FALSE;
}

/* Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables. It does try to exploit erase to end of line.
 *
 * updateline()
 *
 * int row;             row of screen to update
 * struct video *vp1;   virtual screen image
 * struct video *vp2;   physical screen image
 */
static void updateline(int row, struct video *vp1, struct video *vp2) {

    struct grapheme *cp1;
    struct grapheme *cp2;
    struct grapheme *cp3;
    struct grapheme *cp4;
    struct grapheme *cp5;
    int nbflag;             /* non-blanks to the right flag? */
    int hiq;                /* highlight request flag */

/* Set up pointers to virtual and physical lines */
    cp1 = &vp1->v_text[0];
    cp2 = &vp2->v_text[0];

    hiq = (vp1->v_flag & VFHIQ) == VFHIQ;

/* If we need to change the reverse video status of the
 * current line, we need to re-write the entire line.
 */
    int hil;                /* highlight video flag */
    hil = (vp1->v_flag & VFHIL) == VFHIL;
    if (hil != hiq) {
        movecursor(row, 0);     /* Go to start of line. */
        TThilite(hiq);

/* Scan through the line and dump it to the screen and
 * the virtual screen array
 */
        cp3 = &vp1->v_text[term.t_ncol];
        while (cp1 < cp3) {
            TTputgrapheme(cp1);
            clone_grapheme(cp2++, cp1++);
        }
        if (hil != hiq)     /* turn highlight off */
            TThilite(FALSE);

/* Update the needed flags */
        vp1->v_flag &= ~VFCHG;
        if (hiq) vp1->v_flag |= VFHIL;
        else     vp1->v_flag &= ~VFHIL;
        return;
    }

/* Advance past any common chars at the left */
    while (cp1 != &vp1->v_text[term.t_ncol] && same_grapheme(cp1, cp2, 0)) {
        ++cp1;
        ++cp2;
    }

/* This can still happen, even though we only call this routine on changed
 * lines. A hard update is always done when a line splits, a massive
 * change is done, or a buffer is displayed twice. This optimizes out most
 * of the excess updating. A lot of computes are used, but these tend to
 * be hard operations that do a lot of update, so I don't really care.
 */
/* If both lines are the same, no update needs to be done */
    if (cp1 == &vp1->v_text[term.t_ncol]) {
        vp1->v_flag &= ~VFCHG;      /* Flag this line is not changed */
        return;
    }

/* Find out if there is a match on the right */
    nbflag = FALSE;
    cp3 = &vp1->v_text[term.t_ncol];
    cp4 = &vp2->v_text[term.t_ncol];

    while (same_grapheme(&(cp3[-1]), &(cp4[-1]), 0)) {
        --cp3;
        --cp4;
        if (!is_space(&(cp3[0])))   /* Note if any nonblank */
            nbflag = TRUE;          /* in right match. */
    }

    cp5 = cp3;

/* Erase to EOL ? */
    if (!nbflag && !hiq) {
        while (cp5 != cp1 && is_space(&(cp5[-1]))) --cp5;

        if (cp3 - cp5 <= 3)         /* Use only if erase is */
            cp5 = cp3;              /* fewer characters. */
    }

    movecursor(row, (int)(cp1 - &vp1->v_text[0]));  /* Go to start of line. */
    TThilite(hil);

    while (cp1 != cp5) {    /* Ordinary. */
        TTputgrapheme(cp1);
        clone_grapheme(cp2++, cp1++);
    }

    if (cp5 != cp3) {       /* Erase. */
        TTeeol();
        while (cp1 != cp3)
            clone_grapheme(cp2++, cp1++);
    }
    TThilite(FALSE);
    vp1->v_flag &= ~VFCHG;  /* Flag this line as updated */
    return;
}

/* updupd:
 *      update the physical screen from the virtual screen
 *
 * int force;           forced update flag
 */
static int updupd(void) {

    struct video *vp1;
    int i;
    if (scrflags & WFKILLS)
        scrolls(FALSE);
    if (scrflags & WFINS)
        scrolls(TRUE);
    scrflags = 0;

/* GGR - include the last row, so <=, (for mini-buffer) */
    int lrow = inmb? term.t_mbline: term.t_vscreen;
    for (i = 0; i <= lrow; ++i) {
        vp1 = vscreen[i];

/* For each line that needs to be updated */
        if ((vp1->v_flag & VFCHG) != 0) updateline(i, vp1, pscreen[i]);
    }
    return TRUE;
}

/* updext:
 *      update the extended line which the cursor is currently
 *      on at a column greater than the terminal width. The line
 *      will be scrolled right or left to let the user see where
 *      the cursor is
 */
static void updext(void) {
    int rcursor;                /* real cursor location */

/* Calculate what column the real cursor will end up in */
    rcursor = (curcol % term.t_scrsiz) + term.t_margin;
    lbound = curcol - rcursor + 1;
    taboff = lbound + curwp->w.fcol;

/* Scan through the line outputting characters to the virtual screen
 * once we reach the left edge.
 */
    vtmove(currow, -taboff);    /* start scanning offscreen */
    show_line(curwp->w.dotp);   /* Show the line */

/* Truncate the virtual line, restore tab offset */
    vteeol();
    taboff = 0;

/* And put a '$' in column 1. but if this is a multi-width character we also
 * need to change any following NULs to spaces
 */
    int cw = utf8char_width(vscreen[currow]->v_text[0].uc);
    update_grapheme(&(vscreen[currow]->v_text[0]), ovflw);
    for (int pcol = cw - 1; pcol > 0; pcol--) {
        update_grapheme(&(vscreen[currow]->v_text[pcol]), ' ');
    }
}

/* updpos() and upddex() need to agree on when a line will be scrolled
 * right, so define a macro for it.
 */
#define DO_SCROLL (curcol > (term.t_scrsiz + term.t_margin))

static int cline_display_overlong(void) {
    int dcol = 0;       /* Display column */
    int offs = 0;       /* Offset into line buffer */
/* cp is only used if we have text on the line, so ltext() is OK. */
    const char *cp = ltext(curwp->w.dotp);
    int max_offs = lused(curwp->w.dotp);
    while (offs < max_offs) {
        char cc = *(cp+offs);
        if (cc <= 0xa0) {
/* This is a cut-down version of update_screenpos_for_char() from line.h
 * (as we just have a char, not a unicode char).
 */
            if (cc == '\t') { dcol |= tabmask; dcol++; }    /* Round up */  \
            else if (cc < 0x20 || cc == 0x7f)               /* ^X */        \
               dcol += (nodisplay == -1)? 2: 1;             /* or U+24xx */ \
            else if (cc >= 0x80 && cc <= 0xa0) dcol += 3;   /* \nn */       \
            else dcol++;
            offs++;
        }
        else {
            struct grapheme gc;
            offs = build_next_grapheme((char *)cp, offs, max_offs, &gc, TRUE);
            dcol += utf8char_width(gc.uc);
        }
        if (dcol > term.t_ncol) return TRUE;
    }
    return FALSE;
}

/* updpos:
 *      update the position of the hardware cursor and handle extended
 *      lines. This is the only update for simple moves.
 */
static void updpos(void) {
    struct line *lp;
    int i;

/* Find the current row */
    lp = curwp->w_linep;
    currow = curwp->w_toprow;
    while (lp != curwp->w.dotp) {
        ++currow;
        lp = lforw(lp);
    }

/* Find the current column */
    curcol = 0;
    i = 0;
    while (i < curwp->w.doto) {
        unicode_t c;
/* Only reached if we have text on the line, so ltext() is OK. */
        int bytes = utf8_to_unicode(ltext(lp), i, curwp->w.doto, &c);
        i += bytes;
        update_screenpos_for_char(curcol, c);
    }

/* Adjust by the current first column position */

    curcol -= curwp->w.fcol;

/* Make sure it is not off the left side of the screen */
    while (curcol < 0) {
        if (curwp->w.fcol >= hjump) {
            curcol += hjump;
            curwp->w.fcol -= hjump;
        } else {
            curcol += curwp->w.fcol;
            curwp->w.fcol = 0;
        }
        curwp->w_flag |= WFHARD | WFMODE;
    }

/* If horizontal scrolling is enabled, shift if needed */
    if (hscroll) {
        while (curcol >= term.t_ncol - 1) {
            curcol -= hjump;
            curwp->w.fcol += hjump;
            curwp->w_flag |= WFHARD | WFMODE;
        }
    } else {
/* If extended, flag so and update the virtual line image.
 * The old lused(curwp->w.dotp) test does not work if the line contains tabs
 * We need to check whether the *total column count* is > t_ncol
 */
        if (DO_SCROLL && cline_display_overlong()) {
            vscreen[currow]->v_flag |= (VFEXT | VFCHG);
            updext();
        } else
            lbound = 0;
    }

/* If we've now set the w_flag we need to recall update(), which is
 * now re-entrant.
 */
    if (curwp->w_flag) update(FALSE);
}

/* upddex:
 *      de-extend any line that deserves it
 */
static void upddex(void) {
    struct window *wp;
    struct line *lp;
    int i;

    wp = wheadp;

    while (wp != NULL) {
        lp = wp->w_linep;
        i = wp->w_toprow;

/* GGR - FIX1 (version 2)
 * Add check for reaching end-of-buffer (== loop back to start) too
 * otherwise we process lines at start of file as if they are
 * beyond the end of it. (the "lp != " part).
 */
        while ((i < wp->w_toprow + wp->w_ntrows) &&
             (lp != wp->w_bufp->b_linep)) {
            if (vscreen[i]->v_flag & VFEXT) {
                if ((wp != curwp) || (lp != wp->w.dotp) || !DO_SCROLL) {
                    taboff = wp->w.fcol;
                    vtmove(i, -taboff);
                    show_line(lp);
                    vteeol();
                    taboff = 0;

/* This line no longer is extended */
                    vscreen[i]->v_flag &= ~VFEXT;
                    vscreen[i]->v_flag |= VFCHG;
                }
            }
            lp = lforw(lp);
            ++i;
        }
/* And onward to the next window */
        wp = wp->w_wndp;
    }
}

/* Get a display name for a buffer.
 * Remember that with a split window there is more than one buffer on
 * display, so we need to know which we are looking at.
 */

static db_bufdef(last_display);
static db_bufdef(last_bname);
static int last_width = -1;
static struct buffer *last_bp = NULL;

static const char *get_buffer_display_name(struct buffer *tbp, int w_want) {

    if (w_want < 3) w_want = 3;     /* Set a minimum (1 fr, 1 ell, 1 bk) */

/* Handle the most common case quickly.
 * Note that for last_width to match, last_bname and last_display
 * must have been set to non-NULL.
 */

    if ((tbp == last_bp) && (w_want == last_width) &&
        (db_casecmp(last_bname, tbp->b_bname) == 0)) {
        return db_val(last_display);
    }

/* Have to work out a new one... */

    int bn_glyph = glyphcount_utf8(tbp->b_bname);

/* If we have enough space for the whole name, this is easy.  */

    if (bn_glyph <= w_want) {
        db_set(last_display, tbp->b_bname);
    }
    else {

/* Trickier. Need to abbreviate the result. */

        int ncut = bn_glyph - w_want + 1;   /* Glyphs to cut (+ ellipsis) */
        int scut = 2*(w_want - 1)/3;        /* Where to start cut */
        db_clear(last_display);

/* Add up until scut */
        const char *cp = tbp->b_bname;
        int offs = 0;
        int max = istrlen(tbp->b_bname);
        while (scut--) {
            offs = next_utf8_offset(cp, offs, max, TRUE);
        }
        db_appendn(last_display, cp, offs);

/* Skip ncut - replace with MHE  */

        while (ncut--) {
            offs = next_utf8_offset(cp, offs, max, TRUE);
        }
        static char MHE[3] = { (char)0xe2, (char)0x8b, (char)0xaf };
        db_appendn(last_display, MHE, 3);

/* Add the rest */

        db_append(last_display, cp+offs);
    }
    last_width = w_want;
    last_bp = tbp;
    db_set(last_bname, tbp->b_bname);

    return db_val(last_display);
}

/* Redisplay the mode line for the window pointed to by the "wp". This is the
 * only routine that has any idea of how the modeline is formatted. You can
 * change the modeline format by hacking at this routine. Called by "update"
 * any time there is a dirty window.
 * The minibuffer modeline is different, but still handled here.
 */
static void modeline(struct window *wp) {
    const char *cp;
    int c;
    struct buffer *bp;
    int i;                  /* loop index */
    int lchar;              /* character to draw line in buffer with */
    int firstm;             /* is this the first mode? */

/* Determine where the modeline actually is...*/
    int n;
    if (inmb) n = mb_info.main_wp->w_toprow + mb_info.main_wp->w_ntrows;
    else      n = wp->w_toprow + wp->w_ntrows;  /* Normal location. */
    vscreen[n]->v_flag |= VFCHG | VFHIQ;        /* Redraw next time. */

    vtmove(n, 0);           /* Seek to right line. */
    if (wp == curwp)        /* mark the current buffer */
        lchar = '=';
    else {
        if (db_len(hibcolor) > 0) lchar = ' ';  /* We have a background */
        else {  /* No background - make sure we have some full-width marker */
            if (db_len(hifcolor) > 0) lchar = '_';
            else lchar = ' ';   /* The reverse video is a marker */
        }
    }

/* For the minibuffer, wp->w_bufp is the minibuffer.
 * No point in showing its changed state, etc., but there is
 * a use in reporting when it is multi-line.
 */
    if (inmb) bp = mb_info.main_bp;
    else      bp = wp->w_bufp;

/* Display mini-buffer bits at the start.
 * These *do* need to use wp->w_bufp.
 */
    if (inmb) {
        char mbprompt[20];
        vtputc(lchar);
        vtputc(lchar);
        sprintf(mbprompt, " miniBf%d", mb_info.mbdepth);
        cp = mbprompt;
        while ((c = *cp++) != 0) vtputc(c);
/* If next, next from the buffer topmarker doesn't take us back there
 * then we have multiple lines in the minibuffer.  Note this...
 */
        struct buffer *mbp = wp->w_bufp;    /* Need this one here */
        if (lforw(lforw(mbp->b_linep)) != mbp->b_linep) {
            cp = " (multiline!)";
            while ((c = *cp++) != 0) vtputc(c);
        }

/* Display modes for the mini-buffer as single chars within {}
 * The modes-as-words later within [] are left as those in the main buffer.
 */
        vtputc('{');
        int using_phon = 0;
        int mode_mask = 1;
        for (int i = 0; mode_mask != MDNOMOR; i++) {    /* Add in mode flags */
            if (mbp->b_mode & mode_mask) {
                switch(mode_mask) {
                case MDEQUIV:           /* Never displayed alone */
                case MDRPTMG:
                    break;
                case MDPHON:
                    using_phon = 1;
                    break;
                case MDMAGIC:
                    vtputc('M');
/* Append "r" if reporting mode is on and "q" if Equiv. */
                    if (mbp->b_mode & MDRPTMG) vtputc('r');
                    if (mbp->b_mode & MDEQUIV) vtputc('q');
                    break;
                default:
                    vtputc(modecode[i]);
                }
            }
            mode_mask <<= 1;
        }
        if (using_phon) show_utf8(ptt->ptt_headp->display_code);
        vtputc('}');
        vtputc('>');
        vtputc('>');
        vtputc(' ');
    }
    else {      /* A "normal" buffer */
        if ((bp->b_flag & BFTRUNC) != 0)    vtputc('#');
        else                                vtputc(lchar);

        if ((bp->b_flag & BFCHG) != 0)      vtputc('*');
        else                                vtputc(lchar);

        if ((bp->b_flag & BFNAROW) != 0)    vtputc('<');
        else                                vtputc(lchar);

        db_set(glb_db, " " PROGRAM_NAME_LONG);

/* GGR - only if no user-given filename (space issue) */
        if (*(bp->b_dfname) == 0) db_append(glb_db, " " VERSION);
        db_append(glb_db, ": ");
        cp = db_val(glb_db);
        while ((c = *cp++) != 0) vtputc(c);
    }

/* The buffer name is the next item to display, but may now be shortened
 * according to the the screen width (and hence the modeline length).
 * So remember where we are now, build up the rh_side text then
 * get a buffer name display text based on what space is left.
 */
    int lh_width = vtcol;

/* Start building the R/h side (into glb_db) */

    db_set(glb_db, " " MLpre);

/* Are we horizontally scrolled? */
    if (wp->w.fcol > 0) {
        db_append(glb_db, ue_itoa(wp->w.fcol));
        db_append(glb_db, "> ");
    }

/* Display the modes */

    firstm = TRUE;
    if ((bp->b_flag & BFTRUNC) != 0) {
        firstm = FALSE;
        db_append(glb_db, "Truncated");
    }
    struct window *mwp;
    if (inmb) mwp = mb_info.main_wp;
    else      mwp = wp;
    int mode_mask = 1;
    for (i = 0; mode_mask != MDNOMOR; i++) {    /* Add in mode flags */
/* MDEQUIV and MDRPTMG are never displayed alone */
        if (mode_mask & MD_EQVRPT) goto next_mode;
        if (mwp->w_bufp->b_mode & mode_mask) {
            if (!firstm) db_append(glb_db, " ");
            firstm = FALSE;
            switch(mode_mask) {
            case MDPHON:
                db_append(glb_db, ptt->ptt_headp->display_code);
                break;
            case MDMAGIC:
/* How we display Magic depends on whether Equiv mode is on. */
                if ((mwp->w_bufp->b_mode & MD_EQVRPT) == MD_EQVRPT) {
                    db_append(glb_db, "RMgEqv");
                    break;
                }
                if (mwp->w_bufp->b_mode & MDRPTMG) {
                    db_append(glb_db, "RMagic");
                    break;
                }
                if (mwp->w_bufp->b_mode & MDEQUIV) {
                    db_append(glb_db, "MgEqv");
                    break;
                }   /* Fall through */
            default:
                db_append(glb_db, mode2name[i]);
            }
        }
next_mode:
        mode_mask <<= 1;
    }
    db_append(glb_db, MLpost " ");

/* Add in the filename if set and it is different to the buffername.
 * (after allowing for the leading "./" we now use).
 * This can contain utf8...
 * For the minibuffer this will be the main buffer name .
 */
    if ( (*bp->b_dfname != 0) &&
         ( (*(bp->b_dfname) != '.') ||
           (*(bp->b_dfname+1) != '/') ||
           (strcmp(bp->b_bname, bp->b_dfname+2) != 0))) {
        db_append(glb_db, bp->b_dfname);
        db_addch(glb_db, ' ');
    }

/* Now get the buffer name display text for this buffer, display it
 * and (most of) the rh_side.
 */
    int w_avail = term.t_ncol - lh_width - db_len(glb_db) - 7;
    show_utf8(get_buffer_display_name(bp, w_avail));
    show_utf8(db_val(glb_db));

/* Pad to full width. */
    while (vtcol < term.t_ncol) vtputc(lchar);

/* Determine whether top line, bottom line, or both are visible */

    struct line *lp = wp->w_linep;
    int rows = wp->w_ntrows;
    const char *msg = NULL;

    vtcol -= 7;  /* strlen(" top ") plus a couple */
    while (rows--) {
        lp = lforw(lp);
        if (lp == wp->w_bufp->b_linep) {
            msg = " Bot ";
            break;
        }
    }
    if (lback(wp->w_linep) == wp->w_bufp->b_linep) {
        if (msg) {
            if (wp->w_linep == wp->w_bufp->b_linep)
                msg = " Emp ";
            else
                msg = " All ";
            } else {
                msg = " Top ";
            }
    }
    if (!msg) {
        struct line *lp;
        int numlines, predlines;

        lp = lforw(bp->b_linep);
        numlines = 0;
        predlines = 0;
        while (lp != bp->b_linep) {
            if (lp == wp->w_linep) {
                predlines = numlines;
            }
            ++numlines;
            lp = lforw(lp);
        }
        if (wp->w.dotp == bp->b_linep) {
            msg = " Bot ";
        } else {
            int ratio = 0;
/* Use long long to avoid overfflow */
            if (numlines != 0) ratio = (int)((100LL*predlines)/numlines);
            if (ratio > 99)    ratio = 99;
            db_sprintf(glb_db, " %2d%% ", ratio);
            msg = db_val(glb_db);
        }
    }
    show_utf8(msg);
}

/* If cbp is non-NULL only set the flag for windows containing
 * that buffer.
 */
void upmode(struct buffer *cbp) {   /* Update mode lines */
    for (struct window *wp = wheadp; wp; wp = wp->w_wndp) {
        if (!cbp || (wp->w_bufp == cbp)) wp->w_flag |= WFMODE;
    }
}

/* checkscreensize checks the current screen size and if it has changed
 * from the previous setting (term/t_ncol/term.tnrow) it resets/recalculates
 * things and then runs a screen update (optionally).
 */
void mlwrite_one(const char *); /* Forward declaration */
void checkscreensize(int no_update_needed) {
    struct winsize size;
    int width, height;

/* If we can't get the size, we are stuffed (stdout redirected?).
 * Unless the user has specified we set "dummy size" for testing.
 */
    if (pretend_size) {
        width = 80;
        height = 24;
    }
    else {
/* If this call fails, just exit */
        if (ioctl(1, TIOCGWINSZ, &size) < 0) {
            if (prev_mrow == 0) exit(errno);    /* vtinit() not yet run */
            else quickexit(ABORT, errno);       /* vtinit() has run */
        }

/* Claude/Fable reckons "transient" zeroes can arrive here, so
 * if we got 0, but had previously got an answer, use that and hope.
 */
        if (size.ws_col == 0) width = term.t_ncol;
        else                  width = size.ws_col - fake_narrow;
        if (size.ws_row == 0) height = term.t_nrow;
        else                  height = size.ws_row;

/* If we still have a 0 we cannot continue. */

        if ((width == 0) || (height == 0)) {
            if (prev_mrow == 0) exit(ENXIO);
            else quickexit(ABORT, ENXIO);
        }
    }

/* Has the size actually changed? If not, we are done */

    if ((width == term.t_ncol) && (height == term.t_nrow)) return;

/* Make sure it's reasonable.
 * If it isn't, we don't change anything. The display will mess up, but
 * we won't have problems with crashes form areas being too small/-ve.
 */
    if (height < 3 ) {
        mlwrite_one("Screen size too small");
        return;
    }
    if (width < 10) {
        mlwrite_one("TOO SMALL");
        return;
    }

/* We have work to do */

    if (displaying) {           /* do the change later */
        delayed_update = TRUE;
        return;
    }
    delayed_update = FALSE;

/* Given a screen height and width, set t_mcol/t_mrow as a rounded-up
 * amount, with a minimum size (to avoid re-allocs on small changes).
 */
#define MINCOL 240
#define MINROW  70
    term.t_mcol = 50*(1 + (width + 30)/50);
    if (term.t_mcol < MINCOL) term.t_mcol = MINCOL;
    term.t_mrow = 30*(1 + (height + 20)/30);
    if (term.t_mrow < MINROW) term.t_mrow = MINROW;

    vtinit();

/* Things to do if we are changing the height */

    int to_add = height - term.t_nrow;
    if (to_add != 0) {  /* We have a change in height */

        if (wheadp) {       /* Only if windows exist (so not at TTinit) */
/* NOTE that the methods take different args */
            if (ggr_opts&GGR_NEWHEIGHT) {
                new_sizer(to_add);
            }
            else {
                old_sizer(height);
            }
        }
/* Set term.t_nrow and all related vars now */
        SET_t_nrow(height);
    }


/* Things to do if we are changing the width */

    if (width != term.t_ncol) {

/* Otherwise, just re-width it (no big deal).
 * t_margin is just a hueristic. Nothing special...
 */
        term.t_ncol = width;
        term.t_margin = 2 + width/40;
        term.t_scrsiz = width - (2*term.t_margin);

/* If the //List buffer is being shown, recalculate it for the new width */

        int update_blistp = 0;
        if (blistp && (blistp->b_nwnd > 0)) {
            makelist(-1); /* -1 == use last iflag */
            blistp->b_flag |= BFCHG;
            update_blistp = 1;
        }

/* Force all windows to redraw. Update blistp when we hit it */
        struct window *wp = wheadp;
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
    }

/* screen is garbage */
    sgarbf = TRUE;
    if (!no_update_needed) update(TRUE);
    return;
}

void mlrewrite(void) {
    if (ml_text_offset == 0) return;    /* Do NOTHING if there is no text */
    int scol = curcol;
    int srow = currow;
    ml_rewriting = 1;
    mlerase();
    for (int i = 0; i < ml_text_offset; i++) mlout_uc(ml_text[i]);
    ml_rewriting = 0;
    if (!ml_prompt) movecursor(srow, scol - lbound);    /* Send cursor back */
    TTflush();
}

/* Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 *
 * Do nothing if we are not displaying (discmd == FALSE)
 *
 * int force;           force update past type ahead?
 */
void update(int force) {
    struct window *wp;

    if (!discmd) return;

    if (!vismac && (force == FALSE) && (kbdmode == PLAY)) return;

/* GGR Set-up any requested new screen size before working out a screen
 * update, rather than waiting until the end.
 * spawn.c forces a redraw using this on return from a command line, and
 * we need to ensure that term.t_ncol is set before doing any vtputc() calls.
 */
    if (delayed_update) checkscreensize(1);
    int was_displaying = displaying;    /* So this can recurse.... */
    displaying = TRUE;

/* First, propagate mode line changes to all instances of a buffer
 * displayed in more than one window
 */
    for (wp = wheadp; wp; wp = wp->w_wndp) {
        if ((wp->w_flag & WFMODE) && (wp->w_bufp->b_nwnd > 1)) {
/* Make sure all previous windows have this */
            for (struct window *owp = wheadp; owp; owp = owp->w_wndp) {
                if (owp->w_bufp == wp->w_bufp) owp->w_flag |= WFMODE;
            }
        }
    }

/* Update any windows that need refreshing
 * GGR - get the correct window
 */
    if (mbonly) wp = curwp;
    else        wp = wheadp;
    while (wp != NULL) {
        if (wp->w_flag) {
/* If the window has changed, service it */
            reframe(wp);        /* Check the framing */
            if (wp->w_flag & (WFKILLS | WFINS)) {
                scrflags |= (wp->w_flag & (WFINS | WFKILLS));
                wp->w_flag &= ~(WFKILLS | WFINS);
            }
            if ((wp->w_flag & ~WFMODE) == WFEDIT)
                updone(wp);     /* update EDITed line */
            else if (wp->w_flag & ~WFMOVE)
                updall(wp);     /* update all lines */
            if (scrflags || (wp->w_flag & WFMODE))
                modeline(wp);   /* update modeline */
            wp->w_flag = 0;
            wp->w_force = 0;
        }
/* On to the next window.   GGR - stop if in minibuffer */
        if (mbonly) wp = NULL;
        else        wp = wp->w_wndp;
    }
/* Recalc the current hardware cursor location */

    updpos();

/* Check for lines to de-extend */
    upddex();

/* If screen is garbage, re-plot it */

    int need_rewrite = 0;
    if (sgarbf != FALSE) {
        need_rewrite = (ml_text_offset > 0);
        updgar();
    }

/* Update the virtual screen to the physical screen */

    updupd();
    if (need_rewrite) mlrewrite();

/* Update the cursor and flush the buffers */
    movecursor(currow, curcol - lbound);

    TTflush();
    displaying = was_displaying;

    if (delayed_update) checkscreensize(0);
    return;
}

/* Write a message into the message line. Keep track of the physical cursor
 * position.
 * A small class of printf like format items is handled by mlwrite() and
 * mlforce() - mlwrite_one() and mlforce_one() just take a striing.
 * The handling is now hived off to mlwrite_ap for all four functions.
 *
 * GGR modified to handle utf8 strings.
 *
 * char *fmt;           format string for output
 * va_list ap;          variable arg list, or NULL for no interpolation
 */

/* Some gccs are happy to test va_list against NULL.
 * Others are not. Even with similar gcc version and the same stdarg.h
 * So, define a union - which seems to keep everyone happy.
 */
typedef union {
    char *p;
    va_list ap;
} npva;

/* Write out a 8-byte integer, in the specified radix (8, 10, 16).
 * We'll be running with ue64I_t as 8-bytes, and need a "ll" formatter.
 * Update the physical cursor position.
 */
static void mlputli(ue64I_t l, int r) {
    char tbuf[32];
    const char *fmt;

    switch(r) {
    case  8: fmt = "%llo"; break;
    case 16: fmt = "%llX"; break;
    default: fmt = "%lld"; break;
    }
    sprintf(tbuf, fmt, l);
    char *op = tbuf;
    while (*op) mlout_uc(*op++);
    return;
}

/* Do the same except with an integer.
 * So we just pass it on to its longer brother.
 */
#define mlputi(i, r) mlputli((ue64I_t) i, r)

/* write out a scaled integer with two decimal places
 *
 * int s;               scaled integer to output
 */
static void mlputf(int s) {
    int i;                  /* integer portion of number */
    int f;                  /* fractional portion of number */

/* Break it up */
    i = s / 100;
    f = s % 100;

/* Send out the integer portion */
    mlputi(i, 10);
    mlout_uc('.');
    mlout_uc((f / 10) + '0');
    mlout_uc((f % 10) + '0');
}

/* NOTE: that the argument templates here are NOT printf ones.
 * There are no output width/precision options.
 * Allowed templates are:
 *  d   integer (decimal)
 *  o   integer (octal)
 *  x   integer (hex)
 *  D   integer (8-byte integer - long long)
 *  c   character
 *  s   string
 *  f   scaled integer (uemacs - real number to 2 dec places * 100)
 */
static int mlw_level = 0;

static void mlwrite_ap(const char *fmt, npva ap) {
    unicode_t c;                /* current char in format string */

/* If we are not currently echoing on the command line, abort this */
    if (discmd == FALSE) return;

    mlw_level++;            /* Remember we are here */

/* Erase to end-of-line, quickly if we can. But only if we aren't
 * recursing...
 * If we're crashing out (saving files...) when the original terminal
 * window was not at the bottom line, this may leave a lot of blank lines
 * but we don't know where we were on the screen, so just let it happen.
 * Trying to remove this may (will?) just introduce the possibility of
 * something worse.
 */
    int scol=0, srow=0;
    if (mlw_level == 1) {
        scol = curcol;
        srow = currow;
        mlerase();  /* Leaves us at col0 of messageline */
        ml_text_offset = 0; /* In case mlerase() found handling_sigwinch set */
    }

/* GGR - loop through the bytes getting any utf8 sequence as unicode */
    int bytes_togo = istrlen(fmt);
    while (bytes_togo > 0) {
/* Limit output to not go beyond the last column by using mlout_uc()
 * otherwise we get wrap-around and the display messes up.
 */
        int used = utf8_to_unicode(fmt, 0, bytes_togo, &c);
        bytes_togo -= used;
        fmt += used;
        if ((ap.p == NULL) || (c != '%')) {
            mlout_uc(c);
        } else {
            if (bytes_togo <= 0) continue;
            int used = utf8_to_unicode(fmt, 0, bytes_togo, &c);
            bytes_togo -= used;
            fmt += used;

            switch (c) {
            case 'd':   mlputi(va_arg(ap.ap, int), 10);     break;
            case 'o':   mlputi(va_arg(ap.ap, int), 8);      break;
            case 'x':   mlputi(va_arg(ap.ap, int), 16);     break;
            case 'D':   mlputli(va_arg(ap.ap, ue64I_t), 10); break;
            case 'f':   mlputf(va_arg(ap.ap, int));         break;
            case 'c':   mlout_uc(va_arg(ap.ap, int));  break;
            case 's': {
                const char *tp = va_arg(ap.ap, char *);
                if (tp == NULL) tp = "(nil)";
                mlwrite_one(tp);        /* Recurse */
                break;
            }
            case 'B': { /* A dyn_buf */
                db *tp = va_arg(ap.ap, db *);
                int blen = dbp_len(tp);
                const char *bp = dbp_val(tp);
                int offs = 0;
                unicode_t uc;
                while (offs < blen) {
                    int used = utf8_to_unicode(bp, offs, blen, &uc);
                    offs += used;
                    mlout_uc(uc);
                }
                break;
            }
            default:
                mlout_uc(c);
            }
        }
    }
    mlw_level--;    /* Remember we've left */
    if (mlw_level == 0) {
/* Send the cursor back UNLESS it was originally in the message line */
        if (!ml_prompt && (srow != term.t_mbline))
             movecursor(srow, scol - lbound);
        TTflush();
    }
}

void mlwrite(const char *fmt, ...) {
    ml_prompt = 0;
    npva ap;
    va_start(ap.ap, fmt);
    mlwrite_ap(fmt, ap);
    va_end(ap.ap);
    return;
}

/* This one leaves the cursor in the message line */
void mlprompt(const char *fmt, ...) {
    ml_prompt = 1;
    npva ap;
    va_start(ap.ap, fmt);
    mlwrite_ap(fmt, ap);
    va_end(ap.ap);
    return;
}

/* Force a string out to the message line regardless of the
 * current $discmd setting. This is needed when $debug is TRUE
 * and for the write-message and clear-message-line commands
 * Now takes a parameter to indicate an optional pause after display.
 * So each call is responsible for its own wait.
 */
void mlforce(int wait, const char *fmt, ...) {
    ml_prompt = 0;
    int oldcmd;     /* original command display flag */

    npva ap;
    va_start(ap.ap, fmt);

    oldcmd = discmd;        /* save the discmd value */
    discmd = TRUE;          /* and turn display on */
    mlwrite_ap(fmt, ap);    /* write the string out */
    va_end(ap.ap);
    discmd = oldcmd;        /* and restore the original setting */
/* Was there a wait request? */
    if (wait < 0) {    /* Wait based on output length, so work it out */
        int wu = (ml_text_offset / 42) + 1;
        wait *= -wu;
    }
    if (wait > 0) sleep((unsigned)wait);
    return;
}

/* Versions of mlwrite/mlforce that are printing a fixed string
 * and want no accidental interpolation of % chars.
 */
static npva nullva = { NULL };  /* Initialized to type of first member */

void mlwrite_one(const char *fmt) {
    ml_prompt = 0;
    mlwrite_ap(fmt, nullva);
    return;
}
/* This one leaves the cursor in the message line */
void mlprompt_one(const char *fmt) {
    ml_prompt = 1;
    mlwrite_ap(fmt, nullva);
    return;
}

void mlforce_one(int wait, const char *fmt) {
    ml_prompt = 0;
    int oldcmd;             /* original command display flag */
    oldcmd = discmd;        /* save the discmd value */
    discmd = TRUE;          /* and turn display on */
    mlwrite_ap(fmt, nullva);    /* write the string out */
    discmd = oldcmd;        /* and restore the original setting */
/* Was there a wait request? */
    if (wait < 0) {    /* Wait based on output length, so work it out */
        int wu = (ml_text_offset / 42) + 1;
        wait *= -wu;
    }
    if (wait > 0) sleep((unsigned)wait);
    return;
}

/* GGR
 *    function to erase the mapped minibuffer line
 *    (so different from mlerase()
 */
void mberase(void) {
    struct video *vp1;

    vtmove(term.t_mbline, 0);
    vteeol();   /* Replace the rest (== all) of the line with ' ' */
    vp1 = vscreen[term.t_mbline];
    updateline(term.t_mbline, vp1, pscreen[term.t_mbline]);
    return;
}

/* GGR
 *    function to update *just* the minibuffer window
 */
void mbupdate(void) {
    mbonly = TRUE;
    update(TRUE);
    mbonly = FALSE;
    return;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_display(void) {
/* Free any ex graphemes from the vscreen data */
    for (int ri = 0; ri < term.t_mrow; ri++) {
        struct video *vp = vscreen[ri];
        for (int ci = 0; ci < term.t_mcol; ci++) {
            Xfree(vp->v_text[ci].ex);
        }
    }
    Xfree(vscreen);
    Xfree(pscreen);
    Xfree(vdata);
    Xfree(ml_text);

    db_free(last_bname);
    db_free(last_display);
    return;
}
#endif
