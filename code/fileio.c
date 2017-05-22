/*      FILEIO.C
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

static FILE *ffp;                       /* File pointer, all functions. */
static int eofflag;                     /* end-of-file flag */

/* The cache.
 * Used for reading and writing as only one can be active at any one time.
 */
#define CSIZE 8192
static struct {
    char buf[CSIZE];
    int rst;                /* Read pointer */
    int len;                /* Valid tot chars (write) or past rst (read) */
} cache;

/*
 * Open a file for reading.
 */
int ffropen(char *fn) {
#if (BSD | USG)
#if EXPAND_TILDE
    expand_tilde(fn);
#endif
#if EXPAND_SHELL
    expand_shell(fn);
#endif
#endif
    if ((ffp = fopen(fn, "r")) == NULL) return FIOFNF;
    if (pathexpand) {       /* GGR */
/* If activating an inactive buffer, these may be the same and the
 * action of strcpy() is undefined for overlapping strings.
 * On a Mac it will crash...
 */
        if (curbp->b_fname != fn) strcpy(curbp->b_fname, fn);
    }

    cache.rst = cache.len = 0;
    fline = NULL;

    eofflag = FALSE;
    return FIOSUC;
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
int ffwopen(char *fn) {
#if (BSD | USG)
#if EXPAND_TILDE
    expand_tilde(fn);
#endif
#if EXPAND_SHELL
    expand_shell(fn);
#endif
#endif
    if ((ffp = fopen(fn, "w")) == NULL) {
        mlwrite("Cannot open file for writing");
        return FIOERR;
    }
    if (pathexpand) {       /* GGR */
/* If activating an inactive buffer, these may be the same and the
 * action of strcpy() is undefined for overlapping strings.
 * On a Mac it will crash...
 */
        if (curbp->b_fname != fn) strcpy(curbp->b_fname, fn);
    }
    cache.rst = cache.len = 0;

    return FIOSUC;
}

/*
 * Close a file. Should look at the status in all systems.
 */
int ffclose(void) {

    fline = NULL;
    eofflag = FALSE;

#if MSDOS & CTRLZ
    fputc(26, ffp);         /* add a ^Z at the end of the file */
#endif

#if V7 | USG | BSD | (MSDOS & (MSC | TURBO))
    if (fclose(ffp) != FALSE) {
        mlwrite("Error closing file");
        return FIOERR;
    }
#else
    fclose(ffp);
#endif
    return FIOSUC;
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Now done using a write-cache, so our caller needs to do a final
 * ffputline() call with a NULL buf arg to flush the last part of the cache.
 */

/* Routine to flush the cache */
static int flush_write_cache(void) {
#if CRYPT
    if (cryptflag) myencrypt(cache.buf, cache.len);
#endif
    fwrite(cache.buf, sizeof(*cache.buf), cache.len, ffp);
    cache.len = 0;
    if (ferror(ffp)) {
        mlwrite("Write I/O error");
        return FIOERR;
    }
    return FIOSUC;
}

/* The actual callable function */
int ffputline(char *buf, int nbuf) {

    static int doing_newline = 0;
    if (buf == NULL) {      /* Just flush what is left... */
        return flush_write_cache();
    }

    while(nbuf > 0) {
        int to_fill;
        if ((cache.len + nbuf) <= CSIZE) to_fill = nbuf;
        else to_fill = CSIZE - cache.len;
        memcpy(cache.buf+cache.len, buf, to_fill);
        nbuf -= to_fill;        /* bytes left */
        cache.len += to_fill;  /* valid in cache */
        buf += to_fill;         /* new start of input */
        if (nbuf > 0) {         /* More to go, so flush cache */
            int status = flush_write_cache();
            if (status != FIOSUC) return status;
        }
    }

/* Now add the newline, which we won't have been sent
 * It's convenient to do this via a recursive call, provided we
 * note that we are doing it, so don't continue to do so...
 */
    int status = FIOSUC;
    if (!doing_newline) {
        doing_newline = 1;
        status = ffputline("\n", 1);
        doing_newline = 0;
    }
    return status;
}

/*
 * Read a line from a file, and store the bytes into a global, malloc()ed
 * buffer (fline).
 * Complain about lines at the end of the file that don't have a newline.
 * Check for I/O errors too. Return status.
 */

/* Helper routine to add text into fline, re-allocating it if required */
static int add_to_fline(char *buf, int len) {

    int newlen = -1;
    if (fline == NULL) {
        newlen = len;
    }
    else if (len >= (fline->l_size - fline->l_used)) {
        newlen = len + fline->l_used;
    }
    if (newlen >= 0) {      /* Need a new/bigger fline */
        struct line *nl = lalloc(newlen);
        if (nl == NULL) return FALSE;
        if (fline) {
            nl->l_used = fline->l_used;
            memcpy(nl->l_text, fline->l_text, fline->l_used);
            free(fline);
        }
        else
            nl->l_used = 0; /* Need to set to how much is in there *now* */
        fline = nl;
    }
    memcpy(fline->l_text+fline->l_used, cache.buf+cache.rst, len);
    fline->l_used += len;   /* Record the real size of the line */
    cache.rst += len;       /* Advance cache read-pointer */
    cache.len -= len;       /* Decrement left-to-read counter */
    return TRUE;
}

/* The actual callable function */
int ffgetline(void) {

    fline = NULL;       /* Start afresh */

/* Do we have a newline in our cache */

    char *nlp = NULL;
    while (!nlp) {
        nlp = memchr(cache.buf+cache.rst, '\n', cache.len);
        if (!nlp) {
            if (!add_to_fline(cache.buf+cache.rst, cache.len))
                return FIOMEM;      /* Only reason for failure */
            cache.len = fread(cache.buf, sizeof(*cache.buf),
                sizeof(cache.buf), ffp);
            cache.rst = 0;
/* If we are at the end...return it.
 * But - if we still have cached data then there was no final
 * newline, but we still have to return that, and warn about the
 * missing newline.
 */
            if (eofflag && (cache.len == 0)) {
                if (fline->l_used) {
                    mlforce("Newline absent from end of file. Added.");
                    sleep(1);
                }
                return fline->l_used? FIOSUC: FIOEOF;
            }
            if (cache.len != sizeof(cache.buf)) { /* short read - why? */
                if (feof(ffp)) {
                    eofflag = TRUE;
                }
                else {
                    mlwrite("File read error");
                    return FIOERR;
                }
            }
#if CRYPT
            if (cryptflag) myencrypt(cache.buf, cache.len);
#endif
        }
    }
    int cc = nlp - (cache.buf+cache.rst);
    if (!add_to_fline(cache.buf+cache.rst, cc)) return FIOMEM;
    cache.rst++;        /* Step over newline */
    cache.len--;        /* Step over newline */

    return FIOSUC;
}

/*
 * does <fname> exist on disk?
 *
 * char *fname;         file to check for existance
 */
int fexist(char *fname) {
    FILE *fp;
#if (BSD | USG)
#if EXPAND_TILDE
    expand_tilde(fname);
#endif
#if EXPAND_SHELL
    expand_shell(fname);
#endif
#endif

/* Try to open the file for reading */
    fp = fopen(fname, "r");

/* If it fails, just return false! */
    if (fp == NULL) return FALSE;

/* Otherwise, close it and report true */
    fclose(fp);
    return TRUE;
}

#if BSD | USG

#if EXPAND_SHELL
void expand_shell(char *fn) {
    char *ptr;
    char command[NFILEN];
    int c;
    FILE *pipe;

    if (strchr(fn, '$') == NULL)    /* Don't bother if no $s */
        return;
    strcpy(command, "echo ");  /* Not all systems have -n, sadly */
    strcat(command, fn);
    if ((pipe = popen(command, "r")) == NULL)
        return;
    ptr = fn;
    while ((c = getc(pipe)) != EOF && c != 012)
        *ptr++ = c;
    *ptr = 0;
    pclose(pipe);
    return;
}
#endif

#if EXPAND_TILDE
#include <stdlib.h>
#include <pwd.h>
void expand_tilde(char *fn) {
    char tilde_copy[NFILEN];
    char *p, *q;
    short i;
    struct passwd *pwptr;

    if (fn[0]=='~') {
        if (fn[1]=='/' || fn[1]==0) {
            if ((p = getenv("HOME")) != NULL) {
                strcpy(tilde_copy, fn);
                strcpy(fn , p);
                i = 1;
/* Special case for root (i.e just "/")! */
                if (fn[0]=='/' && fn[1]==0 && tilde_copy[1] != 0)
                    i++;
                strcat(fn, tilde_copy+i);
            }
        }
        else {
            p = fn + 1;
            q = tilde_copy;
            while (*p != 0 && *p != '/')
                *q++ = *p++;
            *q = 0;
            if ((pwptr = getpwnam(tilde_copy)) != NULL) {
                strcpy(tilde_copy, pwptr->pw_dir);
                strcat(tilde_copy, p);
                strcpy(fn, tilde_copy);
            }
        }
    }
    return;
}
#endif

#endif
