/*      spawn.c
 *
 *      Various operating system access commands.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#define SPAWN_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

/* If the screen size has changed whilst we were away on the command line
 * force a redraw to the new size.
 * Requires setting the new values (lwidth and lheight) and restoring
 * the old ones (term.t_nrow and term.t_ncol). (Just setting the
 * originals to zero can lead to crashes in vtputc).
 * Need to restore t_mbline and t_vscreen too.
 * Call this at the end of any function that calls system().
 */
static int orig_width, orig_height;
#define get_orig_size() (getscreensize(&orig_width, &orig_height))

static void check_for_resize(void) {
    int lwidth, lheight;
    getscreensize(&lwidth, &lheight);
    if ((lwidth != orig_width) || (lheight != orig_height)) {
        chg_width = lwidth;
        chg_height = lheight;
        SET_t_nrow(orig_height);
        term.t_ncol = orig_width;
    }
    return;
}

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
    char *cp;

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    get_orig_size();

    movecursor(term.t_mbline, 0);   /* Seek to last line.   */
    TTflush();
    TTclose();                      /* stty to old settings */
    TTkclose();                     /* Close "keyboard" */
    if ((cp = getenv("SHELL")) != NULL && *cp != '\0')
        rval = system(cp);
    else
#ifdef SYSSHELL
/* Stringify macros... */
#define xstr(s) str(s)
#define str(s) #s

        rval = system("exec " xstr(SYSSHELL));
#else
        rval = system("exec /bin/sh");
#endif
    sgarbf = TRUE;
    sleep(2);
    TTopen();
    TTkopen();

    check_for_resize();

    return TRUE;
}

int bktoshell(int f, int n) {   /* Suspend MicroEMACS and wait to wake up */
    UNUSED(f); UNUSED(n);
    vttidy();
/******************************
    int pid;

    pid = getpid();
    kill(pid,SIGTSTP);
******************************/
    kill(0, SIGTSTP);

/* fg seems to get us back to here... */
    TTopen();
    curwp->w_flag = WFHARD;
    sgarbf = TRUE;

    return TRUE;
}

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
static db_strdef(prev_spawn_cmd);
static int next_spawn_cmd(int rxtest, char *prompt, db *line) {
    if (inreex && (db_len(prev_spawn_cmd) > 0) && rxtest) {
        dbp_set(line, db_val(prev_spawn_cmd));
    }
    else {
        int s;
        if ((s = mlreply(prompt, line, CMPLT_NONE)) != TRUE) return s;
        db_set(prev_spawn_cmd, dbp_val(line));
    }
    return TRUE;
}
static int run_one_liner(int rxcopy, int wait, char *prompt) {
    int s;
    db_strdef(line);

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    get_orig_size();

    if ((s = next_spawn_cmd(rxcopy, prompt, &line)) != TRUE) goto exit;
    TTflush();
    TTclose();              /* stty to old modes    */
    TTkclose();
    rval = system(db_val(line));
    fflush(stdout);         /* to be sure P.K.      */
    TTopen();

    if (wait) {
        mlwrite_one(MLbkt("Press <return> to continue")); /* Pause */
        TTflush();
        while ((s = tgetc()) != '\r' && s != ' ');
        mlwrite_one("\r\n");
    }
    TTkopen();
    sgarbf = TRUE;

    check_for_resize();

exit:
    db_free(line);
    return s;
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
#define PIPEBUF ".ue_command"
int pipecmd(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;                  /* return status from CLI */
    struct window *wp;      /* pointer to new window */
    struct buffer *bp;      /* pointer to buffer to zot */

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    get_orig_size();

    db_strdef(line);        /* command line sent to shell */
    db_strdef(comfile);

/* Get the command to pipe in */
    if ((s = next_spawn_cmd(RXARG(pipecmd), "@", &line)) != TRUE) goto exit;

/* Find/create the PIPEBUF buffer, switch to it and ensure it is empty. */
    if ( ((bp = bfind(PIPEBUF, TRUE, 0)) == NULL) ||
         (swbuffer(bp, 0) != TRUE) ||
         (bclear(bp) != TRUE) ) {
        s = FALSE;
        goto exit;
    }

/* Create the tempfile via mkstemp() so the name is unpredictable.
 * The old "$HOME/.ue_<pid>" pattern let a co-located attacker pre-create
 * a symlink at that path; the shell's `>` redirection would then follow
 * it and truncate an arbitrary file the user could write.
 * mkstemp() creates the file owned-by-us with mode 0600 and a name an
 * attacker can't predict; the subsequent shell `>` re-opens it, which is
 * fine because the file already exists.
 */
    {
        char *hp = udir.home? udir.home: ".";
        char tmpl[PATH_MAX];
        snprintf(tmpl, sizeof(tmpl), "%s/.ue_XXXXXX", hp);
        int fd = mkstemp(tmpl);
        if (fd < 0) {
            mlwrite("Cannot create tempfile: %s", strerror(errno));
            s = FALSE;
            goto exit;
        }
        close(fd);
        db_set(comfile, tmpl);
    }
    TTflush();
    TTclose();              /* stty to old modes    */
    TTkclose();
    db_append(line, ">");
    db_append(line, db_val(comfile));
    rval = system(db_val(line));
    TTopen();
    TTkopen();
    TTflush();
    sgarbf = TRUE;

    check_for_resize();

/* Split the current window to make room for the command output */
    if (splitwind(FALSE, 1) == FALSE) { s = FALSE; goto exit; }

/* And read the stuff in */
    if (readin(db_val(comfile), FALSE) != TRUE) { s = FALSE; goto exit; }
    terminate_str(bp->b_dfname);    /* Zap temporary filename */
    terminate_str(bp->b_rpname);    /* Zap temporary filename */

/* Put this window into VIEW mode.*/
    curwp->w_bufp->b_mode |= MDVIEW;
    wp = wheadp;
    while (wp != NULL) {    /* Update all mode lines */
        wp->w_flag |= WFMODE;
        wp = wp->w_wndp;
    }

    s = TRUE;

exit:
/* Always remove the tempfile on the way out, even on failure paths
 * (mkstemp creates it before we know if system() succeeds, and the old
 * "return FALSE" mid-function used to leak both the file and the dbs).
 */
    if (db_len(comfile) > 0) unlink(db_val(comfile));
    db_free(comfile);
    db_free(line);
    return s;
}

/* Filter a buffer through an external DOS program
 * Bound to ^X #
 */
int filter_buffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;                  /* return status from CLI */
    struct buffer *bp;      /* pointer to buffer to zot */

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    if (curbp->b_mode & MDVIEW) /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */

    get_orig_size();

    db_strdef(line);         /* command line send to shell */
    db_strdef(tmpnam);       /* place to store real file name */
    db_strdef(fltin);
    db_strdef(fltout);

/* Get the filter name and its args */
    if ((s = next_spawn_cmd(RXARG(filter_buffer), "#", &line)) != TRUE)
         goto exit;

/* Setup the proper file names */
    bp = curbp;
    db_set(tmpnam, bp->b_rpname);   /* Save the (full) original name */
    char *hp = udir.home;
    if (!hp) hp = ".";              /* Default if absent */

/* Create the in/out tempfiles via mkstemp() so the names are
 * unpredictable. The old "$HOME/.ue_f{in,out}_<pid>" pattern was a
 * symlink hazard: an attacker could pre-create either path as a
 * symlink, then have the shell's `<`/`>` redirection follow it.
 * NOTE: the previous code baked the `<`/`>` redirection characters into
 * fltin/fltout themselves and then passed db_val(fltin) to writeout(),
 * which tried to open a file literally named "<HOME/...". That was
 * broken — fltin/fltout now hold the path only; the redirection chars
 * are added when building the shell command (below).
 */
    {
        char tmpl[PATH_MAX];
        int fd;

        snprintf(tmpl, sizeof(tmpl), "%s/.ue_fin_XXXXXX", hp);
        if ((fd = mkstemp(tmpl)) < 0) {
            mlwrite("Cannot create filter input file: %s", strerror(errno));
            s = FALSE;
            goto exit;
        }
        close(fd);
        db_set(fltin, tmpl);

        snprintf(tmpl, sizeof(tmpl), "%s/.ue_fout_XXXXXX", hp);
        if ((fd = mkstemp(tmpl)) < 0) {
            mlwrite("Cannot create filter output file: %s", strerror(errno));
            s = FALSE;
            goto exit;
        }
        close(fd);
        db_set(fltout, tmpl);
    }

/* Set this to our new one for */
    set_buffer_filenames(bp, db_val(fltin));

/* Write it out, checking for errors */
    if (writeout(db_val(fltin)) != TRUE) {
        mlwrite_one(MLbkt("Cannot write filter file"));
        s = FALSE;
        goto reset_bufname_exit;
    }
    ttput1c('\n');          /* Already have '\r'    */
    TTflush();
    TTclose();              /* stty to old modes    */
    TTkclose();
    db_addch(line, '<');
    db_append(line, db_val(fltin));
    db_addch(line, '>');
    db_append(line, db_val(fltout));
    rval = system(db_val(line));
    TTopen();
    TTkopen();
    TTflush();
    sgarbf = TRUE;

    check_for_resize();

/* Unset this flag, otherwise readin() prompts for "Discard changes" if
 * the original buffer (which we've just written out...to edit) was marked
 * as modified.
 */
    bp->b_flag &= ~BFCHG;

/* If we are modfying the buffer that the match-group info points
 * to we want to mark them as invalid - readin() will do that.
 * Report any failure, then continue to tidy up... */
    if ((s = readin(db_val(fltout), FALSE)) == FALSE) {
        mlwrite_one(MLbkt("Execution failed"));
    }
    bp->b_flag |= BFCHG;            /* Flag it as changed */

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

reset_bufname_exit:
    set_buffer_filenames(bp, db_val(tmpnam));
exit:
/* Always remove the tempfiles on the way out — including on any of the
 * early-error paths above that goto exit. mkstemp() may have created
 * one or both; len==0 means we never got that far.
 */
    if (db_len(fltin) > 0) unlink(db_val(fltin));
    if (db_len(fltout) > 0) unlink(db_val(fltout));
    db_free(fltout);
    db_free(fltin);
    db_free(tmpnam);
    db_free(line);
    return s;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_spawn(void) {
    db_free(prev_spawn_cmd);
}
#endif
