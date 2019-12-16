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
/* If the screen size has changed whilst we were away on the command line
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

/* Create a subjob with a copy of the command interpreter in it. When the
 * command interpreter exits, mark the screen as garbage so that you do a full
 * repaint. Bound to "^X C".
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */
int spawncli(int f, int n) {
    UNUSED(f); UNUSED(n);
#if USG | BSD
    char *cp;
#endif

/* Don't allow this command if restricted */
    if (restflag) return resterr();

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

int bktoshell(int f, int n) {   /* suspend MicroEMACS and wait to wake up */
    UNUSED(f); UNUSED(n);
    vttidy();
/******************************
    int pid;

    pid = getpid();
    kill(pid,SIGTSTP);
******************************/
    kill(0, SIGTSTP);
    rtfrmshell();               /* fg seems to get us back to here... */
    return TRUE;
}

void rtfrmshell(void) {
    TTopen();
    curwp->w_flag = WFHARD;
    sgarbf = TRUE;
}
#endif

/* Backend for running a one-liner in a subjob.
 * When the command returns, optionally wait for a <return> before
 * returning character to be typed, then mark the screen as garbage
 * so a full repaint is done.
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 *
 * There are two front-ends for this.
 * spawn (shell-command) bound to "C-X !".
 *      This only waits for <return> if interactive (not running a macro).
 * execprg (execute-program) bound to "C-X $".
 *      This always waits for <return>.
 */

/* So reexecute re-runs same command.
 * Since this can't recurse internally we only need to remember the
 * last one, so don't need to set this after our "working".
 * Also, only one of spawn(), execprg(), pipecmd() and filter_buffer()
 * can ever be the "last executed" for reexecute, so they can share
 * the prev_spawn_cmd setting.
 */
static char prev_spawn_cmd[NLINE] = "";
static int next_spawn_cmd(int rxtest, char *prompt, char *line) {
    if (inreex && (prev_spawn_cmd[0] != '\0') && rxtest) {
        strcpy(line, prev_spawn_cmd);
    }
    else {
        int s;
        if ((s = mlreply(prompt, line, NLINE, CMPLT_NONE)) != TRUE) return s;
        strcpy(prev_spawn_cmd, line);
    }
    return TRUE;
}
static int run_one_liner(int rxcopy, int wait, char *prompt) {
    int s;
    char line[NLINE];

/* Don't allow this command if restricted */
    if (restflag) return resterr();

#ifdef SIGWINCH
    get_orig_size();
#endif
#if USG | BSD
    if ((s = next_spawn_cmd(rxcopy, prompt, line)) != TRUE) return s;
    TTflush();
    TTclose();              /* stty to old modes    */
    TTkclose();
    dnc = system(line);
    fflush(stdout);         /* to be sure P.K.      */
    TTopen();

    if (wait) {
        mlputs(MLbkt("Press <return> to continue")); /* Pause */
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

/* The two front-ends for run_one_liner */
int spawn(int f, int n) {
    UNUSED(f); UNUSED(n);
    return run_one_liner(RXARG(spawn), clexec == FALSE, "!");
}

int execprg(int f, int n) {
    UNUSED(f); UNUSED(n);
    return run_one_liner(RXARG(execprg), TRUE, "$");
}

/* Pipe a one line command into a window
 * Bound to ^X @
 *
 * Don't allow this in the minibuffer, as there's a forced redraw on
 * return, which ends up clearing the minibuffer data, while displaying
 * its status in the status line
 */
#define PIPEFILE ".ue_command"
int pipecmd(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;                  /* return status from CLI */
    struct window *wp;      /* pointer to new window */
    struct buffer *bp;      /* pointer to buffer to zot */
    char line[NLINE];       /* command line send to shell */
    static char bf_name[] = PIPEFILE;

/* Don't allow this command if restricted */
    if (restflag) return resterr();

#ifdef SIGWINCH
    get_orig_size();
#endif
/* Get the command to pipe in */
    if ((s = next_spawn_cmd(RXARG(pipecmd), "@", line)) != TRUE) return s;

/* Get rid of the command output buffer if it exists */
    if ((bp = bfind(bf_name, FALSE, 0)) != FALSE) {
/* Try to make sure we are off screen */
        wp = wheadp;
        while (wp != NULL) {
            if (wp->w_bufp == bp) {
                if (wp == curwp) delwind(FALSE, 1);
                else onlywind(FALSE, 1);
                break;
            }
            wp = wp->w_wndp;
        }
        if (zotbuf(bp) != TRUE) return FALSE;
    }
#if USG | BSD
    TTflush();
    TTclose();              /* stty to old modes    */
    TTkclose();
    strcat(line, ">");
    strcat(line, bf_name);
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
    if (s != TRUE) return s;

/* Split the current window to make room for the command output */
    if (splitwind(FALSE, 1) == FALSE) return FALSE;

/* And read the stuff in */
    if (getfile(bf_name, FALSE, FALSE) == FALSE) return FALSE;

/* Make this window in VIEW mode, update all mode lines */
    curwp->w_bufp->b_mode |= MDVIEW;
    wp = wheadp;
    while (wp != NULL) {
        wp->w_flag |= WFMODE;
        wp = wp->w_wndp;
    }

/* And get rid of the temporary file */
    unlink(bf_name);
    return TRUE;
}

/* Filter a buffer through an external DOS program
 * Bound to ^X #
 */
int filter_buffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;                  /* return status from CLI */
    struct buffer *bp;      /* pointer to buffer to zot */
    char line[NLINE];       /* command line send to shell */
    char tmpnam[NFILEN];    /* place to store real file name */
    static char bname1[] = "fltinp";

    static char filnam1[] = "fltinp";
    static char filnam2[] = "fltout";

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    if (curbp->b_mode & MDVIEW) /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */

#ifdef SIGWINCH
    get_orig_size();
#endif
/* Get the filter name and its args */
    if ((s = next_spawn_cmd(RXARG(filter_buffer), "#", line)) != TRUE) return s;

/* Setup the proper file names */
    bp = curbp;
    strcpy(tmpnam, bp->b_fname);    /* save the original name */
    strcpy(bp->b_fname, bname1);    /* set it to our new one */

/* Write it out, checking for errors */
    if (writeout(filnam1) != TRUE) {
        mlwrite_one(MLbkt("Cannot write filter file"));
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

/* Unset this flag, otherwise readin() prompts for "Discard changes" if
 * the original buffer (which we've just written out...to edit) was marked
 * as modifdied. 
 */
    bp->b_flag &= ~BFCHG;

/* Report any failure, then continue to tidy up... */
    if (s != TRUE || ((s = readin(filnam2, FALSE)) == FALSE)) {
        mlwrite_one(MLbkt("Execution failed"));
    }

/* Reset file name */
    strcpy(bp->b_fname, tmpnam);    /* restore name */
    bp->b_flag |= BFCHG;            /* flag it as changed */

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

/* And get rid of the temporary file */
    unlink(filnam1);
    unlink(filnam2);
    return s;
}
