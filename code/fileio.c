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
    if (status != 0) ffclose();
    return status;
}

/*
 * Open file <fn> for reading on global file-handle ffp
 */
int ffropen(char *fn) {

#if (BSD | USG)
    fixup_fname(fn);
#endif
/* Opening for reading lets the caller display relevant message on not found.
 * This may be an error (insert file) or just mean you are opening a new file.
 */
    if ((ffp = fopen(fn, "r")) == NULL) return FIOFNF;
    int status = check_for_file(fn);    /* Checks ffp - fn is for messages */
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
    int status = check_for_file(fn);    /* Checks ffp - fn is for messages */
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

    eofflag = FALSE;

#if USG | BSD
    if (fclose(ffp) != FALSE) {
        mlwrite_one("Error closing file");
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
 *     => We do not want to add a newline if there wasn't one originally.
 *
 * We can detect 2. by checking for cryptflag && curbp->b_EOLmissing.
 * We also need to code make a heuristic check between a text and binary file.
 */

static int file_is_binary(void) {

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
        mlwrite_one("Write I/O error");
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
 *
 * NOTE: that the fline gets linked in to the buffer lines, so doesn't
 * need to be freed here...
 */

/* Helper routine to add text into fline, re-allocating it if required */
static int add_to_fline(int len) {

    int newlen = -1;
    if (fline == NULL) {
/* Ensure we don't fall through to memcpy() with fline==NULL.
 * Reported by -fanalyzer option to gcc-10.
 * (len should never be -ve anyway).
 */
        if (len < 0) return FALSE;
        newlen = len;
    }
    else if (len >= (fline->l_size - fline->l_used)) {
        newlen = len + fline->l_used;
    }
    if (newlen >= 0) {      /* Need a new/bigger fline */
        if (fline) {        /* realloc.... */
            int newsize = (newlen + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
            fline = (struct line *)Xrealloc(fline, sizeof(struct line) + newsize);
            fline->l_size = newsize;    /* Don't update l_used YET!! */
        }
        else {              /* alloc.... */
            fline = lalloc(newlen);
            if (fline == NULL) return FALSE;
            fline->l_used = 0;  /* Set was is actually there NOW!! */
        }
    }
    memcpy(fline->l_text+fline->l_used, cache.buf+cache.rst, len);
    fline->l_used += len;   /* Record the real size of the line */
    cache.rst += len;       /* Advance cache read-pointer */
    cache.len -= len;       /* Decrement left-to-read counter */
    return TRUE;
}

/* The actual callable function.
 * This starte with a new empty fline and fills it with the next line's data
 * in a malloc'ed area.
 * The building is done by add_to_fline().
 * If there is no data at all (an empty file) fline is not allocated.
 */
int ffgetline(void) {

/* Any previous fline will have been linked into the lp list, so
 * we can start afresh just by settign this to NULL.
 */
    fline = NULL;

/* Do we have a newline in our cache? */

    char *nlp = NULL;
    while (!nlp) {
        nlp = memchr(cache.buf+cache.rst, '\n', cache.len);
        if (!nlp) {                 /* No newline found */
/* If we already have something, add it to fline */
            if ((cache.len > 0) && !add_to_fline(cache.len))
                return FIOMEM;      /* Only reason for failure */
/* Get some more of the file. */
            cache.len = fread(cache.buf, sizeof(*cache.buf),
                sizeof(cache.buf), ffp);
            cache.rst = 0;
/* If we are at the end...return it.
 * But - if we still have cached data then there was no final
 * newline, but we still have to return that, and warn about the
 * missing newline.
 */
            if (eofflag && (cache.len == 0)) {
/* If we had an empty file then we will never have allocated fline.
 * So just return that we are at EOF.
 */
                if (!fline) return FIOEOF;
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
                    mlwrite_one("File read error");
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

/* Routines to "fix-up" a filename.
 * fixup_fname() will replace any ~id at the start with its absolute path
 * replacement, and prepend $PWD to any leading ".." or ".".
 * It then strips out multiple consecutive "/"s and handles internal
 * ".." and "." entries.
 * fixup_full() expands any leading "." to an absolute name (which we
 * don't always wish to do) then calls fixup_fname() with what it has.
 */
#include <libgen.h>
#include <pwd.h>

static char pwd_var[NFILEN];
static int have_pwd = 0;    /* 1 == have it, -1 == tried and failed */

void fixup_fname(char *fn) {
    char fn_copy[NFILEN];
    char *p, *q;
    struct passwd *pwptr;

/* Look for a ~ at the start. */

    if (fn[0]=='~') {          /* HOME dir wanted... */
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
    else if ((have_pwd >= 0) &&
  ((fn[0] == '.' && fn[1] == '.' && (fn[2] == '/' || fn[2] == '\0')) ||
   (fn[0] == '.' && (fn[1] == '/' || fn[1] == '\0')))) {
/* Just prepend $PWD - the slash_* loop at the end will fix up '.' and ".." */
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
            strcat(fn_copy, "/");
            strcat(fn_copy, fn);
            strcpy(fn, fn_copy);
        }
    }

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
    if (fn[0] != '/') {
        resets[0] = fn;
        rsi = 1;
        resettable = 1;
    }
    else resettable = 0;

    for (from = fn, to = fn; *from; from++) {
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
                if ((fn[0] == '/') || (resettable >= 2)) {
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
 * fix that with the to <= fn check.
 */
    if (prev_was_slash) to--;
    if (slash_dot_state == 2) to -= 2;
    if (slash_d_d_state == 3) {
        if ((fn[0] == '/') || (resettable >= 2)) {
            rsi -= 2;
            if (rsi < 0) rsi = 0;
            resettable -= 2;
            if (resettable < 1) resettable = 1;
            to = resets[rsi] - 1;   /* Remove trailing / */
        }
    }
    if (to <= fn) to = fn+1;    /* Just have '/' or '.' left...keep it */
    *to = '\0';

    return;
}

/* This one expands a leading "." (for current dir) as well.
 * It also handles anything not starting with a / or ~/ the same way.
 * We don't usually want this, as we want "code.c" to stay as "code.c"
 * but for directory browsing we always need the full path.
 */

void fixup_full(char *fn) {
    char fn_copy[NFILEN];
    char *p;

/* If the filename doesn't start with '/' or '~' we prepend "$PWD/".
 * Then we call fixup_fname() to do what it can do, which includes
 * stripping out redundant '.'s and handling ".."s.
 */
    if (have_pwd >= 0 && (fn[0] != '/' && fn[0] != '~')) {
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
            strcat(fn_copy, "/");
            strcat(fn_copy, fn);
            strcpy(fn, fn_copy);
        }
    }

    fixup_fname(fn);    /* For '/', '.' and ".."  handling */
    return;
}
#endif
