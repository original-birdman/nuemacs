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

#if MSDOS
#if  TURBO
#include <conio.h>
#include <dir.h>
struct ffblk fileblock; /* structure for directory searches */
#else
#include <dos.h>
struct find_t fileblock;
/*
#define FINDALL _A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_VOLID|_A_SUBDIR|_A_ARCH
*/
#define FINDALL _A_NORMAL|_A_SUBDIR
#endif
#endif

#if VMS
#include <lnmdef.h>
#include <descrip.h>

static $DESCRIPTOR(tab, "LNM$FILE_DEV");

#define MATCH       0
#define NO_MATCH    1
#define CONCEALED   2
#define LIST        4

static int  ANGLEB;
static char LEFTB;
static char RIGHTB;
#endif

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
#if BSD | (MSDOS & MSC)
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
#if VMS | MSDOS
    mklower(so_far);
#endif

    if (directory[0]) {
        strcpy(name, directory);
/*
#if MSDOS
        strcat(name, "\\");
#endif
*/
#if VMS

        l = strlen(name) - 1;
        if (so_far[strlen(so_far)-1] == RIGHTB) {
            if (name[l] == RIGHTB)
                name[l] = '.';
            else {
                l++;
                name[l++] = LEFTB;
                name[l] = 0;
            }
        }
#endif
        strcat(name, so_far);
    }
    else {
#if VMS
        if (so_far[strlen(so_far)-1] == ']') {
            strcpy(name, "[.");
            strcat(name, so_far);
        }
        else
#endif
            strcpy(name, so_far);
    }

    if (unique && strcmp(name, supplied))
        *choices = 0;
#if VMS | MSDOS
    else
        mklower(choices);
#endif

    return(TRUE);
}
/*
 * Here is what getffile() should do.  Parse out any directory part and
 * and file part.  Open that directory, if necessary.  Build a wildcard
 * from the file part.  Call getnfile() to return the first match.  If
 * the system is not case-sensitive, force lower case.
 */
#if MSDOS
char *getffile(char *fspec)
{
        int point;                      /* index into various strings */
        char *strrchr(), *index();      /* indexing function */
        char *p, *q;                    /* handy pointers */
        int extflag;                    /* is there an extension? */
        int len;
        char lookfor[NFILEN];

        strcpy(directory, mklower(fspec));
        strcpy(picture, directory);
        len = strlen(directory);
        point = len -1;
        while (point >= 0 && (directory[point] != '/' &&
            directory[point] != '\\' && directory[point] != ':'))
                --point;
        /* No directory part */
        if (point<0)
            directory[0] = 0;
        else {
            point++;
            directory[point] = 0;
        }
        point++;
        /* check for an extension */
        extflag = FALSE;
        while (point<len) {
            if (picture[point] == '.') {
                    extflag = TRUE;
                    break;
            }
            point++;
        }

        /* Build wildcard picture */
        strcat(picture, "*");
        if (!extflag)
                strcat(picture, ".*");

        /* Start things off.. */
#if MSC
        if (_dos_findfirst(picture, FINDALL, &fileblock))
#else
        if (findfirst(picture, &fileblock, 0) == -1)
#endif
            return(NULL);
#if MSC

        if ((strcmp(fileblock.name, ".") == 0) ||
            (strcmp(fileblock.name, "..") == 0))
                return(getnfile());

        if (fileblock.attrib == _A_SUBDIR)
                strcat(fileblock.name, "\\");

        return(fileblock.name);
#else
        return(fileblock.ff_name);
#endif
}

/*
 * Should work out if it's a directory.. if so return with a trailing \
 */
char *getnfile(void)
{
#if MSC
    if (_dos_findnext(&fileblock))
#else
    if (findnext(&fileblock) == -1)
#endif
        return(NULL);
#if MSC
        if ((strcmp(fileblock.name, ".") == 0) ||
            (strcmp(fileblock.name, "..") == 0))
                return(getnfile());

    if (fileblock.attrib == _A_SUBDIR)
                strcat(fileblock.name, "\\");

    return(fileblock.name);
#else
    return(fileblock.ff_name);
#endif
}

/* nop */
static void close_dir(void)
{
}
#endif
#if VMS

#include <descrip.h>
#include <rmsdef.h>

static struct dsc$descriptor spec, *specptr, result, *resultptr;
static long context;                   /* Context for repeated calls to lib$f_f */
static char suggestion[NFILEN];
static int versions;                   /* Do we want to know about versions? */
static int expand;                     /* expand directory component         */

char *getffile(char *fspec)
{
        int point;                      /* index into various strings */
        char *p, *q;                    /* handy pointers */
        int extflag;                    /* is there an extension? */
        int len, status, dummy;
        short loglen;
        char partial[NFILEN];

        /* is the joker using the old <> forms of treename..? */
        ANGLEB = FALSE;
        LEFTB =  '[';
        RIGHTB = ']';
        for (p = fspec; *p; p++)
            if ((*p == '<') || (*p == '>')) {
               ANGLEB = TRUE;
               LEFTB =  '<';
               RIGHTB = '>';
               break;
            }
/* end */

        strcpy(directory, mklower(fspec));

        len = strlen(directory);
        point = len -1;
        while (point >= 0 && (directory[point] != RIGHTB
            && directory[point] != ':' && directory[point] != LEFTB))
                --point;

        /* No directory part */
        if (point<0)  {
            strcpy(picture, directory);
            directory[0] = 0;
        }
        else if (directory[point] == LEFTB) {
            /* Ah, now here's a funny thing.. they have an open
               directory like [frog.toad?  If so presumably they want
               to dive within [frog].  If we can find the dot,
               build accordingly */
            p = &directory[point];
            while (*p) {
                if (*p == '.') {
                    *p = RIGHTB;
                    p++;
                    break;
                }
                p++;
            }
            strcpy(picture, directory);
            *p = 0;
         }
        else {

            strcpy(picture, directory);
            point++;
            directory[point] = 0;


            /* check for [...] */
            if (point  >= 4) {
                p = &directory[point];
                p--;
                p--;
                extflag = 0;
                while (*p == '.') {
                    extflag++;
                    p--;
                }
                if (extflag >= 3) {
                    return(NULL);   /* disallow */
                }
            }
        }

        point++;
        /* check for an extension */
        extflag = 0;
        while (point<len) {
            if (picture[point] == '.')
                    extflag++;
            point++;
        }

        versions = ((extflag > 1) || ((p = strchr(picture, ';'))));

        /* XX.YY;-1 is a special case */
        if (!(versions && p && (*(++p) == '-'))) {
            /* Build wildcard picture */
            strcat(picture, "*");
            if (!extflag)
                strcat(picture, ".*");
        }

        status = do_expansion((LIST|CONCEALED), 0, suggestion, picture, &dummy);

        if (!(status & NO_MATCH))
            strcpy(picture, suggestion);

        /* Build picture descriptor */
        spec.dsc$a_pointer = &picture[0];
        spec.dsc$w_length  = strlen(picture);
        spec.dsc$b_dtype   = DSC$K_DTYPE_T;
        spec.dsc$b_class   = DSC$K_CLASS_S;
        specptr = &spec;

        result.dsc$a_pointer = &suggestion[0];
        result.dsc$w_length  = NFILEN;
        result.dsc$b_dtype   = DSC$K_DTYPE_T;
        result.dsc$b_class   = DSC$K_CLASS_S;
        resultptr = &result;

        context = 0;

        /* Start things off.. */
        return(getnfile());
}

/*
 * Should work out if it's a directory.. if so return with a trailing \
 */
char *getnfile(void)
{
    char *p;
    int status;
    extern int lib$find_file();

    status = lib$find_file(specptr, resultptr, &context, NULL, NULL, 0, 0);

    if (status == RMS$_NORMAL) {
        suggestion[result.dsc$w_length] = 0;
        p = &suggestion[result.dsc$w_length - 1];
        while (*p == ' ' && p != &suggestion[0])
            p--;
        p++;
        *p = 0;

        /* Code to strip off suffix.. */
        if (!versions) {
            while (*p != ';')
                p--;
            *p = 0;
        }

        /* Now hunt back for the . to see if we have a .dir suffix.. */
        while (*p != '.')
            p--;
        if ((strlen(p) >= 4) && (!strcmp(p, ".DIR"))) {
            *p++ = RIGHTB;
            *p-- = 0;
            p--;
        }

        /* Code to strip off the directory name - there WILL be one */
        while ((*p != ':') && (*p != RIGHTB))
            p--;
        p++;
        strcpy(suggestion, p);

/* what else? */

        return(suggestion);
    }
    else {   /* This should be  (status == RMS$_NMF) */
        return(NULL);
    }
}

static void close_dir(void)
{
    extern int lib$find_file_end();
    lib$find_file_end(&context);
}
#endif


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

static char *getffile(char *fspec)
{
        char *p;                        /* handy pointers */

        /* Initialise things */
        dirptr = NULL;

#if EXPAND_TILDE
        expand_tilde(fspec);
#endif
#if EXPAND_SHELL
        expand_shell(fspec);
#endif

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

        if (!directory[0])
                dirptr = opendir(".");
        else
                dirptr = opendir(directory);

        if (dirptr == NULL)
            return(NULL);

        piclen = strlen(picture);
        allfiles = (piclen == 0);

        /* And return the first match (we return ONLY matches, for speed) */
        return(getnfile());
}

static char *getnfile(void)
{
        unsigned short type;                /* file type                */
#if USG
        struct dirent *dp;
#else
        struct direct *dp;
#endif
        struct stat statbuf;
        int namelen;

        /* Get the next directory entry, if no more, we finish */
        if ((dp = readdir(dirptr)) == NULL)
            return(NULL);

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
                if (type == S_IFDIR)
                    strcat(suggestion, "/");
                return(suggestion);
            }
        }
        return(getnfile());
}

static void close_dir(void)
{
    if (dirptr != NULL)
        closedir(dirptr);
}
#endif


static short ljust(char *str)
{
    char *p, *q;
    short justified;

    justified = FALSE;
    p = str;
    while (*p++ == ' ')
        justified = TRUE;
    p--;
    if (justified) {
        q = str;
        while (*p!=0)
            *q++ = *p++;
        *q = 0;
    }
    return(justified);
}


int comp_buffer(char *name, char *choices)
{
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
#if MSDOS & IBMPC
    max--;
#endif
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
    strcpy(name, so_far);

    if (unique && strcmp(name, supplied))
        *choices = 0;

    return(TRUE);
}

static char *getfbuffer(char *bpic, int bpiclen)
{
    expandbp = bheadp;
    return(getnbuffer(bpic, bpiclen));
}

static char *getnbuffer(char *bpic, int bpiclen)
{
    char *retptr;

    if (expandbp) {
            /* We NEVER return minibuffer buffers (CC$00nnn), and
               we return internal [xx] buffers only if the user asked
               for them by specifying a picture starting with [ */

        /* We don't return a buffer if it doesn't match, or if it's
           a minibuffer buffername (CC$) or if it's an internal
           buffer [xx], unless the user *asked* for these */

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


#if VMS
int logical_depth(char *source)
{
    struct dsc$descriptor spec, *specptr;

    char *p;
    short index_ret;
    int status;
    int depth;

    struct item_list {
        short index_len;
        short index_att;
        long *index_ptr;
        short *index_retptr;
        long terminate;
    };
    struct item_list list;
    static unsigned long attributes = LNM$M_CASE_BLIND;

    list.index_len = 4;
    list.index_att = LNM$_MAX_INDEX;
    list.index_ptr = &depth;
    list.index_retptr = &index_ret;
    list.terminate = 0;

    for (spec.dsc$a_pointer = source; *spec.dsc$a_pointer == ' ';
         spec.dsc$a_pointer++)
        ;

    spec.dsc$w_length  = strlen(spec.dsc$a_pointer);
    spec.dsc$b_dtype   = DSC$K_DTYPE_T;
    spec.dsc$b_class   = DSC$K_CLASS_S;
    specptr = &spec;

    status = sys$trnlnm(&attributes, &tab, specptr, 0, &list);

    if ((status != 1) || (!index_ret))
        depth = 0;

    return(depth);
}

int expand_logical(char *target, char *source, int depth)
{
    struct dsc$descriptor spec, *specptr;

    short index_ret, string_ret, att_ret;
    int details;
    int status;

    char copy[256];

    struct item_list {
        short index_len;
        short index_att;
        long *index_ptr;
        short *index_retptr;

        short string_len;
        short string_att;
        char  *string_ptr;
        short *string_retptr;

        short att_len;
        short att_att;
        long *att_ptr;
        short *att_retptr;

        long terminate;
    };
    struct item_list list;
    static unsigned long attributes = LNM$M_CASE_BLIND;
    int hit;

    hit = NO_MATCH;
    strcpy(copy, source);
    ljust(copy);

    list.index_len = 4;
    list.index_att = LNM$_INDEX;
    list.string_len = NFILEN;
    list.string_att = LNM$_STRING;
    list.terminate = 0;

    list.index_retptr = &index_ret;
    list.string_retptr = &string_ret;

    spec.dsc$a_pointer = copy;
    spec.dsc$w_length  = strlen(copy);
    spec.dsc$b_dtype   = DSC$K_DTYPE_T;
    spec.dsc$b_class   = DSC$K_CLASS_S;
    specptr = &spec;

    list.string_ptr = target;
    list.index_ptr = &depth;

    list.att_len = 4;
    list.att_att = LNM$_ATTRIBUTES;
    list.att_retptr = &att_ret;
    list.att_ptr = &details;

    status = sys$trnlnm(&attributes, &tab, specptr, 0, &list);

    hit = NO_MATCH;

    if ((status == 1) && string_ret) {
        if ((att_ret) && (details & LNM$M_CONCEALED))
            hit = CONCEALED;
        else
            hit = MATCH;
        target[string_ret] = 0;

        string_ret--;
        if (string_ret && (target[string_ret--] == ']')) {
            if (string_ret && (target[string_ret] == '.')) {
                target[string_ret++] = ']';
                target[string_ret++] = 0;
            }
        }

    }

    return(hit);
}

/* char *s; supplied string                        */
char *parse_logical(char *s)
{
char *r;    /* remnant after removing log:, if any    */

    if ((r = strchr(s, ':'))) {
        *r = 0;
        r++;
        return(r);
    }
    else
        return(NULL);
}


/* As strcat(t, s) -> ts, so strtac(t, s) -> st */
/* we also handle ][ -> . */
char *strtac(char *t, char *s)
{
    char *copy;
    char *malloc();
    char *p;

    strcpy((copy = malloc(strlen(t)+1)), t);
    strcpy(t, s);
    p = &t[strlen(t)-1];
    if (*p == ']'  && *copy == '[') {
        *p = '.';
        p++;
        *p = 0;
        strcat(t, &copy[1]);
    }
    else
        strcat(t, copy);
    free(copy);
    return(t);
}


int do_expansion(int key, int level, char *copyout, char *raw, int *depth)
{
    int status;                 /* Return status          */
    char debris[NFILEN];        /* Logical:debris         */
    char copyin[NFILEN];        /* working copy of raw    */
    char *p;                    /* A handy pointer        */
    int expanded;               /* Some expansion         */
    int r;
    int concealment;
    int survive_list;
    int survive_conceal;

    survive_list = (key & LIST);
    survive_conceal = (key & CONCEALED);

    status = NO_MATCH;
    expanded = FALSE;
    *depth = 0;
    concealment = FALSE;

    strcpy(copyin, raw);

    if ((p = parse_logical(copyin)))
        strcpy(debris, p);
    else {
        return(status);
    }

    while (TRUE) {

        *depth = logical_depth(copyin);

        if (*depth && !survive_list)
            break;
        r = expand_logical(copyout, copyin, level);

        if (r == NO_MATCH)
            break;
        if (r == MATCH) {
            expanded = TRUE;
        }
        else {
            concealment = TRUE;
            if (survive_conceal) {
                expanded = TRUE;
            }
            else
                break;
        }
        if ((p = parse_logical(copyout)))
            strtac(debris, p);
        else
            break;
        strcpy(copyin, copyout);
    }

    if (expanded) {
        status = MATCH;
        if (copyin[strlen(copyin)-1] != ']') {
            strcpy(copyout, copyin);
            strcat(copyout, ":");
            strcat(copyout, debris);
        }
        else {
            strtac(debris, copyin);
            strcpy(copyout, debris);
        }
    }
    if (*depth && !survive_list)
        status |= LIST;
    if (concealment && !survive_conceal)
        status |= CONCEALED;

    return(status);
}
#endif
