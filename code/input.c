/*      input.c
 *
 *      Various input routines
 *
 *      written by Daniel Lawrence 5/9/86
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "wrapper.h"

/* GGR - some needed things for minibuffer handling */
#if UNIX
#include <signal.h>
#ifdef SIGWINCH
static int remap_c_on_intr = 0;
#endif
#endif
#include "line.h"

#if UNIX
#define COMPLC  1
#else
#define COMPLC  0
#endif

/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. Used any time a confirmation is required.
 */
int mlyesno(char *prompt)
{
        char c;                 /* input character */
        char buf[NPAT];         /* prompt to user */

        for (;;) {
                /* build and prompt the user */
                strcpy(buf, prompt);
                strcat(buf, " " MLpre "y/n" MLpost "? ");
                mlwrite(buf);

                /* get the response */
                c = tgetc();

                if (c == ectoc(abortc)) /* Bail out! */
                        return ABORT;

                if (c == 'y' || c == 'Y')
                        return TRUE;

                if (c == 'n' || c == 'N')
                        return FALSE;
        }
}

/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */

int mlreply(char *prompt, char *buf, int nbuf)
{
        return nextarg(prompt, buf, nbuf, ctoec('\n'));
}

int mlreplyt(char *prompt, char *buf, int nbuf, int eolchar)
{
        return nextarg(prompt, buf, nbuf, eolchar);
}

/* GGR
 * The bizarre thing about the two functions above is that when processing
 * macros from a (start-up) file they will get one arg at a time.
 * BUT when run interactively they will return the entire prompt.
 * This just complicates any caller's code.
 */
int mlreplyall(char *prompt, char *buf, int nbuf) {
        if (!clexec)
            return nextarg(prompt, buf, nbuf, ctoec('\n'));
        int nb = strlen(execstr);
        memcpy(buf, execstr, nb+1); /* Copy the NUL */
        execstr += nb;              /* Show we've read it all */
        return TRUE;
}

/*
 * ectoc:
 *      expanded character to character
 *      collapse the CONTROL and SPEC flags back into an ascii code
 */
int ectoc(int c)
{
        if (c & CONTROL)
                c = c & ~(CONTROL | 0x40);
        if (c & SPEC)
                c = c & 255;
        return c;
}

/*
 * ctoec:
 *      character to extended character
 *      pull out the CONTROL and SPEC prefixes (if possible)
 */
int ctoec(int c)
{
        if (c >= 0x00 && c <= 0x1F)
                c = CONTROL | (c + '@');
        return c;
}

/*
 * get a command name from the command line. Command completion means
 * that pressing a <SPACE> will attempt to complete an unfinished command
 * name if it is unique.
 */
fn_t getname(char *prompt) {
    int cpos;               /* current column on screen output */
    int c;
    char *sp;               /* pointer to string for output */
    struct name_bind *ffp;  /* first ptr to entry in name binding table */
    struct name_bind *cffp; /* current ptr to entry in name binding table */
    struct name_bind *lffp; /* last ptr to entry in name binding table */
    char buf[NSTRING];      /* buffer to hold tentative command name */

/* starting at the beginning of the string buffer */
    cpos = 0;

/* If we are executing a command line get the next arg and match it */
    if (clexec) {
        if (macarg(buf) != TRUE) return NULL;
        struct name_bind *nbp = name_info(buf);
        if (nbp) return nbp->n_func;
        return NULL;
    }

/* Prompt... */
    mlwrite(prompt);
    mpresf = TRUE;          /* GGR */

/* Build a name string from the keyboard */
    while (TRUE) {
#ifdef SIGWINCH
        remap_c_on_intr = 1;
#endif
        c = tgetc();
#ifdef SIGWINCH
        remap_c_on_intr = 0;
        if (c == UEM_NOCHAR) {  /* SIGWINCH */
            char mlbuf[1024];
            snprintf(mlbuf, 1024, "%s%.*s", prompt, cpos, buf);
            mlwrite(mlbuf);
            continue;
        }
#endif

/* If we are at the end, just match it */
        if (c == 0x0d) {
            buf[cpos] = 0;  /* and match it off */
            struct name_bind *nbp = name_info(buf);
            if (nbp) {
                if (kbdmode == RECORD) addto_kbdmacro(buf, 1, 0);
                return nbp->n_func;
            }
            return NULL;
        }
        else if (c == ectoc(abortc)) {        /* Bell, abort */
            ctrlg(FALSE, 0);
            TTflush();
            return NULL;
        }
        else if (c == 0x7F || c == 0x08) {    /* rubout/erase */
            if (cpos != 0) {
                TTputc('\b');           /* Local handling */
                TTputc(' ');
                TTputc('\b');
                --ttcol;
                --cpos;
                TTflush();
            }
        } else if (c == 0x15) { /* C-U, kill */
            while (cpos != 0) {
                TTputc('\b');
                TTputc(' ');
                TTputc('\b');
                --cpos;
                --ttcol;
            }
            TTflush();
        }
        else if (c == ' ' || c == 0x1b || c == 0x09) {
/* Attempt a completion.
 * Because of the completion code we can't use the sorted index...
 */
            buf[cpos] = 0;  /* terminate it for us */
            ffp = &names[0];        /* scan for matches */
            while (ffp->n_func != NULL) {
                if (strncmp(buf, ffp->n_name, strlen(buf)) == 0) {
/* A possible match! More than one? */
                    if ((ffp + 1)->n_func == NULL ||
/* Also take an exact match, otherwise we can never match "set",
 * as other functions start with "set", so we can never get a trailing
 * space in
 */
                        (strcmp(buf, ffp->n_name) == 0) ||
                        (strncmp(buf, (ffp + 1)->n_name, strlen(buf)) != 0)) {
/* No...we match, print it */
                        sp = ffp->n_name + cpos;
                        while (*sp) {
                            ttput1c(*sp++);
                            if (!zerowidth_type(*sp)) ttcol++;
                        }
                        TTflush();
                        if (kbdmode == RECORD)
                            addto_kbdmacro(ffp->n_name, 1, 0);
                        return ffp->n_func;
                    }
                    else {
/* << << << << << << << << << << << << << << << << << */
/* try for a partial match against the list
 * first scan down until we no longer match the current input
 */
                        lffp = (ffp + 1);
                        while ((lffp + 1)->n_func != NULL) {
                            if (strncmp(buf, (lffp+1)->n_name,
                                 strlen(buf)) != 0) break;
                            ++lffp;
                        }
/* and now, attempt to partial complete the string, char at a time */
                        while (TRUE) {
/* Add the next char in */
                            buf[cpos] = ffp->n_name[cpos];
/* Scan through the candidates */
                            cffp = ffp + 1;
                            while (cffp <= lffp) {
                                if (cffp->n_name[cpos] != buf[cpos])
                                     goto onward;
                                ++cffp;
                            }
/* Add the character */
                            ttput1c(buf[cpos++]);
                            if (!zerowidth_type(buf[cpos])) ttcol++;
                        }
/* << << << << << << << << << << << << << << << << << */
                    }
                }
                ++ffp;
            }
/* No match.....beep and onward */
            TTbeep();
onward:
            TTflush();
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
        }
        else {
            if (cpos < NSTRING - 1 && c > ' ') {
                buf[cpos++] = c;
                ttput1c(c);
                if (!zerowidth_type(c)) ttcol++;
            }
            TTflush();
        }
    }
}

/*      tgetc:  Get a key from the terminal driver, resolve any keyboard
                macro action                                    */

int tgetc(void) {
    int c;                      /* fetched character */

/* If we are playing a keyboard macro back, */
    if (kbdmode == PLAY) {
        if (kbdptr < kbdend)    /* if there is some left... */
            return (int) *kbdptr++;
        if (--kbdrep < 1) {     /* at the end of last repitition? */
            kbdmode = STOP;
#if     VISMAC == 0
            update(FALSE);      /* force a screen update after all is done */
#endif
        }
        else {
            kbdptr = &kbdm[0];  /* reset macro to begining for the next rep */
            return (int) *kbdptr++;
        }
    }

/* Fetch a character from the terminal driver */
#ifdef SIGWINCH
    struct sigaction sigact;
    if (remap_c_on_intr) {
        sigaction(SIGWINCH, NULL, &sigact);
        sigact.sa_flags = 0;
        sigaction(SIGWINCH, &sigact, NULL);
        errno = 0;
    }
#endif
    c = TTgetc();
#ifdef SIGWINCH
    if (remap_c_on_intr) {
        sigaction(SIGWINCH, NULL, &sigact);
        sigact.sa_flags = SA_RESTART;
        sigaction(SIGWINCH, &sigact, NULL);
    }
#endif
/* Record it for $lastkey */
    lastkey = c;

#ifdef SIGWINCH
    if (c == 0 && errno == EINTR && remap_c_on_intr)
        c = UEM_NOCHAR;         /* Note illegal char */
    else
#endif
/* Save it if we need to */
        if (kbdmode == RECORD) {
            *kbdptr++ = c;
            kbdend = kbdptr;
            if (kbdptr == &kbdm[NKBDM - 1]) {   /* don't overrun buffer */
                kbdmode = STOP;
                TTbeep();
            }
        }

/* And finally give the char back */
    return c;
}

/*      GET1KEY:        Get one keystroke. The only prefixs legal here
                        are the SPEC and CONTROL prefixes.
                                                                */

int get1key(void)
{
        int c;

        /* get a keystroke */
        c = tgetc();
        if (c >= 0x00 && c <= 0x1F)     /* C0 control -> C-     */
                c = CONTROL | (c + '@');
        return c;
}

/*      GETCMD: Get a command from the keyboard. Process all applicable
                prefix keys
                                                        */
int getcmd(void)
{
        int c;                  /* fetched keystroke */
#if VT220
        int d;                  /* second character P.K. */
        int cmask = 0;
#endif
        /* get initial character */
        c = get1key();

#if VT220
proc_metac:
        if (c == 128+27)                /* CSI - ~equiv to Esc[ */
                goto handle_CSI;
#endif
        /* process META prefix */
        if (c == (CONTROL | '[')) {
                c = get1key();
#if VT220
                if (c == '[' || c == 'O') {     /* CSI P.K. */
handle_CSI:
                        c = get1key();
                        if (c >= 'A' && c <= 'D')
                                return SPEC | c | cmask;
                        if (c >= 'E' && c <= 'z' && c != 'i' && c != 'c')
                                return SPEC | c | cmask;
                        d = get1key();
                        if (d == '~')   /* ESC [ n ~   P.K. */
                                return SPEC | c | cmask;
                        switch (c) {    /* ESC [ n n ~ P.K. */
                        case '1':
                                c = d + 32;
                                break;
                        case '2':
                                c = d + 48;
                                break;
                        case '3':
                                c = d + 64;
                                break;
                        default:
                                c = '?';
                                break;
                        }
                        if (d != '~')   /* eat tilde P.K. */
                                get1key();
                        if (c == 'i') { /* DO key    P.K. */
                                c = ctlxc;
                                goto proc_ctlxc;
                        } else if (c == 'c')    /* ESC key   P.K. */
                                c = get1key();
                        else
                                return SPEC | c | cmask;
                }
                if (c == (CONTROL | '[')) {
                        cmask = META;
                        goto proc_metac;
                }
#endif
                if (islower(c)) /* Force to upper */
                        c ^= DIFCASE;
                if (c >= 0x00 && c <= 0x1F)     /* control key */
                        c = CONTROL | (c + '@');
                return META | c;
        }
        else if (c == metac) {
                c = get1key();
#if VT220
                if (c == (CONTROL | '[')) {
                        cmask = META;
                        goto proc_metac;
                }
#endif
                if (islower(c)) /* Force to upper */
                        c ^= DIFCASE;
                if (c >= 0x00 && c <= 0x1F)     /* control key */
                        c = CONTROL | (c + '@');
                return META | c;
        }

#if     VT220
proc_ctlxc:
#endif
        /* process CTLX prefix */
        if (c == ctlxc) {
                c = get1key();
#if VT220
                if (c == (CONTROL | '[')) {
                        cmask = CTLX;
                        goto proc_metac;
                }
#endif
                if (c >= 'a' && c <= 'z')       /* Force to upper */
                        c -= 0x20;
                if (c >= 0x00 && c <= 0x1F)     /* control key */
                        c = CONTROL | (c + '@');
                return CTLX | c;
        }

        /* otherwise, just return it */
        return c;
}

/* GGR
 * A version of getstring in which the minibuffer is a true buffer!
 * Note that this loops for each character, so you can manipulate the
 * prompt etc. by providing updated info and using it after the
 * loop: label.
 */

/* If the window size changes whilst this is running the display will end
 * up incorrect (as the code reckons we are in this buffer, not the one we
 * arrived from).
 * So set up a SIGWINCH handler to get us out the minibuffer before
 * display::newscreensize() runs.
 */

static struct window *mb_winp = NULL;

#ifdef SIGWINCH

typedef void (*sighandler_t)(int);

void sigwinch_handler(int signr) {

    UNUSED(signr);

/* We need to get back to how things were before we arrived in teh
 * minibuffer.
 * So we save the current settings, restore the originals, let the
 * resize code run, re-fetch teh original (in case they ahve changed)
 * the restore the ones we arrived with.
 */
    struct buffer *mb_bp = curbp;
    struct window *mb_wp = curwp;
    struct window *mb_hp = wheadp;

    curbp = mb_info.main_bp;
    curwp = mb_info.main_wp;
    wheadp = mb_info.wheadp;
    inmb = FALSE;

/* Do the bits from sizesignal() in display.c that do the work */
    int w, h;

    getscreensize(&w, &h);
    if (h && w && (h - 1 != term.t_nrow || w != term.t_ncol))
        newscreensize(h, w, 0);

/* Need to reget the mb_info data now */

    mb_info.main_bp = curbp;
    mb_info.main_wp = curwp;
    mb_info.wheadp = wheadp;

/* Now get back to how we arrived */

    curbp = mb_bp;
    curwp = mb_wp;
    wheadp = mb_hp;
    curwp->w_toprow = term.t_nrow;  /* Set new value */
    inmb = TRUE;
/* Ensure the minibuffer is redrawn */
    mbupdate();

    return;
}
#endif

int getstring(char *prompt, char *buf, int nbuf, int eolchar) {
    UNUSED(eolchar);        /* Historic... */
    struct buffer *bp;
    struct buffer *cb;
    char mbname[NBUFN];
    int    c;               /* command character */
    int    f;               /* default flag */
    int    n;               /* numeric repeat count */
    int    basec;           /* c stripped of meta character */
    int    mflag;           /* negative flag on repeat */
    struct line *lp;        /* line to copy */
    int size;
    char *sp;               /* string pointer into line */
    char tstring[NSTRING];
    char choices[1000];     /* MUST be > max likely window width */
    int status;
    int savclast;
    int savflast;
    int savnlast;

    short savdoto;
    int prolen;
    char procopy[NSTRING];

/* gcc 4.4.7 is happy to report "may be used initialized" for wsave
 * (which is wrong) even though it doesn't have a -Wmaybe-uninitialized
 * option.
 * So if we find we are using that, use this hammer...
 */
#if __GNUC__ == 4 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ == 7
 volatile
#endif
    struct window wsave;
    short bufexpand, expanded;

#ifdef SIGWINCH
/* We need to block SIGWINCH until we have set-up all of the variables
 * we need after the longjmp.
 */
    sigset_t sigwinch_set, incoming_set;
    sigemptyset(&sigwinch_set);
    sigaddset(&sigwinch_set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &sigwinch_set, &incoming_set);
#endif

/* Create a minibuffer window for use by all minibuffers */
    if (!mb_winp) {
        mb_winp = (struct window *)malloc(sizeof(struct window));
        if (mb_winp == NULL) exit(1);
        mb_winp->w_wndp = NULL;               /* Initialize window    */
#if     COLOR
/* initalize colors to global defaults */
        mb_winp->w_fcolor = gfcolor;
        mb_winp->w_bcolor = gbcolor;
#endif
        mb_winp->w_toprow = term.t_nrow;
        mb_winp->w_ntrows = 1;
        mb_winp->w_fcol = 0;
        mb_winp->w_force = 0;
        mb_winp->w_flag = WFMODE | WFHARD;    /* Full.                */
    }

/* GGR NOTE!!
 * We want to ensure that we don't return with garbage in buf, but we
 * can't initialize it to empty here, as some callers use it as a
 * temporary buffer for the prompt!
 */

/* Expansion commands leave junk in mb */

    mlerase();
    sprintf(mbname, "//minib%04d", mb_info.mbdepth+1);
    cb = curbp;

/* Update main screen before entering minibuffer */

    update(FALSE);

    if ((bp = bfind(mbname, TRUE, BFINVS)) == NULL) {
        buf = "";               /* Ensure we never return garbage */
        sigprocmask(SIG_SETMASK, &incoming_set, NULL);
        return(FALSE);
    }
    bufexpand = (nbuf == NBUFN);

/* Save real reexecution history */

    savclast = clast;
    savflast = flast;
    savnlast = nlast;

/* Remember the original buffer name if at level 1 */

/* Get the PHON state from the current buffer, so we can carry it to
 * the next minibuffer.
 * We *don't* carry back any change on return!
 */
    int new_bmode;
    if (curbp->b_mode & MDPHON) new_bmode = MDEXACT | MDPHON;
    else                        new_bmode = MDEXACT;

    if (++mb_info.mbdepth == 1) {   /* Arrival into minibuffer */
        mb_info.main_bp = curbp;    /* For main buffer info in modeline */
        mb_info.main_wp = curwp;    /* Used to position modeline */
        mb_info.wheadp = wheadp;    /* ??? */
        inmb = TRUE;
    }
    else {                          /* Save this minibuffer for recursion */
        wsave = *curwp;             /* Structure copy... */
    }
/* Switch to the minibuffer window */
    curwp = mb_winp;
    wheadp = curwp;

/* Set-up the (incoming) prompt string and clear any prompt update flag,
 * as it should only get set during loop:
 */
    strcpy(procopy, prompt);
    prolen = strlen(procopy);
    prmpt_buf.update = 0;

    if (mpresf) mlerase();
    mberase();

    swbuffer(bp);

    curwp->w_toprow = term.t_nrow;
    curwp->w_ntrows = 1;
    curbp->b_mode = new_bmode;

#ifdef SIGWINCH
/* The oldact is restored on exit. */
    struct sigaction sigact, oldact;
    sigact.sa_handler = sigwinch_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sigact, &oldact);
/* Now we can enable the signal */
    sigprocmask(SIG_SETMASK, &incoming_set, NULL);
#endif

/* A copy of the main.c command loop from 3.9e, but things are a
 *  *little* different here..
 */
loop:

/* Execute the "command" macro...normally null */
    saveflag = lastflag;        /* preserve lastflag through this */
    execute(META|SPEC|'C', FALSE, 1);
    lastflag = saveflag;

/* Have we been asked to update the prompt? */

    if (prmpt_buf.update) {
        strcpy(procopy, prmpt_buf.prompt);
        prolen = strlen(procopy);
        prmpt_buf.update = 0;
    }

/* Have we been asked to load (a search string)? */
    if (prmpt_buf.preload) {
        linstr(prmpt_buf.preload);
        prmpt_buf.preload = NULL;    /* One-time usage */
    }

/* Prepend the prompt to the beginning of the visible line */
    savdoto = curwp->w_doto;
    curwp->w_doto = 0;
    linstr(procopy);
    curwp->w_doto = savdoto + prolen;

/* Fix up the screen - we do NOT want horizontal scrolling in the mb */

    int real_hscroll;
    real_hscroll = hscroll;
    hscroll = FALSE;
    curwp->w_flag |= WFMODE;    /* Need to update the modeline... */
    mbupdate();                 /* Will set modeline to prompt... */
    hscroll = real_hscroll;

/* Remove the prompt from the beginning of the visible line */
    curwp->w_doto = 0;
    ldelete((long)prolen, FALSE);
    curwp->w_doto = savdoto;

/* Get the next command (character) from the keyboard */
#ifdef SIGWINCH
    remap_c_on_intr = 1;
#endif
    c = getcmd();
/* We get UEM_NOCHAR back on a sigwin signal. */
#ifdef SIGWINCH
    remap_c_on_intr = 0;
    if (c == UEM_NOCHAR) goto loop;
#endif

/* Filename expansion code:
 * a list of matches is temporarily displayed in the minibuffer.
 * THIS ONLY USES THE TEXT ON THE CURRENT LINE, and uses ALL OF IT.
 */
    if (c == (CONTROL|'I')) {
        lp = curwp->w_dotp;
        sp = lp->l_text;
/* NSTRING-1, as we need to add a trailing NUL */
        if (lp->l_used < NSTRING-1) {
            memcpy(tstring, sp, lp->l_used);
            tstring[lp->l_used] = '\0';
            if (bufexpand) expanded = comp_buffer(tstring, choices);
            else           expanded = comp_file(tstring, choices);
            if (expanded) {
                savdoto = curwp->w_doto;
                curwp->w_doto = 0;
                ldelete((long) lp->l_used, FALSE);
                linstr(tstring);
                if (choices[0]) {
                    mlwrite(choices);
                    size = (strlen(choices) < 42) ? 1 : 2;
                    sleep(size);
                    mlerase();
                    mberase();
                }
            }
            else
                TTbeep();
            goto loop;
        }
        else {
            TTbeep();
            goto loop;
        }
    }
/* End of filename expansion code */

    f = FALSE;
    n = 1;

/* Do META-# processing if needed */

    basec = c & ~META;          /* strip meta char off if there */
    if ((c & META) && ((basec >= '0' && basec <= '9') || basec == '-')) {
        f = TRUE;               /* there is a # arg */
        n = 0;                  /* start with a zero default */
        mflag = 1;              /* current minus flag */
        c = basec;              /* strip the META */
        while ((c >= '0' && c <= '9') || (c == '-')) {
            if (c == '-') {     /* already hit a minus or digit? */
                if ((mflag == -1) || (n != 0)) break;
                mflag = -1;
            }
            else {
                n = n * 10 + (c - '0');
            }
            if ((n == 0) && (mflag == -1))  /* lonely - */
                mlwrite("Arg:");
            else
                mlwrite("Arg: %d",n * mflag);

            c = getcmd();       /* get the next key */
        }
        n = n * mflag;          /* figure in the sign */
    }

/* Do ^U repeat argument processing */

    if (c == reptc) {           /* ^U, start argument   */
        f = TRUE;
        n = 4;                  /* with argument of 4 */
        mflag = 0;              /* that can be discarded. */
        mlwrite("Arg: 4");
        while (((c=getcmd()) >='0' && c<='9') || c==reptc || c=='-'){
            if (c == reptc)
                if ((n > 0) == ((n*4) > 0)) n = n*4;
                else n = 1;
/*
 * If dash, and start of argument string, set arg.
 * to -1.  Otherwise, insert it.
 */
            else if (c == '-') {
                if (mflag) break;
                n = 0;
                mflag = -1;
            }
/*
 * If first digit entered, replace previous argument
 * with digit and set sign.  Otherwise, append to arg.
 */
            else {
                if (!mflag) {
                    n = 0;
                    mflag = 1;
                }
                n = 10*n + c - '0';
            }
            mlwrite("Arg: %d", (mflag >=0) ? n : (n ? -n : -1));
        }
/*
 * Make arguments preceded by a minus sign negative and change
 * the special argument "^U -" to an effective "^U -1".
 */
        if (mflag == -1) {
            if (n == 0) n++;
            n = -n;
        }
    }

/* Intercept minibuffer specials.. <NL> and ^G */

    if ((c == (CONTROL|'M')) || (c == (CTLX|CONTROL|'C')))
        goto submit;
    else if (c == (CONTROL|'G')) {
        status = ABORT;
        buf = "";               /* Don't return garbage */
        goto abort;
    }

/* ...and execute the command. DON'T MACRO_CAPTURE THIS!! */

    execute(c, f, n);
    if (mpresf) {
        sleep(1);
        mlerase();
        mberase();
    }

/* Whatever dumb modes, they've put on, allow only the sensible... */

    curbp->b_mode &= (MDEXACT|MDOVER|MDMAGIC|MDPHON|MDEQUIV);
    goto loop;

submit:     /* Tidy up */
    status = TRUE;

/* Find the contents of the current buffer, and its length, including
 * newlines to join lines, but excluding the final newline.
 * Only do this upto the requested return length.
 */

    struct line *mblp = lforw(bp->b_linep);
    int sofar = 0;
    int maxadd = nbuf - 1;
    while (mblp != bp->b_linep && sofar < maxadd) {
        if (sofar != 0) buf[sofar++] = '\n';    /* Add NL if not first */
        int add = llength(mblp);
        if ((sofar + add) > maxadd) add = maxadd - sofar;
        memcpy(buf+sofar, mblp->l_text, add);
        sofar += add;
        mblp = lforw(mblp);
    }
    buf[sofar] = '\0';          /* Add the NUL */

/* Need to copy to return buffer and, if not empty,
 * to save as last minibuffer.
 */
    int retlen = sofar;         /* Without terminating NUL */
    if (retlen) {
        if (retlen >= NSTRING) retlen = NSTRING - 1;
        addto_lastmb_ring(buf); /* Terminating NULL is actually there */
    }
    else status = FALSE;        /* Empty input... */

/* Record the result if we are recording a keyboard macro, but only
 * at first level of the minibuffer (i.e. the "true" result).
 */
    if (kbdmode == RECORD && mb_info.mbdepth == 1) addto_kbdmacro(buf, 0, 1);

abort:

#ifdef SIGWINCH
/* If we get here "normally" SIGWINCH will still be enabled, so we need
 * to block it while we tidy up, otherwise we might run through this end
 * code twice.
 */
    sigprocmask(SIG_BLOCK, &sigwinch_set, NULL);
#endif

    swbuffer(bp);   /* Make sure we're still in our minibuffer */
    unmark(TRUE, 1);

    if (--mb_info.mbdepth) {        /* Back to previous mini-buffer */
        swbuffer(cb);               /* Switch to its buffer... */
        *curwp = wsave;             /* ...and its window info */
    }
    else {                          /* Leaving minibuffer */
        swbuffer(mb_info.main_bp);  /* Switch back to main buffer */
        curwp = mb_info.main_wp;    /* Reset window info ptr */
        wheadp = mb_info.wheadp;    /* Reset window list */
        inmb = FALSE;               /* Note we have left mb */
    }
    curwp->w_flag |= WFMODE;        /* Forces modeline redraw */
    zotbuf(bp);                     /* Remove this minibuffer */

/* Restore real reexecution history */

    clast = savclast;
    flast = savflast;
    nlast = savnlast;

    mberase();
    if (status == ABORT) {
        ctrlg(FALSE, 0);
        TTflush();
    }

#ifdef SIGWINCH
/* We need to re-instate the original handler now... */
    sigaction(SIGWINCH, &oldact, NULL);
    sigprocmask(SIG_SETMASK, &incoming_set, NULL);
#endif

    return(status);
}
