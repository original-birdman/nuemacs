/*      file.c
 *
 *      The routines in this file handle the reading, writing
 *      and lookup of disk files.  All of details about the
 *      reading and writing of the disk are in "fileio.c".
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <libgen.h>

#define FILE_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Routines to deal with standardizing filenames
 * fixup_fname:
 *  replaces any ~id at the start with its absolute path replacement,
 *  and prepends $PWD to any leading ".." or ".".
 *  It then strips out multiple consecutive "/"s and handles internal
 *  ".." and "." entries.
 *
 * fixup_full:
 *  expands any leading "." to an absolute name (something we don't always
 *  wish to do) then calls fixup_fname() with what it has.
 *
 * get_realpath:
 *  get the full physical path (symlinks resolved) of a filename.
 *  So this is a unique path for a file.
 *  The result is then shortened to start with ./, ../ or ~., if appropriate.
 *
 * THESE ALL RETURN A POINTER TO AN INTERNAL static char ARRAY.
 * The CALLER must handle appropriately.
 */
#include <libgen.h>
#include <pwd.h>

/*
 * fixup_fname
 */
char *fixup_fname(char *fn) {
    static char fn_expd[2*NFILEN]; /* Overbig, for sprint overflow warnings */

/* Look for a ~ at the start. */

    if (fn[0]=='~') {          /* HOME dir wanted... */
        if (fn[1]=='/' || fn[1]==0) {
            if (udir.home) {
                strcpy(fn_expd, udir.home);
                int i = 1;
/* Special case for root (i.e just "/")! */
                if (fn[0]=='/' && fn[1]==0 && fn_expd[1] != 0)
                    i++;
                strcat(fn_expd, fn+i);
            }
        }
#ifndef STANDALONE
        else {
           struct passwd *pwptr;
           char *p, *q;
            p = fn + 1;
            q = fn_expd;
            while (*p != 0 && *p != '/')
                *q++ = *p++;
            *q = 0;
            if ((pwptr = getpwnam(fn_expd)) != NULL) {
                sprintf(fn_expd, "%s%s", pwptr->pw_dir, p);
            }
        }
#endif
    }
    else if (udir.current &&
        ((fn[0] == '.' && fn[1] == '.' && (fn[2] == '/' || fn[2] == '\0')) ||
         (fn[0] == '.' && (fn[1] == '/' || fn[1] == '\0')))) {
/* Just prepend $PWD - the slash_* loop at the end will fix up '.' and ".." */
            sprintf(fn_expd, "%s/%s", udir.current, fn);
    }
    else strcpy(fn_expd, fn);

/* Convert any multiple consecutive "/" to "/" and strip any
 * trailing "/"...
 * Change any "/./" entries to "/".
 * Strip out ".." entries and their parent  (xxx/../yyy -> yyy)
 */
    char *from, *to;
    int prev_was_slash = 0;
    int slash_dot_state = 1;    /* 1 seen '/', 2 seen '.' */
    int slash_d_d_state = 1;    /* 1 seen '/', 2 seen '.' , 3 see '..' */
    char *resets[512];          /* location of slashes+1 */
    int rsi = 0;                /* Index into resets - points at "next" one */
    int resettable;             /* Count increments on not . or .. */

/* Work out the first reset - this should never get reset by the
 * rest of the algorithm.
 * resettable only gets incremented when we find somewhere we can reset to,
 * which means that leading ".." entries don't get removed.
 */
    if (fn_expd[0] != '/') {
        resets[0] = fn_expd;
        rsi = 1;
        resettable = 1;
    }
    else resettable = 0;

    for (from = fn_expd, to = fn_expd; *from; from++) {
/* Ignore a repeat '/' */
        if (prev_was_slash && (*from == '/')) continue;
/* Reset states on no '/' */
        if ((slash_dot_state == 2) && (*from != '/')) slash_dot_state = 0;
        if ((slash_d_d_state == 3) && (*from != '/')) slash_d_d_state = 0;
/* If at '/' do we need to "go back"? */
        if (*from == '/') {
            prev_was_slash = 1;
            if (slash_dot_state == 2) {
                to = resets[--rsi];
                resettable--;
            }
            else if (slash_d_d_state == 3) {
                if ((fn_expd[0] == '/') || (resettable >= 2)) {
                    rsi -= 2;
                    if (rsi < 0) rsi = 0;
                    resettable -= 2;
                    if (resettable < 1) resettable = 1;
                    to = resets[rsi];
                }
                else {
                    *to++ = '/';
                }
            }
            else {
                resettable++;
                *to++ = '/';
            }
            slash_dot_state = 1;            /* We have the / */
            slash_d_d_state = 1;            /* We have the / */
            resets[rsi++] = to;             /* Will be ... */
        }
        else {
/* Copy the character we've been processing */
            prev_was_slash = 0;
            *to++ = *from;
/* Advance states on '.', clear on not '.' */
            if (*from == '.') {
               if (slash_dot_state == 1) slash_dot_state = 2;
               if (slash_d_d_state == 2) slash_d_d_state = 3;
               if (slash_d_d_state == 1) slash_d_d_state = 2;
            }
            else {
                slash_dot_state = 0;
                slash_d_d_state = 0;
            }
        }
    }
/* For "./" this next line goes to before start of buffer, but we
 * fix that with the to <= fn_expd check.
 */
    if (prev_was_slash) to--;
    if (slash_dot_state == 2) to -= 2;
    if (slash_d_d_state == 3) {
        if ((fn_expd[0] == '/') || (resettable >= 2)) {
            rsi -= 2;
            if (rsi < 0) rsi = 0;
            resettable -= 2;
            if (resettable < 1) resettable = 1;
            to = resets[rsi] - 1;   /* Remove trailing / */
        }
    }
/* Just have '/' or '.' left...keep it */
    if (to <= fn_expd) to = fn_expd+1;
    terminate_str(to);

    return fn_expd;
}

/*
 * fixup_full
 */
char *fixup_full(char *fn) {
    char fn_expd[2*NFILEN]; /* Overbig, for sprint overflow warnings */

/* If the filename doesn't start with '/' or '~' we prepend "$PWD/".
 * Then we call fixup_fname() to do what it can do, which includes
 * stripping out redundant '.'s and handling ".."s.
 * Either way we copy the name into our buffer, just in case it is
 * currently already in fixup_fname()'s buffer.
 */
    if (udir.current && (fn[0] != '/' && fn[0] != '~')) {
        sprintf(fn_expd, "%s/%s", udir.current, fn);
    }
    else strcpy(fn_expd, fn);

    return fixup_fname(fn_expd);    /* For '/', '.' and ".."  handling */
}

/*
 * get_realpath
 */
char *get_realpath(char *fn) {

    static char rp_res[NFILEN];

/* Get the full pathname...(malloc'ed)
 * Have to cater for file not existing (but the dir has to...).
 */
    char *dir = dirname(strdupa(fn));
    char *ent = basename(strdupa(fn));

    char *rp = realpath(dir, NULL);
    if (!rp) return fn;     /* Return input... */
    if (strlen(rp) == 1) rp[0] = '\0';  /* Blank a bare "/" */
    sprintf(rp_res, "%s/%s", rp, ent);
    free(rp);

/* See whether we can use ., .. or ~ to shorten this.
 * We have to cater for (and ignore) an udir entry being just "/"
 * as matching that would mean lengthening, not shortening, and the
 * code here assumes it can copy strings "leftwards" char by char.
 */
    char *cfp = NULL;
    char *ctp;
    if ((udir.clen > 1) && strncmp(rp_res, udir.current, udir.clen) == 0) {
        strcpy(rp_res, "./");
        ctp = rp_res + 2;
        cfp = rp_res + udir.clen;
    }
    else if ((udir.plen > 1) && strncmp(rp_res, udir.parent, udir.plen) == 0) {
        strcpy(rp_res, "../");
        ctp = rp_res + 3;
        cfp = rp_res + udir.plen;
    } else if ((udir.hlen > 1) &&
               (strncmp(rp_res, udir.home, udir.hlen) == 0)) {
/* NOTE: that any fn input of ~/file as a real file in a dir called "~"
 * will have already been expanded to a full path, so ~/ really will be
 * unique as being under HOME.
 */
        strcpy(rp_res, "~/");
        ctp = rp_res + 2;
        cfp = rp_res + udir.hlen;
    }
    if (cfp) do { *ctp++ = *cfp; } while (*cfp++);

    return rp_res;
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

void set_buffer_filenames(struct buffer *bp, char *fname) {

/* If activating an inactive buffer, these may be the same and the
 * action of strcpy() is undefined for overlapping strings.
 * On a Mac it will crash...
 */
    if (bp->b_fname != fname) update_val(bp->b_fname, fname);
    update_val(bp->b_rpname, get_realpath(fname));
    return;
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

static int resetkey(void) { /* Reset the encryption key if needed */
    int s;              /* return status */

    cryptflag = FALSE;  /* Turn off the encryption flag */

    if (!(curbp->b_mode & MDCRYPT)) return TRUE;

/* So we are in CRYPT mode. Global or Buffer mode? */

    char *ukey;
    int klen;
    if (crypt_mode & CRYPT_GLOBAL) {
        ukey = gl_enc_key;
        klen = gl_enc_len;
    }
    else {
        ukey = curbp->b_key;
        klen = curbp->b_keylen;
    }

    if (klen == 0) {        /* No key set - so get one */
        s = set_encryption_key(FALSE, 0);
/* if this succeeded, things will be fully set */
        if (s == TRUE) cryptflag = TRUE;    /* let others know... */
        return s;
    }
    cryptflag = TRUE;           /* let others know... */

/* We get here when we already have a key, but do need to reset the
 * initial calls to myencrypt().
 * For that we need the raw encryption key, but we store this
 * encrypted. But the encryption is symmetric, so we can retrieve it by
 * encrypting it again - then use that to initalize things again.
 * Hence the odd double-set of calls.
 * We also need to know whether to reset with a buffer or global key,
 * and that is covered by the ukey/klen fetch above.
 * Since we aren't setting the length here, we don't need the indirection
 * of *klenp used in set_encryption_key() itself.
 */
    myencrypt((char *) NULL, 0);
    myencrypt(ukey, klen);
    myencrypt((char *) NULL, 0);
    myencrypt(ukey, klen);
    return TRUE;
}

/* insert file in buffer at iline.
 * Go to end of inserted file if goto_end is set.
 * Check for DOS line-endings if check_dos is set.
 * Return read status as the function value.
 * Used by ifile and readin().
 * This is not re-entrant, so use static file-wide variables to "return":
 *  lines read in nlines.
 *  whether it is a dos_file in dos_file.
 */
static int nlines, dos_file;
static int file2buf(struct line *iline, char *mode, int goto_end,
     int check_dos) {
    int s;
    struct line *lp0, *lp1, *lp2;

    nlines = 0;
    dos_file = FALSE;
    while ((s = ffgetline()) == FIOSUC) {
        lp1 = fline;            /* Allocate by ffgetline..*/
        lp0 = iline;            /* line previous to insert */
        lp2 = lp0->l_fp;        /* line after insert */

/* Re-link new line between lp0 and lp2 */
        lp2->l_bp = lp1;
        lp0->l_fp = lp1;
        lp1->l_bp = lp0;
        lp1->l_fp = lp2;

/* Then advance by remembering the current line */
        iline = lp1;

/* Check for a DOS line ending on first line, and on other lines
 * if this is marked as a DOS file.
 * NOTE, if the line is empty it cannot contain a CR!
 */
        if ((nlines == 0) && check_dos && (lused(lp1) > 0) &&
             (ltext(lp1)[lused(lp1)-1] == '\r')) dos_file = TRUE;
        if (dos_file && (lused(lp1) > 0) &&
             (ltext(lp1)[lused(lp1)-1] == '\r'))
             lused(lp1)--;                  /* Remove the trailing CR */
        if (!(++nlines % 300) && !silent)   /* GGR */
             mlwrite(MLbkt("%s file") " : %d lines", mode, nlines);
    }
    if (goto_end) curwp->w.dotp = iline;
    ffclose();              /* Ignore errors. */
    return s;
}

/* GGR routine to handle file-hooks in one place.
 * We now look for /file-hooks-<<sfx>> after /file-hooks.
 * And now done in readin() only
 */
static void handle_filehooks(char *fname) {
    struct buffer *sb;
    run_filehooks = 0;                  /* reset flag */
    if ((sb = bfind("/file-hooks", FALSE, 0)) != NULL) dobuf(sb);
    char *sfx = strrchr(fname, '.');
/* Check we haven't found ../xxx or ./xxx */
    if (sfx && (*(sfx+1) != '/')
            && strlen(sfx) <= 19) {     /* Max bufname is 32, incl NUL */
        sfx++;                          /* Skip over '.' */
        char sfx_bname[NBUFN];
        sprintf(sfx_bname, "/file-hooks-%s", sfx);
        if ((sb = bfind(sfx_bname, FALSE, 0)) != NULL) dobuf(sb);
    }
    return;
}

/* Read file "fname" into the current buffer, blowing away any text
 * found there - called by both the read and find commands.
 * Return the final status of the read.
 * Also called by the main routine to read in a file specified on the
 * command line as an argument.
 * The command bound to M-FNR (Meta+Spec+R) is called after the buffer
 * is set up and before it is read.
 *
 * char fname[];        name of file to read
 * int lockfl;          check for file locks?
 */
int readin(char *fname, int lockfl) {
    struct window *wp;
    struct buffer *bp;
    int s;

    if (filock && lockfl && lockchk(fname) == ABORT) {
        s = FIOFNF;
        bp = curbp;
        terminate_str(bp->b_fname);     /* Makes it empty */
        goto out;
    }

    bp = curbp;                             /* Cheap.        */
    if ((s = bclear(bp)) != TRUE) return s; /* Might be old. */
    bp->b_flag &= ~(BFINVS | BFCHG);

/* If we are modfying the buffer that the match-group info points
 * to we have to mark them as invalid.
 */
    if (bp == group_match_buffer) group_match_buffer = NULL;

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

/* Set the filenames */

    set_buffer_filenames(bp, fname);

/* Always set the real pathname now, so getfile() can check it.
 * NOTE that if we cannot get a real pathname (e.g trying to
 * open a new file in a non-existant directory) that we just warn
 * about it...
 */
    char *rp = get_realpath(fname);
    if (rp == fname) {  /* Unable to get real path */
        mlwrite_one("Parent directory absent for new file");
        sleep(1);
    }
    else update_val(bp->b_rpname, rp);

/* GGR - run filehooks on this iff the caller sets the flag */
    if (run_filehooks) handle_filehooks(fname);

/* Do this after file-hooks run, so they can set CRYPT mode */
    s = resetkey();
    if (s != TRUE) return s;

/* let a user macro get hold of things...if he wants
 * Don't start the handler when it is already running as that might
 * just get into a loop...
 */
    if (!meta_spec_active.R) {
        meta_spec_active.R = 1;
        execute(META|SPEC|'R', FALSE, 1);
        meta_spec_active.R = 0;
    }

/* NOTE! that all of the above (in particular) filehooks and Esc-Spec-R
 * are run *before* the file is read into the buffer.
 */
    if ((s = ffropen(fname)) == FIOERR) goto out;   /* Hard file open. */
    if (s == FIOFNF) {                              /* File not found. */
        strcpy(readin_mesg, MLbkt("New file"));
        mlwrite_one(readin_mesg);
        goto out;
    }

/* Read the file in */
    if (!silent) mlwrite_one(MLbkt("Reading file"));  /* GGR */
    s = file2buf(curbp->b_linep, "Reading", FALSE, autodos);
    if (dos_file) curbp->b_mode |= MDDOSLE;
    else          curbp->b_mode &= ~MDDOSLE;
    char *emg = "";
    if (s == FIOERR) {
        emg = "I/O ERROR, ";
        curbp->b_flag |= BFTRUNC;
    }
    if (!silent) {                      /* GGR */
        char *dmg = "";
        if (dos_file) dmg = " - from DOS file!";
        sprintf(readin_mesg, MLbkt("%sRead %d line%s%s"), emg, nlines,
             (nlines > 1)? "s": "", dmg);
        mlwrite_one(readin_mesg);
        if (s == FIOERR) sleep(1);   /* Let it be seen */
    }

out:
    for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_bufp == curbp) {
            wp->w_linep = lforw(curbp->b_linep);
            wp->w.dotp = lforw(curbp->b_linep);
            wp->w.doto = 0;
            wp->w.markp = NULL;
            wp->w.marko = 0;
            wp->w_flag |= WFMODE | WFHARD;
        }
    }
    if (s == FIOERR || s == FIOFNF) return FALSE;   /* False if error. */
    return TRUE;
}

/* Read a file into the current buffer.
 * This is really easy; all you do is find the name of the file and
 * call the standard "read a file into the current buffer" code.
 * Bound to "C-X C-R".
 */
int fileread(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;
    char fname[NFILEN];

    if (restflag)           /* don't allow this command if restricted */
        return resterr();
/* GGR - return any current filename for the buffer on <CR>.
 *      Useful for ^X^R <CR> to re-read current file on erroneous change
 */
    s = mlreply("Read file: ", fname, NFILEN, CMPLT_FILE);
    if (s == ABORT) return(s);
    else if (s == FALSE) {
        if (*(curbp->b_fname) == 0) return(s);
        else strcpy(fname, curbp->b_fname);
    }
    return readin(fname, TRUE);
}

/* Insert file "fname" into the current buffer.
 * Called by insert file command.
 * Return the final status of the read.
 */
static int ifile(char *fname) {
    struct buffer *bp;
    int s;

    bp = curbp;             /* Cheap.               */
    bp->b_flag |= BFCHG;    /* we have changed      */
    bp->b_flag &= ~BFINVS;  /* and are not temporary */

/* If we are modfying the buffer that the match-group info points
 * to we have to mark them as invalid.
 */
    if (bp == group_match_buffer) group_match_buffer = NULL;

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

    pathexpand = FALSE;     /* GGR */

    if ((s = ffropen(fname)) == FIOERR) goto out;   /* Hard file open */
    if (s == FIOFNF) {                              /* File not found */
        mlwrite_one(MLbkt("No such file"));
        return FALSE;
    }
    mlwrite_one(MLbkt("Inserting file"));

    s = resetkey();
    if (s != TRUE) return s;

/* Back up a line and save the mark here.
 * We can only insert between lines, not into the middle of one.
 */
    curwp->w.dotp = lback(curwp->w.dotp);
    curwp->w.doto = 0;
    curwp->w.markp = curwp->w.dotp;
    curwp->w.marko = 0;

    s = file2buf(curwp->w.dotp, "Inserting", TRUE, TRUE);
    curwp->w.markp = lforw(curwp->w.markp);     /* Restore original mark */
    char *emg = "";
    if (s == FIOERR) {
        emg = "I/O ERROR, ";
        curbp->b_flag |= BFTRUNC;
    }
    char *dmg = "";
    if (dos_file) dmg = " - from DOS file!";
/* This doesn't need to set readin_mesg - which is only for start-up */
    mlwrite(MLbkt("%sInserted %d line%s%s"), emg, nlines,
        (nlines > 1)? "s": "", dmg);
    if (s == FIOERR) sleep(1);  /* Let it be seen */

out:
/* Advance to the next line and mark the window for changes */
    curwp->w.dotp = lforw(curwp->w.dotp);
    curwp->w_flag |= WFHARD | WFMODE;

/* Copy window parameters back to the buffer structure */
    curbp->b = curwp->w;

    if (s == FIOERR) return FALSE;      /* False if error. */
    return TRUE;
}

/* Insert a file into the current buffer.
 * This is really easy; all you do it find the name of the file and
 * call the standard "insert a file into the current buffer" code.
 * Bound to "C-X C-I".
 */
int insfile(int f, int n) {
    int s;
    char fname[NFILEN];

    if (restflag)           /* don't allow this command if restricted */
        return resterr();
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
        return rdonly();        /* we are in read only mode     */
    if ((s = mlreply("Insert file: ", fname, NFILEN, CMPLT_FILE)) != TRUE)
        return s;

/* If we are given a user arg of 2 then it means "replace the active
 * content of the buffer by the file contents".
 * So, if the buffer is narrowed just replace the narrowed data,
 * otherwise replace the whole buffer.
 */
    if (f && (n == 2)) {
        if (curbp->b_flag & BFNAROW) {
            gotobob(0, 0);
            curwp->w.markp = curwp->w.dotp;
            curwp->w.marko = curwp->w.doto;
            gotoeob(0, 0);
/* We do not want to save this kill, so tell killregion not to. */
            killregion(0, 2);
        }
        else if ((s = bclear(curbp)) != TRUE) return s; /* Might be old.  */
    }

    if ((s = ifile(fname)) != TRUE)
        return s;
    return reposition(TRUE, -1);
}

/* showdir_handled
 *  Check for the incoming pathname being a directory.
 *  If it is, and we have a "showdir" userproc, then let that handle it.
 *  Return TRUE if we passed it to showdir, otherwise FALSE.
 */
int showdir_handled(char *pname) {
    struct stat statbuf;
    char exp_pname[NFILEN];

/* Have to expand it *now* to allow for ~ usage in dir-check */

    strcpy(exp_pname, fixup_full(pname));   /* Make absolute pathname */
    int status = stat(exp_pname, &statbuf);
    if ((status == 0) && (statbuf.st_mode & S_IFMT) == S_IFDIR) {
/* We can only call showdir if it exists as a userproc.
 * If it doesn't we report that...
 */
        struct buffer *sdb = bfind("/showdir", FALSE, 0);
        if (sdb && (sdb->b_type == BTPROC)) {
            userproc_arg = exp_pname;
            (void)run_user_proc("showdir", 0, 1);
            userproc_arg = NULL;
        }
        else {
           mlwrite("No showdir userproc handler: %s", pname);
           if (comline_processing) sleep(2);    /* Let user see it */
        }
        status = TRUE;
    }
    else {
        status = FALSE;
    }
    return status;
}

/* Take a file name, and from it fabricate a buffer name.
 * The fabricated name is 4-chars short of the maximum, to allow
 * the unique-check code to append !nnn to make it unique
 */
void makename(char *bname, char *fname, int ensure_unique) {

/* First we have to step over any directory part in the name */

    int maxlen = strlen(fname);

/* We wish to copy as much of the fname into bname as we can such that
 * we have 4 free bytes (besides the terminating NUL) for unqname
 * to ensure uniqueness.
 * We need to find and dir separator first.
 */
    char *cp, *ocp;
    char *dsp = strrchr(fname, '/');
    if (dsp)  {
        cp = dsp + 1;
        maxlen -= (cp - fname);
    }
    else      cp = fname;
    ocp = cp;
    int bn_len = 0;
    int newlen = 0;
    while(newlen < maxlen) {
        newlen = next_utf8_offset(cp, bn_len, maxlen, 1);
        if (newlen < 0) break;              /* End of string */
        if (newlen > (NBUFN - 5)) break;    /* Out of space */
        bn_len = newlen;
    }

/* We can't have an empty buffer name (you wouldn't be able to specify
 * it in any command), so if it is empty (fname with a trailing /) change
 * it to " 0"
 */
    if (bn_len > 0) {
        memcpy(bname, ocp, bn_len);
        terminate_str(bname+bn_len);
    }
    else strcpy(bname, " 0");

/* All done if we're not checking uniqueness */

    if (!ensure_unique) return;

/* Check to see if it is in the buffer list */

    if (bfind(bname, 0, FALSE) == NULL) return;  /* OK - not there */

/* That name is already there, so append numbers until it is unique.
 * the code above has already left 4 bytes free for us so append !nnn
 * with nnn starting at 000 and increasing until unique.
 * If we still fail, well...
 * 999 rather than 1000 as otherwise gcc 8.4.0 on Arm64 complains
 * about possible excessive field-width.
 */
    char *tagp = bname + strlen(bname);
    for (unsigned int unum = 0; unum < 999; unum++) {
        snprintf(tagp, 5, "!%03u", unum);
/* Check to see if *this* one is in the buffer list */
        if (bfind(bname, 0, FALSE) == NULL) {   /* This is unique */
            return;
        }
    }
    mlforce("Unable to generate a unique buffer name for %s- exiting", bname);
    sleep(2);
    quickexit(FALSE, 0);
}

/* getfile()
 *
 * char fname[];        file name to find
 * int lockfl;          check the file for locks?
 */
int getfile(char *fname, int lockfl, int check_dir) {
    struct buffer *bp;
    struct line *lp;
    int i;
    int s;
    char bname[NBUFN];      /* buffer name to put file */

    fname = fixup_fname(fname);

/* Check *now* if this is a directory and we've been asked to check.
 * This prevents setting up an incomplete buffer that isn't used by
 * the showdir code.
 * If it isn't a directory, or we can't find out, just continue,
 */
    if ((check_dir == TRUE) && (showdir_handled(fname) == TRUE)) return TRUE;

/* Look for a buffer holding the same realpath, regardless of how the user
 * specified it.
 * NOTE!!! It is possible for >1 buffer to have the same b_rpname.
 *  e.g.,   open the file
 *          create a new, empty buffer.
 *          read the file into that buffer
 * We just use the first we come to....
 */
    char *testp = get_realpath(fname);
    int found = 0;
    int moved_to = 0;
    for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if (((bp->b_flag & BFINVS) == 0) &&
             (strcmp(bp->b_rpname, testp) == 0)) {
            found++;
            if (!moved_to && !swbuffer(bp, 0)) continue;
            moved_to = 1;
            lp = curwp->w.dotp;
            i = curwp->w_ntrows / 2;
            while (i-- && lback(lp) != curbp->b_linep) lp = lback(lp);
            curwp->w_linep = lp;
            curwp->w_flag |= WFMODE | WFHARD;
            cknewwindow();
        }
    }
    if (moved_to) {
        mlwrite_one((found == 1)? MLbkt("Old buffer"):
             MLbkt("Old buffer (from multiple choices"));
        return TRUE;
    }

    makename(bname, fname, FALSE); /* New buffer name. No unique check. */
    while ((bp = bfind(bname, FALSE, 0)) != NULL) {
/* Old buffer name conflict code */
        s = mlreply("Buffer name: ", bname, NBUFN, CMPLT_BUF);
        if (s == ABORT) return s;   /* ^G to just quit      */
        if (s == FALSE) {           /* CR to clobber it     */
            makename(bname, fname, FALSE);
            break;
        }
    }
    if (bp == NULL && (bp = bfind(bname, TRUE, 0)) == NULL) {
        mlwrite_one("Cannot create buffer");
        return FALSE;
    }
    if (--curbp->b_nwnd == 0) curbp->b = curwp->w;  /* Undisplay */

/* GGR - remember last buffer */
    if (!inmb) strcpy(savnam, curbp->b_bname);

    curbp = bp;                     /* Switch to it.        */
    curwp->w_bufp = bp;
    curbp->b_nwnd++;
    s = readin(fname, lockfl);      /* Read it in.          */
    cknewwindow();
    return s;
}

/* Select a file for editing.
 * Look around to see if you can find the file in another buffer; if you
 * can find it just switch to the buffer.
 * If you cannot find the file, create a new buffer, read in the text
 * and switch to the new buffer.
 * Bound to C-X C-F.
 */
int filefind(int f, int n) {
    UNUSED(f); UNUSED(n);
    char fname[NFILEN];     /* file user wishes to find */
    int s;                  /* status return */

    if (restflag)           /* don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("Find file: ", fname, NFILEN, CMPLT_FILE)) != TRUE) {
        return s;
    }
    run_filehooks = 1;      /* set flag */
    return getfile(fname, TRUE, TRUE);
}

int viewfile(int f, int n) {    /* Visit a file in VIEW mode */
    UNUSED(f); UNUSED(n);
    char fname[NFILEN];         /* File user wishes to find */
    int s;                      /* Status return */
    struct window *wp;          /* Scan for windows that need updating */

    if (restflag)               /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("View file: ", fname, NFILEN, CMPLT_FILE)) != TRUE)
        return s;
    run_filehooks = 1;          /* Set flag */
    s = getfile(fname, FALSE, TRUE);
    if (s) {                    /* If we succeed, put it in view mode */
        curwp->w_bufp->b_mode |= MDVIEW;

/* Scan through and update mode lines of all windows */
        wp = wheadp;
        while (wp != NULL) {
            wp->w_flag |= WFMODE;
            wp = wp->w_wndp;
        }
    }
    return s;
}

/* This function performs the details of file writing.
 * Uses the file management routines in the "fileio.c" package.
 * The number of lines written is displayed.
 * Most of the grief is error checking of some sort.
 */
int writeout(char *fn) {
    int s;
    struct line *lp;
    int nline;

    s = resetkey();
    if (s != TRUE) return s;

    if ((s = ffwopen(fn)) != FIOSUC) return FALSE;  /* Open writes message */

    mlwrite_one(MLbkt("Writing..."));       /* tell us were writing */
    lp = lforw(curbp->b_linep);             /* First line.          */
    nline = 0;                              /* Number of lines.     */
    while (lp != curbp->b_linep) {
        if ((s = ffputline(ltext(lp), lused(lp))) != FIOSUC) break;
        ++nline;
        if (!(nline % 300) && !silent)      /* GGR */
            mlwrite(MLbkt("Writing...") " : %d lines",nline);
        lp = lforw(lp);
    }
    if (s == FIOSUC) {                      /* No write error.      */
        ffputline(NULL, 0);                 /* Must flush write cache!! */
        s = ffclose();
        if (s == FIOSUC) {                  /* No close error.      */
            mlwrite(MLbkt("Wrote %d line%s"), nline, (nline == 1)? "": "s");
        }
    } else                                  /* Ignore close error   */
        ffclose();                          /* if a write error.    */
    if (s != FIOSUC) return FALSE;          /* Some sort of error.  */
    return TRUE;
}

/* Ask for a file name, and write the contents of the current buffer
 * to that file.
 * Update the remembered file name and clear the buffer changed flag.
 * This handling of file names is different from the earlier versions and
 * is more compatible with Gosling EMACS than with ITS EMACS.
 * Bound to "C-X C-W".
 */
int filewrite(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;
    int s;
    char fname[NFILEN];

    if (restflag)           /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("Write file: ", fname, NFILEN, CMPLT_FILE)) != TRUE)
        return s;
    if ((s = writeout(fname)) == TRUE) {
        set_buffer_filenames(curbp, fname);
        curbp->b_flag &= ~BFCHG;
        wp = wheadp;        /* Update mode lines.   */
        while (wp != NULL) {
            if (wp->w_bufp == curbp) wp->w_flag |= WFMODE;
            wp = wp->w_wndp;
        }
    }
    return s;
}

/* Save the contents of the current buffer in its associated file.
 * Do nothing if nothing has changed (this may be a bug, not a
 * feature) - unless a numeric arg was given.
 * Error if there is no remembered file name for the buffer.
 * Bound to "C-X C-S". May get called by "C-Z".
 */
int filesave(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;
    int s;

    if (curbp->b_mode & MDVIEW) /* Don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if (!f && (curbp->b_flag & BFCHG) == 0) /* Return, no changes.  */
        return TRUE;
    if (*(curbp->b_fname) == 0) {   /* Must have a name. */
        mlwrite_one("No file name");
        return FALSE;
    }

/* Complain about truncated files */
    if ((curbp->b_flag & BFTRUNC) != 0) {
        if (mlyesno("Truncated file ... write it out") == FALSE) {
            mlwrite_one(MLbkt("Aborted"));
            return FALSE;
        }
    }

/* Complain about narrowed buffers */
    if ((curbp->b_flag&BFNAROW) != 0) {
        if (mlyesno("Narrowed buffer: write it out anyway") != TRUE) {
            mlwrite_one(MLbkt("Aborted"));
            return(FALSE);
        }
    }

    if ((s = writeout(curbp->b_fname)) == TRUE) {
        curbp->b_flag &= ~BFCHG;
        wp = wheadp;            /* Update mode lines. */
        while (wp != NULL) {
            if (wp->w_bufp == curbp) wp->w_flag |= WFMODE;
            wp = wp->w_wndp;
        }
    }
    return s;
}

/* The command allows the user to modify the file name associated with
 * the current buffer.
 * It is like the "f" command in UNIX "ed".
 * The operation is simple; just zap the name in the buffer structure
 * and mark the windows as needing an update.
 * You can type a blank line at the prompt if you wish.
 */
int filename(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;
    char fname[NFILEN];

    if (restflag)           /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("Name: ", fname, NFILEN, CMPLT_FILE)) == ABORT)
        return s;
    set_buffer_filenames(curbp, (s == FALSE)? "": fname);

    for (struct window *wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_bufp == curbp) wp->w_flag |= WFMODE;
    }
    curbp->b_mode &= ~MDVIEW;   /* no longer read only mode */
    return TRUE;
}
