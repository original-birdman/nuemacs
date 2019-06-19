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

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Max number of lines from one file. */
#define MAXNLINE 10000000

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
    s = mlreply("Read file: ", fname, NFILEN);
    if (s == ABORT) return(s);
    else if (s == FALSE) {
        if (strlen(curbp->b_fname) == 0) return(s);
        else strcpy(fname, curbp->b_fname);
    }
    return readin(fname, TRUE);
}

static int resetkey(void) { /* Reset the encryption key if needed */
    int s;              /* return status */

    cryptflag = FALSE;  /* Turn off the encryption flag */

    if (curbp->b_mode & MDCRYPT) {  /* If we are in crypt mode */
        if (curbp->b_keylen == 0) {
            s = set_encryption_key(FALSE, 0);
            if (s != TRUE) return s;
        }
        cryptflag = TRUE;           /* let others know... */

/* Set up the key to be used! */
        myencrypt(NULL, 0);
        myencrypt(curbp->b_key, curbp->b_keylen);

/* curbp->b_key is encrypted when you set it, and since this is done in
 * place we've just decrypted it. So we re-encrypt it.
 * Since this uses a symmetric cipher the running key ends up the same
 * Taking a copy would mean we could do only one set of encrypting, but
 * would then leave us with the decrypted key in memory, and we're trying
 * to avoid that...
 */
        myencrypt(NULL, 0);
        myencrypt(curbp->b_key, curbp->b_keylen);
    }
    return TRUE;
}

/* Insert file "fname" into the current buffer.
 * Called by insert file command.
 * Return the final status of the read.
 */
static int ifile(char *fname) {
    struct line *lp0;
    struct line *lp1;
    struct line *lp2;
    struct buffer *bp;
    int s;
    int nline;

    bp = curbp;             /* Cheap.               */
    bp->b_flag |= BFCHG;    /* we have changed      */
    bp->b_flag &= ~BFINVS;  /* and are not temporary */

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

    pathexpand = FALSE;     /* GGR */

    if ((s = ffropen(fname)) == FIOERR) goto out;   /* Hard file open */
    if (s == FIOFNF) {                              /* File not found */
        mlwrite(MLpre "No such file" MLpost);
        return FALSE;
    }
    mlwrite(MLpre "Inserting file" MLpost);

    s = resetkey();
    if (s != TRUE) return s;

/* Back up a line and save the mark here */
    curwp->w_dotp = lback(curwp->w_dotp);
    curwp->w_doto = 0;
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = 0;

    nline = 0;
    int dos_include = 0;
    while ((s = ffgetline()) == FIOSUC) {
        lp1 = fline;            /* Allocate by ffgetline..*/
        lp0 = curwp->w_dotp;    /* line previous to insert */
        lp2 = lp0->l_fp;        /* line after insert */

/* Re-link new line between lp0 and lp2 */
        lp2->l_bp = lp1;
        lp0->l_fp = lp1;
        lp1->l_bp = lp0;
        lp1->l_fp = lp2;

/* And advance and write out the current line */
        curwp->w_dotp = lp1;
        ++nline;

/* Check for a DOS line ending on line 1 */
        if (nline == 1 && lp1->l_text[lp1->l_used-1] == '\r')
            dos_include = 1;
        if (dos_include && lp1->l_text[lp1->l_used-1] == '\r') {
             lp1->l_used--;             /* Remove the trailing CR */
        }

        if (!(nline % 300) && !silent)      /* GGR */
             mlwrite(MLpre "Inserting file" MLpost " : %d lines", nline);

    }
    ffclose();              /* Ignore errors. */
    curwp->w_markp = lforw(curwp->w_markp);
    strcpy(readin_mesg, MLpre);
    if (s == FIOERR) {
        strcat(readin_mesg, "I/O ERROR, ");
        curbp->b_flag |= BFTRUNC;
    }
    if (s == FIOMEM) {
        strcat(readin_mesg, "OUT OF MEMORY, ");
        curbp->b_flag |= BFTRUNC;
    }
    sprintf(&readin_mesg[strlen(readin_mesg)], "Inserted %d line", nline);
    if (nline > 1) strcat(readin_mesg, "s");
    if (dos_include) strcat(readin_mesg, " - from DOS file!");
    strcat(readin_mesg, MLpost);
    mlwrite(readin_mesg);

out:
/* Advance to the next line and mark the window for changes */
    curwp->w_dotp = lforw(curwp->w_dotp);
    curwp->w_flag |= WFHARD | WFMODE;

/* Copy window parameters back to the buffer structure */
    curbp->b_dotp = curwp->w_dotp;
    curbp->b_doto = curwp->w_doto;
    curbp->b_markp = curwp->w_markp;
    curbp->b_marko = curwp->w_marko;
    curbp->b_fcol = curwp->w_fcol;

    if (s == FIOERR) return FALSE;      /* False if error. */
    return TRUE;
}

/* Insert a file into the current buffer.
 * This is really easy; all you do it find the name of the file and
 * call the standard "insert a file into the current buffer" code.
 * Bound to "C-X C-I".
 */
int insfile(int f, int n) {
    UNUSED(f); UNUSED(n);
    int s;
    char fname[NFILEN];

    if (restflag)           /* don't allow this command if restricted */
        return resterr();
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if      */
        return rdonly();        /* we are in read only mode     */
    if ((s = mlreply("Insert file: ", fname, NFILEN)) != TRUE)
        return s;
    if ((s = ifile(fname)) != TRUE)
        return s;
    return reposition(TRUE, -1);
}

/*
 * Select a file for editing.
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
    if ((s = mlreply("Find file: ", fname, NFILEN)) != TRUE) {
        return s;
    }
    run_filehooks = 1;      /* set flag */
    return getfile(fname, TRUE);
}

int viewfile(int f, int n) {    /* Visit a file in VIEW mode */
    UNUSED(f); UNUSED(n);
    char fname[NFILEN];         /* File user wishes to find */
    int s;                      /* Status return */
    struct window *wp;          /* Scan for windows that need updating */

    if (restflag)               /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("View file: ", fname, NFILEN)) != TRUE)
        return s;
    run_filehooks = 1;          /* Set flag */
    s = getfile(fname, FALSE);
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

/*
 * getfile()
 *
 * char fname[];        file name to find
 * int lockfl;          check the file for locks?
 */
int getfile(char *fname, int lockfl) {
    struct buffer *bp;
    struct line *lp;
    int i;
    int s;
    char bname[NBUFN];      /* buffer name to put file */
    for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if ((bp->b_flag & BFINVS) == 0 && strcmp(bp->b_fname, fname) == 0) {
            if (!swbuffer(bp, 0)) return FALSE;
            lp = curwp->w_dotp;
            i = curwp->w_ntrows / 2;
            while (i-- && lback(lp) != curbp->b_linep) lp = lback(lp);
            curwp->w_linep = lp;
            curwp->w_flag |= WFMODE | WFHARD;
            cknewwindow();
            mlwrite(MLpre "Old buffer" MLpost);
            return TRUE;
        }
    }
    makename(bname, fname); /* New buffer name.     */
    while ((bp = bfind(bname, FALSE, 0)) != NULL) {
/* Old buffer name conflict code */
        s = mlreply("Buffer name: ", bname, NBUFN);
        if (s == ABORT) return s;   /* ^G to just quit      */
        if (s == FALSE) {           /* CR to clobber it     */
            makename(bname, fname);
            break;
        }
    }
    if (bp == NULL && (bp = bfind(bname, TRUE, 0)) == NULL) {
        mlwrite("Cannot create buffer");
        return FALSE;
    }
    if (--curbp->b_nwnd == 0) {     /* Undisplay.           */
        curbp->b_dotp = curwp->w_dotp;
        curbp->b_doto = curwp->w_doto;
        curbp->b_markp = curwp->w_markp;
        curbp->b_marko = curwp->w_marko;
        curbp->b_fcol = curwp->w_fcol;
    }
/* GGR - remember last buffer */
    if (!inmb) strcpy(savnam, curbp->b_bname);

    curbp = bp;                     /* Switch to it.        */
    curwp->w_bufp = bp;
    curbp->b_nwnd++;
    s = readin(fname, lockfl);      /* Read it in.          */
    cknewwindow();
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
    if (sfx && strlen(sfx) <= 19) {     /* Max bufname is 32, incl NUL */
        sfx++;                          /* Skip over '.' */
        char sfx_bname[NBUFN];
        strcpy(sfx_bname, "/file-hooks-");
        strcat(sfx_bname, sfx);
        if ((sb = bfind(sfx_bname, FALSE, 0)) != NULL) dobuf(sb);
    }
    return;
}

/* Read file "fname" into the current buffer, blowing away any text
 * found there - called by both the read and find commands.
 * Return the final status of the read.
 * Also called by the main routine to read in a file specified on the
 * command line as an argument.
 * The command bound to M-FNR is called after the buffer is set up
 * and before it is read.
 *
 * char fname[];        name of file to read
 * int lockfl;          check for file locks?
 */
int readin(char *fname, int lockfl) {
    struct line *lp1;
    struct line *lp2;
    struct window *wp;
    struct buffer *bp;
    int s;
    int nline;

#if FILOCK && (BSD || SVR4)
    if (lockfl && lockchk(fname) == ABORT) {
        s = FIOFNF;
        bp = curbp;
        strcpy(bp->b_fname, "");
        goto out;
    }
#else
        UNUSED(lockfl);
#endif
    bp = curbp;                             /* Cheap.        */
    if ((s = bclear(bp)) != TRUE) return s; /* Might be old. */
    bp->b_flag &= ~(BFINVS | BFCHG);

/* If this is a translation table, remove any compiled data */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

/* If activating an inactive buffer, these may be the same and the
 * action of strcpy() is undefined for overlapping strings.
 * On a Mac it will crash...
 */
    if (bp->b_fname != fname) strcpy(bp->b_fname, fname);

/* GGR - run filehooks on this iff the caller sets the flag */
    if (run_filehooks) handle_filehooks(fname);

/* Do this after file-hooks run, so they can set CRYPT mode */
    s = resetkey();
    if (s != TRUE) return s;

/* let a user macro get hold of things...if he wants */
    execute(META | SPEC | 'R', FALSE, 1);

    if ((s = ffropen(fname)) == FIOERR) goto out;   /* Hard file open. */
    if (s == FIOFNF) {                              /* File not found. */
        strcpy(readin_mesg, MLpre "New file" MLpost);
        mlwrite(readin_mesg);
        goto out;
    }

/* Read the file in */
    if (!silent) mlwrite(MLpre "Reading file" MLpost);  /* GGR */
    nline = 0;
    while ((s = ffgetline()) == FIOSUC) {
        if (nline > MAXNLINE) {
            s = FIOMEM;
            break;
        }
        lp1 = fline;                    /* Allocated by ffgetline() */
        lp2 = lback(curbp->b_linep);
        lp2->l_fp = lp1;
        lp1->l_fp = curbp->b_linep;
        lp1->l_bp = lp2;
        curbp->b_linep->l_bp = lp1;
        ++nline;
/* Check for a DOS line ending on line 1?
 * Only if the check is enabled...
 * NOTE!! that we set/unset this flag every time we read line 1,
 * so a file can be re-read with the required autodos setting.
 */
        if (nline == 1) {
            if (autodos && lp1->l_text[lp1->l_used-1] == '\r')
                curbp->b_mode |= MDDOSLE;
            else
                curbp->b_mode &= ~MDDOSLE;
        }
        if ((curbp->b_mode & MDDOSLE) != 0 &&
             lp1->l_text[lp1->l_used-1] == '\r') {
             lp1->l_used--;             /* Remove the trailing CR */
        }
        if (!(nline % 300) && !silent)  /* GGR */
            mlwrite(MLpre "Reading file" MLpost " : %d lines", nline);
    }
    ffclose();                          /* Ignore errors. */
    strcpy(readin_mesg, MLpre);
    if (s == FIOERR) {
        strcat(readin_mesg, "I/O ERROR, ");
        curbp->b_flag |= BFTRUNC;
    }
    if (s == FIOMEM) {
        strcat(readin_mesg, "OUT OF MEMORY, ");
        curbp->b_flag |= BFTRUNC;
    }
    if (!silent) {                      /* GGR */
        sprintf(&readin_mesg[strlen(readin_mesg)], "Read %d line", nline);
        if (nline != 1) strcat(readin_mesg, "s");
        if (curbp->b_mode & MDDOSLE)
            strcat(readin_mesg, " - DOS mode enabled!");
        strcat(readin_mesg, MLpost);
        mlwrite(readin_mesg);
        if (s == FIOERR || s == FIOMEM) sleep(1);   /* Let it be seen */
    }

out:
    for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_bufp == curbp) {
            wp->w_linep = lforw(curbp->b_linep);
            wp->w_dotp = lforw(curbp->b_linep);
            wp->w_doto = 0;
            wp->w_markp = NULL;
            wp->w_marko = 0;
            wp->w_flag |= WFMODE | WFHARD;
        }
    }
    if (s == FIOERR || s == FIOFNF) return FALSE;   /* False if error. */
    return TRUE;
}

/* Take a file name, and from it fabricate a buffer name.
 * The fabricated name is 4-chars short of the maximum, to allow
 * unqname to append nnnn to make it unique
 */
void makename(char *bname, char *fname) {

/* First we have to step over any directory part in the name */

    unsigned int clen = 0;
    unsigned int fn4bn_start = 0;
    unsigned int maxlen = strlen(fname);
    unicode_t uc;
    while (1) {
        unsigned int nb = utf8_to_unicode(fname, clen, maxlen, &uc);
        if (nb == 0) break;
        clen += nb;
        if (uc == '/')
            fn4bn_start = clen;
    }

/* We wish to copy as much of the fname into bname as we can such that
 * we have 4 free bytes (besides the terminating NUL) for unqname
 * to ensure uniqueness.
 */
    clen = fn4bn_start;
    while(1) {
        unsigned int newlen = next_utf8_offset(fname, clen, maxlen, 1);
        if (newlen == clen) break;                          /* End of string */
        if ((newlen - fn4bn_start) > (NBUFN - 5)) break;    /* Out of space */
        if (newlen > maxlen) break;                         /* ??? */
        clen = newlen;
    }

/* We can't have an empty buffer name (you couldn't specify it in any
 * command, so if it is empty (fname with a trailing /) change it to " 0"
 */
    if (clen > fn4bn_start) {
        memcpy(bname, fname + fn4bn_start, clen - fn4bn_start);
        *(bname+clen-fn4bn_start) = '\0';
    }
    else strcpy(bname, " 0");
}

/* Make sure a buffer name is unique by, if necessary, appending
 * an up-to 4 digit number.
 * makename() will have left a 4-char space for this.
 *
 * char *name;          name to check on
 */
static int power10(int exp) {
    int res = 1;
    while (exp-- > 0) res *= 10;
    return res;
}

void unqname(char *name) {
    char testname[NBUFN];
/* Check to see if it is in the buffer list */
    if (bfind(name, 0, FALSE) == NULL) return;  /* OK - not there */

/* That name is already there, so append numbers until it is unique */

    int namelen = strlen(name);
    strcpy(testname, name);
    for (int numlen = 1; namelen + numlen < NBUFN; numlen++) {
        int maxnum = power10(numlen);
        for (int unum = 0; unum < maxnum; unum++) {
            sprintf(testname + namelen, "%0*d", numlen, unum);
/* Check to see if *this* one in the buffer list */
            if (bfind(testname, 0, FALSE) == NULL) {    /* This is unique */
                strcpy(name, testname);
                return;
            }
        }
    }
    mlforce("Unable to generate a unique buffer name - exiting");
    sleep(2);
    quickexit(FALSE, 0);
}

/* Ask for a file name, and write the contents of the current buffer
 * to that file.
 * Update the remembered file name and clear the buffer changed flag.
 * This handling of file names is different from the earlier versions and
 * is more compatable with Gosling EMACS than with ITS EMACS.
 * Bound to "C-X C-W".
 */
int filewrite(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;
    int s;
    char fname[NFILEN];

    if (restflag)           /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("Write file: ", fname, NFILEN)) != TRUE)
        return s;
    if ((s = writeout(fname)) == TRUE) {
        strcpy(curbp->b_fname, fname);
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
 * feature).
 * Error if there is no remembered file name for the buffer.
 * Bound to "C-X C-S". May get called by "C-Z".
 */
int filesave(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct window *wp;
    int s;

    if (curbp->b_mode & MDVIEW) /* Don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if ((curbp->b_flag & BFCHG) == 0)   /* Return, no changes.  */
        return TRUE;
    if (curbp->b_fname[0] == 0) {   /* Must have a name. */
        mlwrite("No file name");
        return FALSE;
    }

/* Complain about truncated files */
    if ((curbp->b_flag & BFTRUNC) != 0) {
        if (mlyesno("Truncated file ... write it out") == FALSE) {
            mlwrite(MLpre "Aborted" MLpost);
            return FALSE;
        }
    }

/* Complain about narrowed buffers */
    if ((curbp->b_flag&BFNAROW) != 0) {
        if (mlyesno("Narrowed buffer: write it out anyway") != TRUE) {
            mlwrite(MLpre "Aborted" MLpost);
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

    mlwrite(MLpre "Writing..." MLpost);     /* tell us were writing */
    lp = lforw(curbp->b_linep);             /* First line.          */
    nline = 0;                              /* Number of lines.     */
    while (lp != curbp->b_linep) {
        if ((s = ffputline(lp->l_text, llength(lp))) != FIOSUC) break;
        ++nline;
        if (!(nline % 300) && !silent)      /* GGR */
            mlwrite(MLpre "Writing..." MLpost " : %d lines",nline);
        lp = lforw(lp);
    }
    if (s == FIOSUC) {                      /* No write error.      */
        ffputline(NULL, 0);                 /* Must flush write cache!! */
        s = ffclose();
        if (s == FIOSUC) {                  /* No close error.      */
            if (nline == 1)
                mlwrite(MLpre "Wrote 1 line" MLpost);
            else
                mlwrite(MLpre "Wrote %d lines" MLpost, nline);
            }
    } else                                  /* Ignore close error   */
        ffclose();                          /* if a write error.    */
    if (s != FIOSUC) return FALSE;          /* Some sort of error.  */
    return TRUE;
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
    struct window *wp;
    int s;
    char fname[NFILEN];

    if (restflag)           /* Don't allow this command if restricted */
        return resterr();
    if ((s = mlreply("Name: ", fname, NFILEN)) == ABORT)
        return s;
    if (s == FALSE)
        strcpy(curbp->b_fname, "");
    else
        strcpy(curbp->b_fname, fname);
    wp = wheadp;            /* Update mode lines.   */
    while (wp != NULL) {
        if (wp->w_bufp == curbp) wp->w_flag |= WFMODE;
        wp = wp->w_wndp;
    }
    curbp->b_mode &= ~MDVIEW;   /* no longer read only mode */
    return TRUE;
}
