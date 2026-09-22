/*      tcap.c
 *
 *      Unix SysV and BS4 Termcap video driver
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <signal.h>

/* Old Centos doesn't have const in the header files.
 * Well, it does sort-off, but NCURSES_CONST is set to nothing in the
 * header files, and you can't do anything to get it set to const.
 * So get them out of the way, and define "const" versions later.
 * SunOS is similar, but worse.
 */

#include <curses.h>
#if (!__clang__ &&__GNUC__ <= 4) || __sun__
#define tgetstr     tgetstr_OOTW
#define tgetnum     tgetnum_OOTW
#endif
#if __sun__
#define tgoto       tgoto_OOTW
#define putp        putp_OOTW
#endif

#include <term.h>

#if (!__clang__ &&__GNUC__ <= 4) || __sun__
#undef tgetstr
extern char *tgetstr(const char *, char **);
#undef tgetnum
extern int tgetnum(const char *);
#endif
#if __sun__
#undef tgoto
extern char *tgoto(const char *, int, int);
#undef putp
extern int putp(const char *);
#endif

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

static const char *UP, *PC, *CM, *CE, *CL, *SO, *SE, *TI, *TE,
     *_CS, *DL, *AL, *SF, *SR;

/* Define the functions that will be put into the struct terminal */

static void tcapmove(int row, int col) {
    putp(tgoto(CM, col, row));
}

/* CS is set up just like CM, so we use tgoto... */
static void tcapscrollregion(int top, int bot) {
    ttputc(*PC);
    putp(tgoto(_CS, bot, top));
}

/* Move howmanylines lines starting at from to to - if CS is dedined */
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

/* Move howmanylines lines starting at from to to - if CS is not defined */
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

/* The TERM vqalue, and associated termcap entries, are not going to
 * change, so only get them once.
 */
char *termval = NULL;
static void tcapopen(void) {
    char *t;
    char tcbuf[1024];
    char err_str[72];
    int int_col, int_row;

    if (!termval) {     /* We only do the termcp setup once */
        if ((termval = getenv("TERM")) == NULL) {
            puts("Environment variable TERM not defined!");
            exit(1);
        }

        if ((tgetent(tcbuf, termval)) != 1) {
/* Handle overlong TERM settings. Only print the first 40 chars */
            const char *xtra = "";
            if (strlen(termval) > 40) xtra = "...";
            sprintf(err_str, "Unknown terminal type: %.40s%s!", termval, xtra);
            puts(err_str);
            exit(1);
        }

/* Get screen size from system, or else from termcap.  */

        getscreensize(&int_col, &int_row, TRUE);
        if ((int_col <= 0) && (int_col = (short)tgetnum("co")) == -1) {
            puts("Termcap entry incomplete (columns)");
            exit(1);
        }
        if ((int_row <= 0) && (int_row = (short)tgetnum("li")) == -1) {
            puts("termcap entry incomplete (lines)");
            exit(1);
        }
        term.t_ncol = int_col;
        SET_t_nrow(int_row);
        set_scrarray_size(term.t_nrow, term.t_ncol);

        t = tgetstr("pc", NULL);
        if (t) PC = t;
        else   PC = "";             /* So *PC is NUL */

        CL = tgetstr("cl", NULL);
        CM = tgetstr("cm", NULL);
        CE = tgetstr("ce", NULL);
        UP = tgetstr("up", NULL);
        SE = tgetstr("se", NULL);
        SO = tgetstr("so", NULL);
        revexist = (SO != NULL);
        if (tgetnum("sg") > 0) {    /* Can reverse be used? P.K. */
            revexist = FALSE;
            SE = NULL;
            SO = NULL;
        }
        TI = tgetstr("ti", NULL);     /* terminal init and exit */
        TE = tgetstr("te", NULL);

        if (CL == NULL || CM == NULL || UP == NULL) {
            puts("Incomplete termcap entry\n");
            exit(1);
        }

/* will we be able to use clear to EOL? */
        eolexist = (CE != NULL);
        _CS = tgetstr("cs", NULL);
        SF = tgetstr("sf", NULL);
        SR = tgetstr("sr", NULL);
        DL = tgetstr("dl", NULL);
        AL = tgetstr("al", NULL);

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
    }
    ttopen();
}

static void tcapclose(void) {
    putp(tgoto(CM, 0, term.t_mbline));
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
 * Done by removing the putp(TE) from tcapclose(), as it seems logical
 * to have it here.
 */
    putp(TE);
    ttflush();
}

static void tcapeeol(void) {
    putp(CE);
}

static void tcapeeop(void) {
    putp(CL);
}

#define ESC "\x1b"

/* Change highlight status
 *
 * @state: FALSE = normal video, TRUE = highlight video.
 */
static void tcaphilite(int state) {

    if ((db_len(hifcolor) == 0) && (db_len(hibcolor) == 0)) {
        if (state) {
            if (SO != NULL) putp(SO);
        }
        else if (SE != NULL) putp(SE);
    }
    else {

/* Code to set the highlight colour.
 * Sets both in the same escape sequence.
 */
        char obuf[64];
        if (state) {
            const char *sep;
            if ((db_len(hifcolor) > 0) && (db_len(hibcolor) > 0))
                sep = ";";
            else
                sep = "";
            snprintf(obuf, 64, ESC "[%s%s%sm",
                 db_val(hifcolor), sep, db_val(hibcolor));
        }
        else {
            const char *fg = (db_len(glfcolor) > 0)? db_val(glfcolor): "39";
            snprintf(obuf, 64, ESC "[%s;49m", fg);

        }
        putp(obuf);
    }
}

/* Change screen resolution. */
static int tcapcres(char *res) {
    UNUSED(res);
    return TRUE;
}

/* Code to set the foreground colour
 * Should only be called when the colour is changed, or the screen state
 * is unknown.
 * If called with an empty colour string it unsets colours.
 */
static void tcapfgrnd(int set) {
    char obuf[64];
    const char *esq;
/* Esc[39;49m can reset colors for linux/xterm*, but Esc[0m works
 * on all tested systems.
 */
    if (!set || (db_len(glfcolor) == 0)) esq = "0";
    else esq = db_val(glfcolor);
    snprintf(obuf, 64, ESC "[%sm", esq);
    putp(obuf);
    if (set) {  /* Don't do this if we're not actually setting. */
        sgarbf = TRUE;
    }
}

#define BEL     0x07
static void tcapbeep(void) {
        ttputc(BEL);
}

/* Declare this now that we've decalred all of the functions. */

struct terminal term = {
/* Functions */
    tcapopen,
    tcapclose,
    tcapkopen,
    tcapkclose,
    ttgetc,
    ttungetc,
    ttputc,
    ttflush,
    tcapmove,
    tcapeeol,
    tcapeeop,
    tcapbeep,
    tcaphilite,
    tcapcres,
    tcapfgrnd,
    NULL,               /* Set dynamically at open time */
/* "Constants" (== variables that are set)
 * The next eight values are set dynamically at open/resize time.
 */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};
