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

#include "utf8proc.h"

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
 * Save the first FILE_START_LEN bytes of each file written for
 * binary/text checking (excludes any newlines).
 */
#define FILE_START_LEN 100
static struct {
    int vlen;
    char buf[FILE_START_LEN];
} file_start;

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
    curbp->b_EOLmissing = 0;        /* Unset this on open */
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
    file_start.vlen = 0;

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
 *
 * NOTE!!!
 * We wish to handle files which didn't have a newline at the EOF in a
 * helpful manner.
 * If the file actually had a newlien tehre we have no problem - we just
 * put one there while writing out.
 * If it didn;t have one then the b_EOLmissing flags will have been set.
 * There are three possibly cases:
 *
 *  1. We have a text file.
 *     => We want to add a newline.
 *  2. We have a CRYPT file .  i.e. we read in a CRYPT file ot a normal
 *     buffer, switched on CRYPT and are now writing it back out again
 *     (as real text). the final newline here is not real, and if we
 *     write it out then it will get decrypted to an arbitrary character
 *     => We do not want to add a newline.
 *  3. We have an arbitrary binary file. (Let's leave aside why you would be
 *     editing this in uemacs.)
 *     > We want to add a newline.
 *
 * We can detect 2. by checking for cryptflag && curbp->b_EOLmissing.
 * We also need to code make a heuristic check between a text and binary file.
 */

int file_is_binary(void) {

    int bi = 0;
    int uc_total = 0, uc_text = 0;
    while (bi < file_start.vlen) {
        unicode_t uc;
        unsigned int ub =
             utf8_to_unicode(file_start.buf, bi, file_start.vlen, &uc);
        if (ub <= 0) break;
        bi += ub;
        const utf8proc_property_t *utf8prpty = utf8proc_get_property(uc);
        uc_total++;
/* Count the "text" characters */
        switch(utf8prpty->category) {
        case UTF8PROC_CATEGORY_LU:      /* Letter, uppercase */
        case UTF8PROC_CATEGORY_LL:      /* Letter, lowercase */
        case UTF8PROC_CATEGORY_LT:      /* Letter, titlecase */
        case UTF8PROC_CATEGORY_LM:      /* Letter, modifier */
        case UTF8PROC_CATEGORY_LO:      /* Letter, other */
        case UTF8PROC_CATEGORY_ND:      /* Number, decimal digit */
        case UTF8PROC_CATEGORY_NL:      /* Number, letter */
        case UTF8PROC_CATEGORY_NO:      /* Number, other */
        case UTF8PROC_CATEGORY_PC:      /* Punctuation, connector */
        case UTF8PROC_CATEGORY_PD:      /* Punctuation, dash */
        case UTF8PROC_CATEGORY_PS:      /* Punctuation, open */
        case UTF8PROC_CATEGORY_PE:      /* Punctuation, close */
        case UTF8PROC_CATEGORY_PI:      /* Punctuation, initial quote */
        case UTF8PROC_CATEGORY_PF:      /* Punctuation, final quote */
        case UTF8PROC_CATEGORY_PO:      /* Punctuation, other */
        case UTF8PROC_CATEGORY_ZS:      /* Separator, space */
            uc_text++;
        }
    }
/* We'll say it's binary if <80% of the unicode characters are text */
    return (uc_text < (4*uc_total)/5);
}

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

/* The actual callable function.
 * We don't know whether to add the newline at the EOF until we get there
 * and the only time we know that is when we are called with a NULL
 * buffer to do the final flush.
 * So what we do is out put a newline *before* the line text we have
 * been sent and use the otherwise unused cache.rst variable to note
 * that we've seen the first line.
 */
int ffputline(char *buf, int nbuf) {

    static int doing_newline = 0;
    int status;

/* Now add the newline, which we won't have been sent
 * It's convenient to do this via a recursive call, provided we
 * note that we are doing it, so don't continue to do so...
 */
    if (buf == NULL) {      /* Just flush what is left... */
        char *reason = NULL;
        if (curbp->b_EOLmissing) {
            if (cryptflag) reason = "crypt";
            else if (file_is_binary()) reason = "binary";
        }
        if (reason) {       /* Do we have a reason to skip the final NL? */
            mlforce("Removed \"added\" trailing newline for %s file", reason);
            sleep(1);
        }
        else {
            if (cache.rst != 0 && !doing_newline) {
                doing_newline = 1;
                status = ffputline("\n", 1);
                doing_newline = 0;
                if (status != FIOSUC) return status;
            }
        }
        return flush_write_cache();
    }

/* Save the first FILE_START_LEN bytes sent in */

    if ((FILE_START_LEN - file_start.vlen) > 0) {
        int cc = FILE_START_LEN - file_start.vlen;
        if (nbuf < cc) cc = nbuf;       /* Don't copy more that we have! */
        memcpy(file_start.buf+file_start.vlen, buf, cc);
        file_start.vlen += cc;
    }

/* If not the first call for an output file, add a newline */
    if (cache.rst != 0 && !doing_newline) {
        doing_newline = 1;
        status = ffputline("\n", 1);
        doing_newline = 0;
        if (status != FIOSUC) return status;
    }
    cache.rst = 1;              /* Set the "seen first line" flag */

    while(nbuf > 0) {
        int to_fill;
        if ((cache.len + nbuf) <= CSIZE) to_fill = nbuf;
        else to_fill = CSIZE - cache.len;
        memcpy(cache.buf+cache.len, buf, to_fill);
        nbuf -= to_fill;        /* bytes left */
        cache.len += to_fill;  /* valid in cache */
        buf += to_fill;         /* new start of input */
        if (nbuf > 0) {         /* More to go, so flush cache */
            status = flush_write_cache();
            if (status != FIOSUC) return status;
        }
    }
    return FIOSUC;
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
                    curbp->b_EOLmissing = 1;
                    mlforce("Newline absent at end of file. Added....");
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
