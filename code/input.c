/*      input.c
 *
 *      Various input routines
 *
 *      written by Daniel Lawrence 5/9/86
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "wrapper.h"

/* GGR - some needed things for minibuffer handling */
#include "line.h"

static int mbdepth = 0;
                
#if MSDOS | BSD | USG
static int tmpnamincr = 0;
#define tmpnam our_tmpnam
static char *tmpnam(char *);

#if MSC
void sleep();
#define MAXDEPTH 8
#endif
#endif


#if     PKCODE
#if     MSDOS && TURBO
#include        <dir.h>
#endif
#endif

#if     PKCODE && (UNIX || (MSDOS && TURBO))
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

                /* get the responce */
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
fn_t getname(void)
{
        int cpos;               /* current column on screen output */
        int c;
        char *sp;               /* pointer to string for output */
        struct name_bind *ffp;  /* first ptr to entry in name binding table */
        struct name_bind *cffp; /* current ptr to entry in name binding table */
        struct name_bind *lffp; /* last ptr to entry in name binding table */
        char buf[NSTRING];      /* buffer to hold tentative command name */

        /* starting at the beginning of the string buffer */
        cpos = 0;

        /* if we are executing a command line get the next arg and match it */
        if (clexec) {
                if (macarg(buf) != TRUE)
                        return NULL;
                return fncmatch(&buf[0]);
        }

        /* build a name string from the keyboard */
        while (TRUE) {
                c = tgetc();

                /* if we are at the end, just match it */
                if (c == 0x0d) {
                        buf[cpos] = 0;

                        /* and match it off */
                        return fncmatch(&buf[0]);

                } else if (c == ectoc(abortc)) {        /* Bell, abort */
                        ctrlg(FALSE, 0);
                        TTflush();
                        return NULL;

                } else if (c == 0x7F || c == 0x08) {    /* rubout/erase */
                        if (cpos != 0) {
                                TTputc('\b');
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

                } else if (c == ' ' || c == 0x1b || c == 0x09) {
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
                        /* attempt a completion */
                        buf[cpos] = 0;  /* terminate it for us */
                        ffp = &names[0];        /* scan for matches */
                        while (ffp->n_func != NULL) {
                                if (strncmp(buf, ffp->n_name, strlen(buf))
                                    == 0) {
                                        /* a possible match! More than one? */
                                        if ((ffp + 1)->n_func == NULL ||
                                            (strncmp
                                             (buf, (ffp + 1)->n_name,
                                              strlen(buf)) != 0)) {
                                                /* no...we match, print it */
                                                sp = ffp->n_name + cpos;
                                                while (*sp)
                                                        TTputc(*sp++);
                                                TTflush();
                                                return ffp->n_func;
                                        } else {
/* << << << << << << << << << << << << << << << << << */
                                                /* try for a partial match against the list */

                                                /* first scan down until we no longer match the current input */
                                                lffp = (ffp + 1);
                                                while ((lffp +
                                                        1)->n_func !=
                                                       NULL) {
                                                        if (strncmp
                                                            (buf,
                                                             (lffp +
                                                              1)->n_name,
                                                             strlen(buf))
                                                            != 0)
                                                                break;
                                                        ++lffp;
                                                }

                                                /* and now, attempt to partial complete the string, char at a time */
                                                while (TRUE) {
                                                        /* add the next char in */
                                                        buf[cpos] =
                                                            ffp->
                                                            n_name[cpos];

                                                        /* scan through the candidates */
                                                        cffp = ffp + 1;
                                                        while (cffp <=
                                                               lffp) {
                                                                if (cffp->
                                                                    n_name
                                                                    [cpos]
                                                                    !=
                                                                    buf
                                                                    [cpos])
                                                                        goto onward;
                                                                ++cffp;
                                                        }

                                                        /* add the character */
                                                        TTputc(buf
                                                               [cpos++]);
                                                }
/* << << << << << << << << << << << << << << << << << */
                                        }
                                }
                                ++ffp;
                        }

                        /* no match.....beep and onward */
                        TTbeep();
                      onward:;
                        TTflush();
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
                } else {
                        if (cpos < NSTRING - 1 && c > ' ') {
                                buf[cpos++] = c;
                                TTputc(c);
                        }

                        ++ttcol;
                        TTflush();
                }
        }
}

/*      tgetc:  Get a key from the terminal driver, resolve any keyboard
                macro action                                    */

int tgetc(void)
{
        int c;                  /* fetched character */

        /* if we are playing a keyboard macro back, */
        if (kbdmode == PLAY) {

                /* if there is some left... */
                if (kbdptr < kbdend)
                        return (int) *kbdptr++;

                /* at the end of last repitition? */
                if (--kbdrep < 1) {
                        kbdmode = STOP;
#if     VISMAC == 0
                        /* force a screen update after all is done */
                        update(FALSE);
#endif
                } else {

                        /* reset the macro to the begining for the next rep */
                        kbdptr = &kbdm[0];
                        return (int) *kbdptr++;
                }
        }

        /* fetch a character from the terminal driver */
        c = TTgetc();

        /* record it for $lastkey */
        lastkey = c;

        /* save it if we need to */
        if (kbdmode == RECORD) {
                *kbdptr++ = c;
                kbdend = kbdptr;

                /* don't overrun the buffer */
                if (kbdptr == &kbdm[NKBDM - 1]) {
                        kbdmode = STOP;
                        TTbeep();
                }
        }

        /* and finally give the char back */
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

#if     MSDOS
        if (c == 0) {           /* Apply SPEC prefix    */
                c = tgetc();
                if (c >= 0x00 && c <= 0x1F)     /* control key? */
                        c = CONTROL | (c + '@');
                return SPEC | c;
        }
#endif

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
#endif
        if (c == 128+27)                /* CSI */
                goto handle_CSI;
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
#endif
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
#if     PKCODE
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
#endif


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
 * A version of getstring in which the  minibuffer is a true buffer!
 */

int getstring(char *prompt, char *buf, int nbuf, int eolchar)
{
    struct buffer *bp;
    struct buffer *cb;
    char mbname[NBUFN];
    char *mbnameptr;
    int    c;               /* command character */
    int    f;               /* default flag */
    int    n;               /* numeric repeat count */
    int    basec;           /* c stripped of meta character */
    int    mflag;           /* negative flag on repeat */
    struct line *lp;        /* line to copy */
    int size;
    char *sp;               /* string pointer into line */
    char tstring[NSTRING];
#if VMS
#define MATCH       0
#define NO_MATCH    1
#define CONCEALED   2
#define LIST        4
    int searchhit;
    int stripped;
    char listmatch[NSTRING];
    char savstring[NSTRING];
    char xstring[NSTRING];
    char dstring[NSTRING];
    char copy[NSTRING];
    int depth;
    int r, i, dummy;
#endif
    char choices[1000];       /* MUST be > max likely window width */
    short savdoto;
    int prolen;
    int status;
    int savclast;
    int savflast;
    int savnlast;
    char procopy[NSTRING];

    struct window wsave;

    short bufexpand, expanded;

#if MSDOS
    if (mbdepth >= MAXDEPTH) {
        TTbeep();
        return(ABORT);
    }
#endif
/* Expansion commands leave junk in mb */

    mlerase();
    mbnameptr = tmpnam(NULL);
#if VMS
    strcpy(mbname, "CC$");
    strcat(mbname, mbnameptr);
#else
    strcpy(mbname, mbnameptr);
#endif
    cb = curbp;
    inmb = TRUE;
    if ((bp = bfind(mbname, TRUE, BFINVS)) == NULL) {
        inmb = FALSE;
        return(FALSE);
    }
    bufexpand = (nbuf == NBUFN);
/* save real reexecution history */
    savclast = clast;
    savflast = flast;
    savnlast = nlast;

    wsave = *curwp;             /* Structure copy - save */
    swbuffer(bp);

    curwp->w_toprow = term.t_nrow;
    curwp->w_ntrows = 1;
    curbp->b_mode = MDEXACT;    /* start-up just with EXACT mode */

    mbdepth++;
    if (mbdepth > 1) {
        strcpy(procopy, itoa(mbdepth));
        strcat(procopy, prompt);
    }
    else
        strcpy(procopy, prompt);
    prolen = strlen(procopy);

    if (mpresf)
        mlerase();
    mberase();

/* A copy of the main.c command loop from 3.9e, but things are a
 *  *little* different here..
 */
loop:

/* Execute the "command" macro...normally null */
    saveflag = lastflag;        /* preserve lastflag through this */
    execute(META|SPEC|'C', FALSE, 1);
    lastflag = saveflag;

    savdoto = curwp->w_doto;
    curwp->w_doto  = 0;
    linstr(procopy);
    curwp->w_doto = savdoto + prolen;

/* Fix up the screen */
    update(FALSE);
    if (typahead()) {
            mbupdate();
    }

    curwp->w_doto  = 0;
    ldelete((long)prolen, FALSE);
    curwp->w_doto = savdoto;

/* Get the next command (character) from the keyboard */
    c = getcmd();

/* Filename expansion code:
 * a list of matches is temporarily displayed in the minibuffer.
 */
    if ((c == (CONTROL|'I'))) {
        lp = curwp->w_dotp;
        sp = lp->l_text;
/* NSTRING-1, as we need to add a trailing NUL */
        if (lp->l_used < NSTRING-1) {
            strncpy(tstring, sp, lp->l_used);
            tstring[lp->l_used] = '\0';
            if (bufexpand) {
                expanded = comp_buffer(tstring, choices);
            }
            else {
                expanded = comp_file(tstring, choices);
            }
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
                if ((mflag == -1) || (n != 0))
                    break;
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
                if ((n > 0) == ((n*4) > 0))
                    n = n*4;
                else
                    n = 1;
/*
 * If dash, and start of argument string, set arg.
 * to -1.  Otherwise, insert it.
 */
            else if (c == '-') {
                if (mflag)
                    break;
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
            if (n == 0)
                n++;
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

/* ...and execute the command */

    execute(c, f, n);
    if (mpresf) {
        sleep(1);
        mlerase();
        mberase();
    }

/* Whatever dumb modes, they've put on, allow only the sensible... */

    curbp->b_mode &= (MDEXACT|MDOVER|MDMAGIC);
    goto loop;

submit:     /* Tidy up */
    status = TRUE;

/* Find the contents of the current line and its length */

    lp = curwp->w_dotp;
    sp = lp->l_text;
    size = lp->l_used;

/* Need to copy to return buffer and, if not empty, 
 * to save as last minibuffer.
 */

    int retlen = size;          /* Without terminating NUL */
    if (retlen >= nbuf) retlen = nbuf - 1;
    strncpy(buf, sp, retlen);   /* No NUL sent here */
    buf[retlen] = '\0';         /* Here it is... */
    if (retlen) {
        if (retlen >= NSTRING) retlen = NSTRING - 1;
        strncpy(lastmb, sp, retlen);
        lastmb[retlen] = '\0';
    }
    else status = FALSE;        /* Empty input... */

abort:  /* Make sure we're still in our minibuffer */
    swbuffer(bp);
    unmark(TRUE, 1);
    mbdepth--;

    swbuffer(cb);
    *curwp = wsave;             /* Structure copy - restore */

    if (!mbdepth) inmb = FALSE;

#if (MSDOS & (LATTICE | MSC)) | BSD | USG
    free(mbnameptr);            /* free the space */
#endif
     zotbuf(bp);

/* Restore real reexecution history */

    clast = savclast;
    flast = savflast;
    nlast = savnlast;
    mberase();
    if (status == ABORT) {
        ctrlg(FALSE, 0);
        TTflush();
    }
    return(status);
}

/* Yank back last minibuffer */
int yankmb(int f, int n)
{
        int    c;
        int    i;
        char   *sp;    /* pointer into string to insert */

        if (curbp->b_mode&MDVIEW)       /* don't allow this command if  */
                return(rdonly());       /* we are in read only mode     */
        if (n < 0)
                return (FALSE);
        /* make sure there is something to yank */
        if (strlen(lastmb) == 0)
                return(TRUE);           /* not an error, just nothing */

        sp = &lastmb[0];
        i = strlen(lastmb);
        while (i--) {
                if ((c = *sp++) == '\n' && !inmb) {
                        if (lnewline() == FALSE)
                                return (FALSE);
                }
                else {
                        if (linsert(1, c) == FALSE)
                                return (FALSE);
                }
        }
        return (TRUE);
}

int mbstop()
{
        if (inmb) {
                mlwrite("Command not allowed in the minibuffer");
                return(TRUE);
        }
        else
                return(FALSE);
}

#if MSDOS | BSD | USG
static char *tmpnam(dum)
char *dum;
{
   char *tstring;

        tstring = malloc(NBUFN);
        strcpy(tstring, "CC$000");
        tmpnamincr++;
        strcat(tstring, itoa(tmpnamincr));
        return(tstring);
}
#endif

#if MSDOS & MSC
void sleep(n)
int n;
{
#include <dos.h>

    struct dostime_t tbuf;
    int sec, dsec, i;

    for (i=0; i<n; i++) {
        /* Yes, I *know* this is really lazy... */
        _dos_gettime(&tbuf);
        sec =  (int)tbuf.second;
        dsec = (int)tbuf.hsecond / 10;
        while ((int)tbuf.second == sec)
            _dos_gettime(&tbuf);
        while (((int)tbuf.hsecond / 10) != dsec)
            _dos_gettime(&tbuf);
    }
}
#endif

/* GGR - Back to original code... */

/*
 * output a string of characters
 *
 * char *s;             string to output
 */
void outstring(char *s)
{
        if (disinp)
                while (*s)
                        TTputc(*s++);
}

/*
 * output a string of output characters
 *
 * char *s;             string to output
 */
void ostring(char *s)
{
        if (discmd)
                while (*s)
                        TTputc(*s++);
}
