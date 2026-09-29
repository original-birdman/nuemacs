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

/* We run these several times, so centralize it. */

enum TTway { OPEN, CLOSE };
static void TTstate(enum TTway w) {
    if (w == OPEN) {
        TTopen();
        TTforg(TRUE);
        TTflush();
    }
    else {
        TTforg(FALSE);  /* Just send the escape sequence */
        TTclose();      /* stty to old modes    */
    }
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

    movecursor(term.t_mbline, 0);   /* Seek to last line.   */
    TTstate(CLOSE);
    if ((cp = getenv("SHELL")) != NULL && *cp != '\0') {
/* SHELL should be a simple command with no args.
 * So run it within ''.
 */
        db_sprintf(glb_db, "exec '%s'", cp);
        rval = system(db_val(glb_db));
    }
    else {
#ifdef SYSSHELL
/* Stringify macros... */
#define xstr(s) str(s)
#define str(s) #s

        rval = system("exec " xstr(SYSSHELL));
#else
        rval = system("exec /bin/sh");
#endif
    }
    sleep(2);
    TTstate(OPEN);
    checkscreensize(0);

    return TRUE;
}

int bktoshell(int f, int n) {   /* Suspend MicroEMACS and wait to wake up */
    UNUSED(f); UNUSED(n);

    TTstate(CLOSE);

    kill(0, SIGTSTP);

/* fg seems to get us back to here...we need to get the modeline redrawn
 * as otherwise it may contain the minibuffer info (from EscX suspend-emacs).
 */
    curwp->w_flag = WFHARD | WFMODE;
    TTstate(OPEN);
    checkscreensize(0);

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
static db_bufdef(prev_spawn_cmd);
static int next_spawn_cmd(int rxtest, const char *prompt, db *line) {
    if (inreex && (db_len(prev_spawn_cmd) > 0) && rxtest) {
        dbp_copy(line, &prev_spawn_cmd);
    }
    else {
        int s;
        if ((s = mlreply(prompt, line, CMPLT_NONE)) != TRUE) return s;
        db_copy(prev_spawn_cmd, line);
    }
    return TRUE;
}
/* The return code from this function is the return code of the
 * uemacs code ONLY.
 * To get the exit status of the system comamnd, check $rval after
 * the uemacs command completes.
 */
static int run_one_liner(int rxcopy, int wait, const char *prompt) {
    int s;
    db_bufdef(line);

/* Don't allow this command if restricted */
    if (restflag) return resterr();

    if ((s = next_spawn_cmd(rxcopy, prompt, &line)) != TRUE) goto exit;

    TTstate(CLOSE);

    rval = system(db_val(line));
    if (WIFEXITED(rval)) {          /* exit code */
        rval = WEXITSTATUS(rval);
    }
    else if (WIFSIGNALED(rval)) {   /* exited on a signal */
        rval = 256 + WTERMSIG(s);
    }
    else rval = INT_MIN;            /* Unknown */
    fflush(stdout);                 /* to be sure P.K. */

    if (wait) {
        fputs(MLbkt("Press <return> to continue"), stdout); /* Pause */
        fflush(stdout);
        while (1) {
            int k = fgetc(stdin);
            if (k == '\n') break;
            if (k == '\r') break;
            if (k == EOF) break;
        };
    }
    TTstate(OPEN);
    checkscreensize(0);
    curwp->w_flag = WFHARD;
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

    db_bufdef(line);        /* command line sent to shell */
    db_bufdef(cmd);         /* command from user */
    db_bufdef(comfile);

/* Get the command to pipe in */
    if ((s = next_spawn_cmd(RXARG(pipecmd), "@", &cmd)) != TRUE) goto exit;

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
    const char *hp = udir.home? udir.home: ".";
    db_sprintf(comfile, "%s/.ue_XXXXXX", hp);
/* The text will only be overwritten - which is OK */
    int fd = mkstemp((char *)db_val(comfile));
    if (fd < 0) {
        mlwrite("Cannot create tempfile: %s", strerror(errno));
        s = FALSE;
        goto exit;
    }
    close(fd);

    TTstate(CLOSE);
/* We put the outfile filename into '', to prevent any active chars
 * being introduced via HOME setting.
 * Does mean that you can't have a ' in HOME.
 */
    db_sprintf(line, "%s >'%s'", db_val(cmd), db_val(comfile));
    rval = system(db_val(line));

    TTstate(OPEN);
    checkscreensize(0);

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
    db_free(cmd);
    return s;
}

/* Filter a buffer through an external OS program
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

    db_bufdef(line);        /* command line send to shell */
    db_bufdef(cmd);         /* command from user */
    db_bufdef(tmpnam);      /* place to store real file name */
    db_bufdef(fltin);
    db_bufdef(fltout);

/* Get the filter name and its args */
    if ((s = next_spawn_cmd(RXARG(filter_buffer), "#", &cmd)) != TRUE)
         goto exit;

/* Setup the proper file names */
    bp = curbp;
    db_set(tmpnam, bp->b_rpname);   /* Save the (full) original name */
    const char *hp = udir.home;
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
    int fd;
    db_sprintf(fltin, "%s/.ue_fin_XXXXXX", hp);
/* The text will only be overwritten - which is OK */
    if ((fd = mkstemp((char *)db_val(fltin))) < 0) {
        mlwrite("Cannot create filter input file: %s", strerror(errno));
        s = FALSE;
        goto exit;
    }
    close(fd);

    db_sprintf(fltout, "%s/.ue_fout_XXXXXX", hp);
/* The text will only be overwritten - which is OK */
    if ((fd = mkstemp((char *)db_val(fltout))) < 0) {
        mlwrite("Cannot create filter output file: %s", strerror(errno));
        s = FALSE;
        goto exit;
    }
    close(fd);

/* Set this to our new one for */
    set_buffer_filenames(bp, db_val(fltin));

/* Write it out, checking for errors */
    if (writeout(db_val(fltin)) != TRUE) {
        mlwrite_one(MLbkt("Cannot write filter file"));
        s = FALSE;
        goto reset_bufname_exit;
    }
    mlwrite_one("\r\n");    /* Get to col1 */
    TTstate(CLOSE);
/* We put the infile and outfile filenames into '', to prevent any
 * active chars being introduced via HOME setting.
 * Does mean that you can't have a ' in HOME.
  */
    db_sprintf(line, "%s <'%s' >'%s'", db_val(cmd), db_val(fltin),
         db_val(fltout));
    rval = system(db_val(line));

    TTstate(OPEN);
    checkscreensize(0);

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
    db_free(cmd);
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
