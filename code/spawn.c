/*      spawn.c
 *
 *      Various operating system access commands.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

#if USG | BSD
#include        <signal.h>
#endif

static int dnc __attribute__ ((unused));   /* GGR - a throwaway */

#ifdef SIGWINCH
/*
 * If the screen size has changed whilst we were away on the command line
 * force a redraw to the new size.
 * Requires setting the new values (lwidth and lheight) and restoring
 * the old ones (term.t_nrow and term.t_ncol). (Just setting the
 * originals to zero can lead to crashes in vtputc).
 * Call this at the end of any function that calls system().
 */
static int orig_width, orig_height;
void get_orig_size(void) {
        getscreensize(&orig_width, &orig_height);
        return;
}
void check_for_resize(void) {
        int lwidth, lheight;
        getscreensize(&lwidth, &lheight);
        if ((lwidth != orig_width) || (lheight != orig_height)) {
            chg_width = lwidth;
            chg_height = lheight;
            term.t_nrow = orig_height - 1;
            term.t_ncol = orig_width;
        }
        return;
}
#endif

/*
 * Create a subjob with a copy of the command intrepreter in it. When the
 * command interpreter exits, mark the screen as garbage so that you do a full
 * repaint. Bound to "^X C".
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */
int spawncli(int f, int n)
{
        UNUSED(f); UNUSED(n);
#if USG | BSD
        char *cp;
#endif

        /* don't allow this command if restricted */
        if (restflag)
                return resterr();

#ifdef SIGWINCH
        get_orig_size();
#endif
#if USG | BSD
        movecursor(term.t_nrow, 0);     /* Seek to last line.   */
        TTflush();
        TTclose();                      /* stty to old settings */
        TTkclose();                     /* Close "keyboard" */
        if ((cp = getenv("SHELL")) != NULL && *cp != '\0')
                dnc = system(cp);
        else
#if BSD
                dnc = system("exec /bin/csh");
#else
                dnc = system("exec /bin/sh");
#endif
        sgarbf = TRUE;
        sleep(2);
        TTopen();
        TTkopen();
#ifdef SIGWINCH
        check_for_resize();
#endif
        return TRUE;
#endif
}

#if BSD | __hpux | SVR4

int bktoshell(int f, int n)
{                               /* suspend MicroEMACS and wait to wake up */
        UNUSED(f); UNUSED(n);
        vttidy();
/******************************
        int pid;

        pid = getpid();
        kill(pid,SIGTSTP);
******************************/
        kill(0, SIGTSTP);
        rtfrmshell();           /* fg seems to get us back to here... */
        return TRUE;
}

void rtfrmshell(void)
{
        TTopen();
        curwp->w_flag = WFHARD;
        sgarbf = TRUE;
}
#endif

/*
 * Run a one-liner in a subjob. When the command returns, wait for a single
 * character to be typed, then mark the screen as garbage so a full repaint is
 * done. Bound to "C-X !".
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */
int spawn(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int s;
        char line[NLINE];

        /* don't allow this command if restricted */
        if (restflag)
                return resterr();

#ifdef SIGWINCH
        get_orig_size();
#endif
#if USG | BSD
        if ((s = mlreply("!", line, NLINE)) != TRUE)
                return s;
        TTflush();
        TTclose();              /* stty to old modes    */
        TTkclose();
        dnc = system(line);
        fflush(stdout);         /* to be sure P.K.      */
        TTopen();

        if (clexec == FALSE) {
                mlputs(MLpre "Press <return> to continue" MLpost); /* Pause */
                TTflush();
                while ((s = tgetc()) != '\r' && s != ' ');
                mlputs("\r\n");
        }
        TTkopen();
        sgarbf = TRUE;
#ifdef SIGWINCH
        check_for_resize();
#endif
        return TRUE;
#endif
}

/*
 * Run an external program with arguments. When it returns, wait for a single
 * character to be typed, then mark the screen as garbage so a full repaint is
 * done. Bound to "C-X $".
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */

int execprg(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int s;
        char line[NLINE];

        /* don't allow this command if restricted */
        if (restflag)
                return resterr();

#ifdef SIGWINCH
        get_orig_size();
#endif
#if USG | BSD
        if ((s = mlreply("!", line, NLINE)) != TRUE)
                return s;
        ttput1c('\n');          /* Already have '\r'    */
        TTflush();
        TTclose();              /* stty to old modes    */
        TTkclose();
        dnc = system(line);
        fflush(stdout);         /* to be sure P.K.      */
        TTopen();
        mlputs(MLpre "Press <return> to continue" MLpost); /* Pause */
        TTflush();
        while ((s = tgetc()) != '\r' && s != ' ');
        sgarbf = TRUE;
#ifdef SIGWINCH
        check_for_resize();
#endif
        return TRUE;
#endif
}

/*
 * Pipe a one line command into a window
 * Bound to ^X @
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */
int pipecmd(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int s;                  /* return status from CLI */
        struct window *wp;      /* pointer to new window */
        struct buffer *bp;      /* pointer to buffer to zot */
        char line[NLINE];       /* command line send to shell */
        static char bname[] = "command";

        static char filnam[NSTRING] = "command";

        /* don't allow this command if restricted */
        if (restflag)
                return resterr();

#ifdef SIGWINCH
        get_orig_size();
#endif
        /* get the command to pipe in */
        if ((s = mlreply("@", line, NLINE)) != TRUE)
                return s;

        /* get rid of the command output buffer if it exists */
        if ((bp = bfind(bname, FALSE, 0)) != FALSE) {
                /* try to make sure we are off screen */
                wp = wheadp;
                while (wp != NULL) {
                        if (wp->w_bufp == bp) {
                                if (wp == curwp)
                                        delwind(FALSE, 1);
                                else
                                        onlywind(FALSE, 1);
                                break;
                        }
                        wp = wp->w_wndp;
                }
                if (zotbuf(bp) != TRUE)

                        return FALSE;
        }
#if USG | BSD
        TTflush();
        TTclose();              /* stty to old modes    */
        TTkclose();
        strcat(line, ">");
        strcat(line, filnam);
        dnc = system(line);
        TTopen();
        TTkopen();
        TTflush();
        sgarbf = TRUE;
        s = TRUE;
#endif
#ifdef SIGWINCH
        check_for_resize();
#endif
        if (s != TRUE)
                return s;

        /* split the current window to make room for the command output */
        if (splitwind(FALSE, 1) == FALSE)
                return FALSE;

        /* and read the stuff in */
        if (getfile(filnam, FALSE) == FALSE)
                return FALSE;

        /* make this window in VIEW mode, update all mode lines */
        curwp->w_bufp->b_mode |= MDVIEW;
        wp = wheadp;
        while (wp != NULL) {
                wp->w_flag |= WFMODE;
                wp = wp->w_wndp;
        }

        /* and get rid of the temporary file */
        unlink(filnam);
        return TRUE;
}

/*
 * filter a buffer through an external DOS program
 * Bound to ^X #
 */
int filter_buffer(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int s;                  /* return status from CLI */
        struct buffer *bp;      /* pointer to buffer to zot */
        char line[NLINE];       /* command line send to shell */
        char tmpnam[NFILEN];    /* place to store real file name */
        static char bname1[] = "fltinp";

        static char filnam1[] = "fltinp";
        static char filnam2[] = "fltout";

        /* don't allow this command if restricted */
        if (restflag)
                return resterr();

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */

#ifdef SIGWINCH
        get_orig_size();
#endif
        /* get the filter name and its args */
        if ((s = mlreply("#", line, NLINE)) != TRUE)
                return s;

        /* setup the proper file names */
        bp = curbp;
        strcpy(tmpnam, bp->b_fname);    /* save the original name */
        strcpy(bp->b_fname, bname1);    /* set it to our new one */

        /* write it out, checking for errors */
        if (writeout(filnam1) != TRUE) {
                mlwrite(MLpre "Cannot write filter file" MLpost);
                strcpy(bp->b_fname, tmpnam);
                return FALSE;
        }
#if USG | BSD
        ttput1c('\n');          /* Already have '\r'    */
        TTflush();
        TTclose();              /* stty to old modes    */
        TTkclose();
        strcat(line, " <fltinp >fltout");
        dnc = system(line);
        TTopen();
        TTkopen();
        TTflush();
        sgarbf = TRUE;
        s = TRUE;
#endif
#ifdef SIGWINCH
        check_for_resize();
#endif
        /* on failure, escape gracefully */
        if (s != TRUE || (readin(filnam2, FALSE) == FALSE)) {
                mlwrite(MLpre "Execution failed" MLpost);
                strcpy(bp->b_fname, tmpnam);
                unlink(filnam1);
                unlink(filnam2);
                return s;
        }

        /* reset file name */
        strcpy(bp->b_fname, tmpnam);    /* restore name */
        bp->b_flag |= BFCHG;            /* flag it as changed */

/* If this is a translation table, remove any compiled data */

        if ((bp->b_type == BTPHON) && bp->ptt_headp)
                    ptt_free(bp);

        /* and get rid of the temporary file */
        unlink(filnam1);
        unlink(filnam2);
        return TRUE;
}
