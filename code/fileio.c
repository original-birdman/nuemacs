/*      fileio.c
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILEIO_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "errno.h"

#include "utf8proc.h"

static int ffp;             /* File unit, all functions. */

/* The cache.
 * Used for reading and writing as only one can be active at any one time.
 */
#define CSIZE 8192
static struct {
    char buf[CSIZE];
    int rst;                /* Read pointer */
    int len;                /* Valid tot chars (write) or past rst (read) */
} cache;

/* Check the first FILE_START_LEN bytes of each file written for
 * binary/text checking.
 */
#define FILE_START_LEN 100
static int file_type;   /* -1 binary, 0 unknown, +1 text */

/* Close the ffp file descriptor. Should look at the status in all systems.
 */
static int ffp_mode;
int ffclose(void) {

    int error_number = -1;
    if (ffp_mode == O_WRONLY) {
        if (fdatasync(ffp) < 0) error_number = errno;
    }
    if (close(ffp) < 0) {
        if (error_number != -1) error_number = errno;
    }
    if (error_number != -1) {
        mlwrite("Error closing file: %s", strerror(error_number));
        return FIOERR;
    }
    return FIOSUC;
}

/* Open file <fn> for reading on global file-unit ffp
 */
int ffropen(const char *fn) {
    struct stat statbuf;

    ffp_mode = O_RDONLY;

/* Opening for reading lets the caller display relevant message on not found.
 * This may be an error (insert file) or just mean you are opening a new file.
 */
    if ((ffp = open(fn, O_RDONLY)) < 0) return FIOFNF;

/* Check that whatever is open on ffp is a regular file.
 * uemacs *only* deals with files.
 */

    int status = fstat(ffp, &statbuf);
    if (status != 0)
        mlwrite("Cannot stat %s", fn);
    else {
        if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
            mlwrite("Not a file: %s", fn);
            status = FIOERR;        /* So we close&exit... */
        }
    }
    if (status != FIOSUC) {
        ffclose();
        return status;
    }

    if (pathexpand) set_buffer_filenames(curbp, fn);

/* Unset these on open */
    cache.rst = cache.len = 0;
    curbp->b_EOLmissing = 0;

    return FIOSUC;
}

/* Open file <fn> for writing on global file-unit ffp
 */
int ffwopen(const char *fn) {

    ffp_mode = O_WRONLY;

/* Opening for writing displays errors here */

    if ((ffp = open(fn, O_WRONLY|O_TRUNC|O_CREAT, 0666)) < 0) {
        if (errno == EISDIR)    /* Can't open a dir for writing */
            mlwrite("Can't write a directory: %s", fn);
        else
            mlwrite("Cannot open %s for writing - %s", fn, strerror(errno));
        return FIOERR;
    }
    if (pathexpand) set_buffer_filenames(curbp, fn);

    cache.rst = cache.len = 0;
    file_type = 0;      /* Unkown... */

    return FIOSUC;
}

/* Write a line to the already opened file. The "buf" points to the buffer,
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
 * file_is_binary() is only called when we are writing the first cache block,
 * so rst is still 0 and len is the real valid length.
 */

static int file_is_binary(void) {

    int bi = 0;
    int uc_total = 0, uc_text = 0;
/* Check up to FILE_START_LEN bytes. - but not more than is in the cache */
    int ccount = (FILE_START_LEN > cache.len)? cache.len: FILE_START_LEN;
    if (ccount == 0) return FALSE;  /* Prevent 0 division at end. */
    while (bi < ccount) {
        unicode_t uc;
        if (*(cache.buf+bi) < 0x7f) {   /* Could be ASCII */
            if ((*(cache.buf+bi) >= ' ') || (*(cache.buf+bi) == '\n')
                 || (*(cache.buf+bi) == '\t')) {
                uc_text++;              /* Printing char... */
            }
            bi++;
        }
/* We have a possible Unicode character */
        else {
            unsigned int ub = utf8_to_unicode(cache.buf, bi, cache.len, &uc);
            if (ub <= 0) break;
            bi += ub;
            uc_total++;
            const char *cat_str = utf8proc_category_string(uc);
/* Count the "text" characters. Get the category strings for
 * simpler test code.
 * We only need to check the first character.
 */
            switch(cat_str[0]) {
            case 'L':       /* Letter - LU, LL, LT, LM, LO */
            case 'N':       /* Number - ND, NL, NO */
            case 'P':       /* Punctuation - PC, PD, PS, PE, PI, PF, PO */
            case 'Z':       /* Separator, space */
                uc_text++;
            }
        }
        uc_total++;
    }
/* We'll say it's binary if <80% of the unicode characters are text */
    return (uc_text < (4*uc_total)/5)? -1: 1;
}

/* Routine to flush the cache */
static int flush_write_cache(void) {
    if (cryptflag) myencrypt(cache.buf, cache.len);
    int written = 0;
    errno = 0;
    while(1) {  /* Allow for interrupted writes */
        written = write(ffp, cache.buf, cache.len);
        if (errno != EINTR) break;
    }
    if (written < 0) {
        mlwrite("Write I/O error: %s", strerror(errno));
        return FIOERR;
    }
    cache.len -= written;
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
int ffputline(const char *buf, int nbuf) {

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
            else {
/* For a small file we might not have triggered the test in the loop below,
 * so check it now.
 */
                if (!file_type) file_type = file_is_binary();
                if (file_type == -1) reason = "binary";
            }
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
    cache.rst = 1;          /* Set the "seen first line" flag */

    while(nbuf > 0) {
        int to_fill;
        if ((cache.len + nbuf) <= CSIZE) to_fill = nbuf;
        else to_fill = CSIZE - cache.len;
        memcpy(cache.buf+cache.len, buf, to_fill);

/* Check the first FILE_START_LEN bytes once we have them.
 * Once the chekc has run file_type will be set to non-zero.
 */
        if (!file_type && (cache.len >= FILE_START_LEN)) {
            file_type = file_is_binary();
        }

        nbuf -= to_fill;        /* bytes left */
        cache.len += to_fill;   /* valid in cache */
        buf += to_fill;         /* new start of input */
        if (nbuf > 0) {         /* More to go, so flush cache */
            status = flush_write_cache();
            if (status != FIOSUC) return status;
        }
    }
    return FIOSUC;
}

/* Read a line from a file, and store the bytes into a global, Xmalloc()ed
 * buffer (fline).
 * Complain about lines at the end of the file that don't have a newline.
 * Check for I/O errors too. Return status.
 *
 * NOTE: that the fline gets linked in to the buffer lines, so doesn't
 * need to be freed here...
 */

/* Helper routine to add text into fline, re-allocating it if required.
 * Cannot return with any error. Any allocation error exist uemacs.
 */
static void add_to_fline(int len) {
    if (fline == NULL) fline = lalloc();

    db_appendn(ldb(fline), cache.buf+cache.rst, len);
    cache.rst += len;       /* Advance cache read-pointer */
    cache.len -= len;       /* Decrement left-to-read counter */
    return;
}

/* The actual callable function.
 * This starte with a new empty fline and fills it with the next line's data
 * in a malloc'ed area.
 * The building is done by add_to_fline().
 * If there is no data at all (an empty file) fline is not allocated.
 */
int ffgetline(void) {

/* Any previous fline will have been linked into the lp list, so
 * we can start afresh just by setting this to NULL.
 */
    fline = NULL;

/* Do we have a newline in our cache? */

    char *nlp = NULL;
    while (!nlp) {
        nlp = memchr(cache.buf+cache.rst, '\n', cache.len);
        if (!nlp) {                 /* No newline found */
/* If we already have something, add it to fline */
            if (cache.len > 0) add_to_fline(cache.len);
/* Get some more of the file. */
            errno = 0;
            while(1) {  /* Allow for interrupted reads */
                cache.len = read(ffp, cache.buf, sizeof(cache.buf));
                if (errno != EINTR) break;
            }
            if (cache.len < 0) {
                mlwrite("Read I/O error: %s", strerror(errno));
                cache.len = 0;
                return FIOERR;
            }
            cache.rst = 0;
/* If we are at the end...return it.
 * But - if we still have cached data then there was no final
 * newline, but we still have to return that, and warn about the
 * missing newline.
 */
            if (cache.len == 0) {   /* We read nothing... */
/* If we had an empty file then we will never have allocated fline.
 * So just return that we are at EOF.
 */
                if (!fline) return FIOEOF;
                if (lused(fline)) {
                    curbp->b_EOLmissing = 1;
                    mlforce("Newline absent at end of file. Added....");
                    sleep(1);
                }
                return lused(fline)? FIOSUC: FIOEOF;
            }
            if (cryptflag) myencrypt(cache.buf, cache.len);
        }
    }
    int cc = nlp - (cache.buf+cache.rst);
    add_to_fline(cc);
    cache.rst++;        /* Step over newline */
    cache.len--;        /* Step over newline */

    return FIOSUC;
}

/* Does file <fn> exist on disk?
 *
 */
int fexist(const char *fn) {
    struct stat statbuf;

    int status = stat(fn, &statbuf);
    if (status != 0) return FALSE;
    if ((statbuf.st_mode & S_IFMT) != S_IFREG) return FALSE;
    return TRUE;
}
