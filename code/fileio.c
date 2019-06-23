/*      FILEIO.C
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "errno.h"

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
 * Check that whatever is open on ffp is a regular file.
 * uemacs *only* deals with files.
 */
static int check_for_file(char *fn) {
    struct stat statbuf;

    int status = fstat(fileno(ffp), &statbuf);
    if (status != 0)
        mlwrite("Cannot stat %s", fn);
    else {
        if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
            mlwrite("Not a file: %s", fn);
            status = FIOERR;        /* So we close&exit... */
        }
    }
    if (status != 0) {
        fclose(ffp);
        return FIOERR;
    }
    return FIOSUC;
}

/*
 * Open file <fn> for reading on global file-handle ffp
 */
int ffropen(char *fn) {

#if (BSD | USG)
    fixup_fname(fn);
#endif
/* Opening for reading lets the caller display relevant message on not found.
 * This may be an error (insert file) or just mean you are openign a new file.
 */
    if ((ffp = fopen(fn, "r")) == NULL) return FIOFNF;
    int status = check_for_file(fn);    /* Check ffp - fn is for messages */
    if (status != FIOSUC) return status;

    if (pathexpand) {       /* GGR */
/* If activating an inactive buffer, these may be the same and the
 * action of strcpy() is undefined for overlapping strings.
 * On a Mac it will crash...
 */
        if (curbp->b_fname != fn) strcpy(curbp->b_fname, fn);
    }

/* Unset these on open */
    cache.rst = cache.len = 0;
    curbp->b_EOLmissing = 0;
    fline = NULL;
    eofflag = FALSE;

    return FIOSUC;
}

/*
 * Open file <fn> for writing on global file-handle ffp
 */
int ffwopen(char *fn) {

#if (BSD | USG)
    fixup_fname(fn);
#endif
/* Opening for writing displays errors here */
    if ((ffp = fopen(fn, "w")) == NULL) {
        if (errno == EISDIR)    /* Can't open a dir for writing */
            mlwrite("Can't write a directory: %s", fn);
        else
            mlwrite("Cannot open %s for writing", fn);
        return FIOERR;
    }
    int status = check_for_file(fn);    /* Check ffp - fn is for messages */
    if (status != FIOSUC) return status;

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

#if USG | BSD
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
 * If the file actually had a newline there we have no problem - we just
 * put one there while writing out.
 * If it didn't have one then the b_EOLmissing flags will have been set.
 * There are three possibly cases:
 *
 *  1. We have a text file.
 *     => We want to add a newline.
 *  2. We have a CRYPT file .  i.e. we read in a CRYPT file to a normal
 *     buffer, switched on CRYPT and are now writing it back out again
 *     (as real text). The final newline here is not real, and if we
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
    if (cryptflag) myencrypt(cache.buf, cache.len);
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
                if ((curbp->b_mode & MDDOSLE) == 0)
                   status = ffputline("\n", 1);
                else
                   status = ffputline("\r\n", 2);
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
        if ((curbp->b_mode & MDDOSLE) == 0)
            status = ffputline("\n", 1);
        else
            status = ffputline("\r\n", 2);
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
 * Read a line from a file, and store the bytes into a global, Xmalloc()ed
 * buffer (fline).
 * Complain about lines at the end of the file that don't have a newline.
 * Check for I/O errors too. Return status.
 */

/* Helper routine to add text into fline, re-allocating it if required */
static int add_to_fline(int len) {

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
            if (!add_to_fline(cache.len))
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
            if (cryptflag) myencrypt(cache.buf, cache.len);
        }
    }
    int cc = nlp - (cache.buf+cache.rst);
    if (!add_to_fline(cc)) return FIOMEM;
    cache.rst++;        /* Step over newline */
    cache.len--;        /* Step over newline */

    return FIOSUC;
}

/*
 * Does file <fn> exist on disk?
 *
 */
int fexist(char *fn) {
    struct stat statbuf;

#if (BSD | USG)
    fixup_fname(fn);
#endif

    int status = stat(fn, &statbuf);
    if (status != 0) return FALSE;
    if ((statbuf.st_mode & S_IFMT) != S_IFREG) return FALSE;
    return TRUE;
}

#if BSD | USG

#include <libgen.h>
#include <pwd.h>

static char pwd_var[NFILEN];
static int have_pwd = 0;    /* 1 == have it, -1 == tried and failed */

void fixup_fname(char *fn) {
    char fn_copy[NFILEN];
    char *p, *q;
    struct passwd *pwptr;

/* We start with looking for either ../ or ~ at the start (we can't have
 * both).
 */
    if (have_pwd >= 0 &&
        fn[0] == '.' && fn[1] == '.' && (fn[2] == '/' || fn[2] == '\0')) {
/* We have to count the leading occurrences of ../ and append what is left
 * to $PWD less the number of occurences.
 */
        int dd_cnt = 1;             /* We know we have one */
        char *fnwp = fn+2;
        while (*fnwp) {
            while (*++fnwp == '/'); /* Skip over any '/' */
            if (*fnwp == '.' && *(fnwp+1) == '.' &&
                  (*(fnwp+2) == '/' || *(fnwp+2) == '\0')) {
                dd_cnt++;
                fnwp = fnwp+2;
                continue;
            }
            break;
        }
        if (have_pwd == 0) {
            if ((p = getenv("PWD")) == NULL) have_pwd = -1;
            else {
                if (*p == '/') {    /* Only if valid path.. */
                    strcpy(pwd_var, p);
                    have_pwd = 1;
                }
                else
                    have_pwd = -1;
            }
        }
        if (have_pwd == 1) {
            strcpy(fn_copy, pwd_var);
            p = fn_copy;
            while(dd_cnt--) p = dirname(p);
            strcpy(fn_copy, p);
            strcat(fn_copy, "/");
            strcat(fn_copy, fnwp);
            strcpy(fn, fn_copy);
        }
    }
    else if (fn[0]=='~') {          /* HOME dir wanted... */
        if (fn[1]=='/' || fn[1]==0) {
            if ((p = getenv("HOME")) != NULL) {
                strcpy(fn_copy, fn);
                strcpy(fn , p);
                int i = 1;
/* Special case for root (i.e just "/")! */
                if (fn[0]=='/' && fn[1]==0 && fn_copy[1] != 0)
                    i++;
                strcat(fn, fn_copy+i);
            }
        }
        else {
            p = fn + 1;
            q = fn_copy;
            while (*p != 0 && *p != '/')
                *q++ = *p++;
            *q = 0;
            if ((pwptr = getpwnam(fn_copy)) != NULL) {
                strcpy(fn_copy, pwptr->pw_dir);
                strcat(fn_copy, p);
                strcpy(fn, fn_copy);
            }
        }
    }
    return;
}
#endif
