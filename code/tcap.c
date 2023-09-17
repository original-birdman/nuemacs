/*      tcap.c
 *
 *      Unix SysV and BS4 Termcap video driver
 *
 *      modified by Petri Kutvonen
 */

/*
 * Defining this to 1 breaks tcapopen() - it doesn't check if the
 * screen size has changed.
 *      -lbt
 */
#define USE_BROKEN_OPTIMIZATION 0
#define termdef 1 /* Don't define "term" external. */

#include <stdio.h>
#include <curses.h>
#include <term.h>
#include <signal.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

#define MARGIN  8
#define SCRSIZ  64
#define NPAUSE  10    /* # times thru update to pause. */
#define BEL     0x07
#define ESC     0x1B

static void tcapkopen(void);
static void tcapkclose(void);
static void tcapmove(int, int);
static void tcapeeol(void);
static void tcapeeop(void);
static void tcapbeep(void);
static void tcaprev(int);
static int tcapcres(char *);
static void tcapscrollregion(int top, int bot);

static void tcapopen(void);
static void tcapclose(void);

#if COLOR
static void tcapfcol(int);
static void tcapbcol(int);
#endif
static void tcapscroll_reg(int from, int to, int linestoscroll);
static void tcapscroll_delins(int from, int to, int linestoscroll);

#define TCAPSLEN 315
static char tcapbuf[TCAPSLEN];
static char *UP, PC, *CM, *CE, *CL, *SO, *SE;

static char *TI, *TE;
#if USE_BROKEN_OPTIMIZATION
static int term_init_ok = 0;
#endif

static char *_CS, *DL, *AL, *SF, *SR;

struct terminal term = {
/* Functions */
    tcapopen,
    tcapclose,
    tcapkopen,
    tcapkclose,
    ttgetc,
    ttputc,
    ttflush,
    tcapmove,
    tcapeeol,
    tcapeeop,
    tcapbeep,
    tcaprev,
    tcapcres,
#if COLOR
    tcapfcol,
    tcapbcol,
#endif
    NULL,              /* set dynamically at open time */
/* "Constants" (== variables that are set)
 * The first eight values are set dynamically at open/resize time.
 */
    0,
    0,
    0,
    0,
    0,
    0,
    MARGIN,
    SCRSIZ,
/* */
    NPAUSE,
};

static void tcapopen(void) {
    char *t, *p;
    char tcbuf[1024];
    char *tv_stype;
    char err_str[72];
    int int_col, int_row;

#if USE_BROKEN_OPTIMIZATION
    if (!term_init_ok) {
#endif
    if ((tv_stype = getenv("TERM")) == NULL) {
        puts("Environment variable TERM not defined!");
        exit(1);
    }

    if ((tgetent(tcbuf, tv_stype)) != 1) {
        sprintf(err_str, "Unknown terminal type %s!", tv_stype);
        puts(err_str);
        exit(1);
    }

/* Get screen size from system, or else from termcap.  */
    getscreensize(&int_col, &int_row);
    term.t_ncol = int_col;
    SET_t_nrow(int_row);

    if ((term.t_nrow <= 0) && (term.t_nrow = (short)tgetnum("li")) == -1) {
        puts("termcap entry incomplete (lines)");
        exit(1);
    }

    if ((term.t_ncol <= 0) && (term.t_ncol = (short)tgetnum("co")) == -1) {
        puts("Termcap entry incomplete (columns)");
        exit(1);
    }

    set_scrarray_size(term.t_nrow, term.t_ncol);

    p = tcapbuf;
    t = tgetstr("pc", &p);
    if (t) PC = *t;
    else   PC = 0;

    CL = tgetstr("cl", &p);
    CM = tgetstr("cm", &p);
    CE = tgetstr("ce", &p);
    UP = tgetstr("up", &p);
    SE = tgetstr("se", &p);
    SO = tgetstr("so", &p);
    revexist = (SO != NULL);
    if (tgetnum("sg") > 0) {    /* can reverse be used? P.K. */
        revexist = FALSE;
        SE = NULL;
        SO = NULL;
    }
    TI = tgetstr("ti", &p);     /* terminal init and exit */
    TE = tgetstr("te", &p);

    if (CL == NULL || CM == NULL || UP == NULL) {
        puts("Incomplete termcap entry\n");
        exit(1);
    }

/* will we be able to use clear to EOL? */
    eolexist = (CE != NULL);
    _CS = tgetstr("cs", &p);
    SF = tgetstr("sf", &p);
    SR = tgetstr("sr", &p);
    DL = tgetstr("dl", &p);
    AL = tgetstr("al", &p);

    if (_CS && SR) {
        if (SF == NULL) /* assume '\n' scrolls forward */
            SF = "\n";
        term.t_scroll = tcapscroll_reg;
    }
    else if (DL && AL) {
        term.t_scroll = tcapscroll_delins;
    }
    else {
        term.t_scroll = NULL;
    }

    if (p >= &tcapbuf[TCAPSLEN]) {
        puts("Terminal description too big!\n");
        exit(1);
    }
#if USE_BROKEN_OPTIMIZATION
    term_init_ok = 1;
    }
#endif
    ttopen();
}

static void tcapclose(void) {
    putp(tgoto(CM, 0, term.t_mbline));
    putp(TE);
    ttflush();
    ttclose();
}

static void tcapkopen(void) {
    putp(TI);
    ttflush();
    ttrow = -1;
    ttcol = -1;
    sgarbf = TRUE;
}

static void tcapkclose(void) {
/* Each of these three will have just called TTclose (-> tcapclose())
 * immediately before TTkclose (-> tcapkclose()).
 * So don't send TE again, as on some systems (OE Linux PVR boxes)
 * this clears the *current* screen, and sending it twice means
 * that the original screen gets cleared (as you've switched to it before
 * the second clear arrives).
 * Actually, don't need to do anything for any valid system now.
 */
#if 0
    putp(TE);
    ttflush();
#endif
}

static void tcapmove(int row, int col) {
    putp(tgoto(CM, col, row));
}

static void tcapeeol(void) {
    putp(CE);
}

static void tcapeeop(void) {
    putp(CL);
}

/* Change reverse video status
 *
 * @state: FALSE = normal video, TRUE = reverse video.
 */
static void tcaprev(int state) {
    if (state) {
        if (SO != NULL) putp(SO);
    }
    else if (SE != NULL) putp(SE);
}

/* Change screen resolution. */
static int tcapcres(char *res) {
    UNUSED(res);
    return TRUE;
}

/* move howmanylines lines starting at from to to */
static void tcapscroll_reg(int from, int to, int howmanylines) {
    int i;
    if (to == from) return;
    if (to < from) {
        tcapscrollregion(to, from + howmanylines - 1);
        tcapmove(from + howmanylines - 1, 0);
        for (i = from - to; i > 0; i--) putp(SF);
    }
    else {  /* from < to */
        tcapscrollregion(from, to + howmanylines - 1);
        tcapmove(from, 0);
        for (i = to - from; i > 0; i--) putp(SR);
    }
    tcapscrollregion(0, term.t_mbline);
}

/* move howmanylines lines starting at from to to */
static void tcapscroll_delins(int from, int to, int howmanylines) {
    int i;
    if (to == from) return;
    if (to < from) {
        tcapmove(to, 0);
        for (i = from - to; i > 0; i--) putp(DL);
        tcapmove(to + howmanylines, 0);
        for (i = from - to; i > 0; i--) putp(AL);
    }
    else {
        tcapmove(from + howmanylines, 0);
        for (i = to - from; i > 0; i--) putp(DL);
        tcapmove(from, 0);
        for (i = to - from; i > 0; i--) putp(AL);
    }
}

/* cs is set up just like cm, so we use tgoto... */
static void tcapscrollregion(int top, int bot) {
    ttputc(PC);
    putp(tgoto(_CS, bot, top));
}

#if COLOR
/* No colors here, ignore this. */
static void tcapfcol(int color) {
    UNUSED(color);
}
/* No colors here, ignore this. */
static void tcapbcol(int color) {
    UNUSED(color);
}
#endif

static void tcapbeep(void) {
        ttputc(BEL);
}
