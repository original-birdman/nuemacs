/*      complet.c
 *
 *      A set of functions having to do with filename completion.
 *      Partof thre GGR additions.
 */

#include <stdio.h>
#include <string.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

static void close_dir(void);

static char picture[NFILEN], directory[NFILEN];

static char *getffile(char *), *getnfile(void);
static char *getfbuffer(char *, int), *getnbuffer(char *, int);
static short ljust(char *);

static struct buffer *expandbp;

int comp_file(char *name, char *choices)
{
    char so_far[NFILEN];        /* Maximal match so far     */
    char supplied[NFILEN];      /* Maximal match so far     */
    char *next;                 /* Next filename to look at */
    char *p, *q;                /* Handy pointers           */
    short match_length;         /* Maximal match length     */
    short max;
    short l, unique;

    *choices = 0;

    /* Left-justify the user's input */
    ljust(name);
    if ((p = getffile(name)) == NULL) {
        close_dir();
        return(FALSE);
    }
    else
        strcpy(so_far, p);
    strcpy(supplied, name);
    match_length = strlen(so_far);
/* Restrict length of returned string to number of columns, so that we
 * don't end up wrapping in the minibuffer line.
 */
    max = term.t_ncol;
#if BSD
    max--;
#endif
    l = (match_length < max) ? match_length : max;
/* We also need to check we are not going to overflow the
 * destination buffer, and we have to allow for the final NUL
 */
    if (l >= NFILEN) l = NFILEN - 1;
    strncpy(choices, so_far, l);
    choices[l] = '\0';          /* We need to do this ourself */
    max -= l;
    unique = TRUE;
    while ((next = getnfile())) {
        unique = FALSE;
        for (p = &so_far[0], q = next, match_length = 0;
            (*p && *q && (*p == *q)); p++, q++)
            match_length++;
        so_far[match_length] = 0;
        l = strlen(next);
        if (max == 0) {
            if (match_length == 0)
                break;
        }
        else if (l < max) {
            strcat(choices, " ");
            strcat(choices, next);
            max -= (l + 1);
        }
        else {
            l = (max - 1);
            strcat(choices, " ");
            strncat(choices, next, l);
            max = 0;
        }
    }
    close_dir();
    if (directory[0]) {
        strcpy(name, directory);
        strcat(name, so_far);
    }
    else {
        strcpy(name, so_far);
    }

    if (unique && strcmp(name, supplied))
        *choices = 0;
    return(TRUE);
}
/*
 * Here is what getffile() should do.  Parse out any directory part and
 * and file part.  Open that directory, if necessary.  Build a wildcard
 * from the file part.  Call getnfile() to return the first match.  If
 * the system is not case-sensitive, force lower case.
 */
#if BSD | USG
#include <sys/types.h>

#if USG
#include <dirent.h>
#else
#include <sys/dir.h>
#endif

#include <sys/stat.h>

static DIR *dirptr;
static int piclen;
static char *nameptr;
static char suggestion[NFILEN];
static short allfiles;
static char fullname[NFILEN];

static char *getffile(char *fspec) {
    char *p;                        /* handy pointers */

    dirptr = NULL;                  /* Initialise things */

    fixup_fname(fspec);
    strcpy(directory, fspec);

    if ((p = strrchr(directory, '/'))) {
        p++;
        strcpy(picture, p);
        *p = 0;
        strcpy(fullname, directory);
    }
    else {
        strcpy(picture, directory);
        directory[0] = 0;
        fullname[0] = 0;
    }

    nameptr = &fullname[strlen(fullname)];

    if (!directory[0]) dirptr = opendir(".");
    else               dirptr = opendir(directory);

    if (dirptr == NULL) return(NULL);

    piclen = strlen(picture);
    allfiles = (piclen == 0);

/* And return the first match (we return ONLY matches, for speed) */
    return(getnfile());
}

static char *getnfile(void) {
    unsigned short type;        /* file type */
#if USG
    struct dirent *dp;
#else
    struct direct *dp;
#endif
    struct stat statbuf;
    int namelen;

/* Get the next directory entry, if no more, we finish */
    if ((dp = readdir(dirptr)) == NULL) return(NULL);

    if ((allfiles ||
         (((namelen = strlen(dp->d_name)) >= piclen) &&
         (!strncmp(dp->d_name, picture, piclen)))) &&
         (strcmp(dp->d_name, ".")) &&
         (strcmp(dp->d_name, ".."))) {

        strcpy(nameptr, dp->d_name);
        stat(fullname, &statbuf);
        type = (statbuf.st_mode & S_IFMT);
        if ((type == S_IFREG)
#ifdef S_IFLNK
             || (type == S_IFLNK)
#endif
             || (type == S_IFDIR)) {
            strcpy(suggestion, dp->d_name);
            if (type == S_IFDIR) strcat(suggestion, "/");
            return(suggestion);
        }
    }
    return(getnfile());
}

static void close_dir(void) {
    if (dirptr != NULL) closedir(dirptr);
}
#endif

static short ljust(char *str) {
    char *p, *q;
    short justified;

    justified = FALSE;
    p = str;
    while (*p++ == ' ') justified = TRUE;
    p--;
    if (justified) {
        q = str;
        while (*p!=0) *q++ = *p++;
        *q = 0;
    }
    return(justified);
}


int comp_buffer(char *name, char *choices) {
    char so_far[NFILEN];        /* Maximal match so far     */
    char supplied[NFILEN];      /* Maximal match so far     */
    char *next;                 /* Next filename to look at */
    char *p, *q;                /* Handy pointers           */
    short match_length;         /* Maximal match length     */
    short max;
    short l, unique;
    int namelen;

    *choices = 0;
    ljust(name);
    namelen = strlen(name);
    if ((p = getfbuffer(name, namelen)) == NULL)
        return(FALSE);
    else
        strcpy(so_far, p);
    strcpy(supplied, name);
    match_length = strlen(so_far);
    max = term.t_ncol;
    l = (match_length < max) ? match_length : max;
/* We also need to check we are not going to overflow the
 *  destination buffer, and we have to allow for the final NUL
 */
    if (l >= NFILEN) l = NFILEN - 1;
    strncpy(choices, so_far, l);
    choices[l] = '\0';          /* We need to do this ourself */
    max -= l;
    unique = TRUE;
    while ((next = getnbuffer(name, namelen)) != NULL) {
        unique = FALSE;
        for (p = &so_far[0], q = next, match_length = 0;
            (*p && *q && (*p == *q)); p++, q++)
            match_length++;
        so_far[match_length] = 0;
        l = strlen(next);
        if (max == 0) {
            if (match_length == 0) break;
        }
        else if (l < max) {
            strcat(choices, " ");
            strcat(choices, next);
            max -= (l + 1);
        }
        else {
            l = (max - 1);
            strcat(choices, " ");
            strncat(choices, next, l);
            max = 0;
        }
    }
    strcpy(name, so_far);

    if (unique && strcmp(name, supplied)) *choices = 0;

    return(TRUE);
}

static char *getfbuffer(char *bpic, int bpiclen) {
    expandbp = bheadp;
    return(getnbuffer(bpic, bpiclen));
}

static char *getnbuffer(char *bpic, int bpiclen) {
    char *retptr;

    if (expandbp) {
/* We NEVER return minibuffer buffers (CC$00nnn), and we return internal
 * [xx] buffers only if the user asked for them by specifying a picture
 * starting with [.
 */

/* We don't return a buffer if it doesn't match, or if it's a minibuffer
 * buffername (CC$) or if it's an internal buffer [xx], unless the user
 * *asked* for these.
 */
        if ((strncmp(bpic, expandbp->b_bname, bpiclen)) ||
            (!strncmp(expandbp->b_bname, "CC$", 3)) ||
            ((expandbp->b_bname[0] == '[') && bpiclen == 0)) {

            expandbp = expandbp->b_bufp;
            return(getnbuffer(bpic, bpiclen));
        }
        else {
            retptr = expandbp->b_bname;
            expandbp = expandbp->b_bufp;
            return(retptr);
        }
    }
    else
        return(NULL);
}
