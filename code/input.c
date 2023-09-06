/*      input.c
 *
 *      Various input routines
 *
 *      written by Daniel Lawrence 5/9/86
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"

#include "utf8proc.h"

/* GGR - some needed things for minibuffer handling */

#include <signal.h>
#include "line.h"
static int remap_c_on_intr = 0;

/* A set of functions to do with filename/buffer/name completions.
 * Part of the GGR additions.
 */

#include <string.h>

static char picture[NFILEN], directory[NFILEN];

static struct buffer *expandbp;

/* Static variable shared by comp_file(), comp_buff() and matcher() */

static char so_far[NFILEN];     /* Maximal match so far */

/* getnfile() and getffile()
 * Handle file name completion
 *
 * Here is what getffile() should do.
 * Parse out any directory and file parts.
 * Open that directory, if necessary.
 * Build a wildcard from the file part.
 * Call getnfile() to return the first match.
 * If the system is not case-sensitive, force lower case.
 */
#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>

static DIR *dirptr;
static int piclen;
static char *nameptr;
static char suggestion[NFILEN];
static short allfiles;
static char fullname[NFILEN];

static void close_dir(void) {
    if (dirptr != NULL) {
        closedir(dirptr);
        dirptr = NULL;
    }
}

/* We need to remember these across successive calls to getnfile */
static char exp_id[NFILEN];
static char id_to_find[40];
static int id_to_find_len;
static int run_id_finder = 0;

static char *id_finder(void) {
/* Handle ~xxx by looking up all matching login ids.
 * If we come here we know we have an id to handle...
 */
    struct passwd *pwe;
    errno = 0;
    while ((pwe = getpwent())) {
        if (!strncmp(pwe->pw_name, id_to_find, id_to_find_len)) {
            snprintf(exp_id, NFILEN, "~%s", pwe->pw_name);
            return exp_id;
        }
    }
    if (errno) mlwrite("passwd lookup: %s", strerror(errno));
    endpwent();
    return NULL;
}

/* What to show in the minibuffer if completion doesn't find anything */

#define NOMATCH "No match"

/* Get the next matching file name
 */
static char *getnfile(void) {
    unsigned short type;        /* file type */

/* Handle ~xxx by looking up all matching login ids.
 * NOTE that ~xxx/ has specified a full id, and so is handled by
 * fixup_fname() when we drop into the filename code.
 */
    if (run_id_finder) return id_finder();

    struct dirent *dp;
    struct stat statbuf;
    int namelen;

/* Get the next directory entry, if no more, we finish */
    if ((dp = readdir(dirptr)) == NULL) return NULL;

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
            return suggestion;
        }
    }
    return getnfile();
}

/* Get the first matching filename.
 * Actually just sets things up so that getnfile() or id_finder()
 * can get it...
 */
static char *getffile(char *fspec) {
    char *p;                        /* handy pointers */

    dirptr = NULL;                  /* Initialise things */

/* Handle ~xxx by looking up all matching login ids.
 * NOTE that ~xxx/ has specified a full id, and that is handled by
 * fixup_fname().
 */
    if ((*fspec == '~') && (!strchr(fspec, '/'))) {
        directory[0] = '\0';        /* Forget any previous one */
        strncpy(id_to_find, fspec+1, 39);
        id_to_find[39] = '\0';      /* Ensure NUL terminated */
        id_to_find_len = strlen(id_to_find);
        run_id_finder = 1;
        setpwent();                 /* Start it off */
        char *res = id_finder();
/* If we don't find anything in pwd, drop into the file code
 * so we can find a file of that name, if one is there...
 * Since there is no matching id, fixup_fname() will leave it unchanged.
 */
        if (res) return res;
    }
    if (run_id_finder) endpwent();  /* In case it was opened earlier */
    run_id_finder = 0;

/* fixup_fname will strip any trailing '/'
 * This leads to a "loop", as the rest of this code would just add it back
 * and so we never get to the point of looking for entries within it.
 * So, if one is there on entry, put it back after fixup_fname does its work.
 * NOTE: that we check for the end offset being > 0, as that means "/" isn't
 * treated as a trailing slash.
 */
    int fspec_eoff = strlen(fspec) - 1;
    if (fspec_eoff && (fspec[fspec_eoff] == '/'))
        fspec[fspec_eoff] = '\0';
    else
        fspec_eoff = 0;         /* Just forget it... */
    fixup_fname(fspec);
/* fspec may have been expanded in situ by fixup_fname, so we can't just
 * use fspec_eoff as the place to put put back a '/'!
 */
    if (fspec_eoff) strcat(fspec, "/");
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
    if (dirptr == NULL) return NULL;

    piclen = strlen(picture);
    allfiles = (piclen == 0);

/* And return the first match (we return ONLY matches, for speed) */
    return getnfile();
}

/* getnbuffer() and getfbuffer()
 * Handle buffer name completion
 * Also handles userproc name completion, as those are just buffers with
 * names starting '/' and a type of BTPROC.
 * and phonetic translation table name completion, which are just buffers
 * with names starting '/' and a type of BTPHON.
 */

static char *getnbuffer(char *bpic, int bpiclen, enum cmplt_type mtype) {
    char *retptr;

    if (expandbp) {
/* We NEVER return minibuffer buffers (//minibnnnn), and we return internal
 * /xx buffers only if the user asked for them by specifying a picture
 * starting with /.
 * For a type of CMPLT_PROC we only consider buffer-names starting with '/'
 * with a b_type of BTPROC. We return the name *without* the leading '/'.
 */
        int offset;
            if ((mtype == CMPLT_PROC) ||
                (mtype == CMPLT_PHON)) offset = 1;
            else                       offset = 0;
        if ((mtype == CMPLT_PROC &&
              (expandbp->b_type != BTPROC || expandbp->b_bname[0] != '/' ||
               expandbp->b_bname[1] == '/')) ||     /* Catch //kbd_macro */
            (mtype == CMPLT_PHON &&
              (expandbp->b_type != BTPHON || expandbp->b_bname[0] != '/')) ||
            (strncmp(bpic, expandbp->b_bname + offset, bpiclen)) ||
            (!strncmp(expandbp->b_bname, "//minib", 7)) ||
            ((expandbp->b_bname[0] == '[') && bpiclen == 0)) {

            expandbp = expandbp->b_bufp;
            return getnbuffer(bpic, bpiclen, mtype);
        }
        else {
            retptr = expandbp->b_bname + offset;
            expandbp = expandbp->b_bufp;
            return retptr;
        }
    }
    else
        return NULL;
}

static char *getfbuffer(char *bpic, int bpiclen, enum cmplt_type mtype) {
    expandbp = bheadp;
    return getnbuffer(bpic, bpiclen, mtype);
}

/* getnname() and getfname()
 * Handle internal command name completions.
 *
 * Just uses the sorted index to step through the names in order.
 * So for getfname() we just set things up then call getnname(), and
 * that keeps going until it finds the first match, if there.
 * On successive getnname() calls we return NULL as soon as we have no
 * match.
 */
static int n_nidx;
static char *getnname(char *name, int namelen) {
    int noname_on_nomatch = (n_nidx != -1);
    while ((n_nidx = nxti_name_info(n_nidx)) >= 0) {
        if (strncmp(name, names[n_nidx].n_name, namelen) == 0)
            return names[n_nidx].n_name;
        if (noname_on_nomatch) break;
    }
    return NULL;
}
static char *getfname(char *name, int namelen) {
    n_nidx = -1;                    /* First call has to find first match */
    char *res = getnname(name, namelen);
    return res;
}

/* getnvar() and getfvar()
 * Handle internal env/user variable completions.
 *
 * We build the env variable sort index in getfvar once
 * We build the use variable sort index in getfvar each time.
 * However iff the incoming text is blank we just want to show
 * $... %..., and if it doesn't start with $ or % we want show no match.
 */
extern int *envvar_index;
extern int *usrvar_index;
extern char *uvnames[];
extern struct evlist evl[];

/* Since we know the string will be copied immediately after return we
 * can just put it into a static buffer here
 */

static char *varcat(char pfx, char *name) {
    static char pfxd_name[NVSIZE + 1];
    pfxd_name[0] = pfx;
    strcpy(pfxd_name+1, name);
    return pfxd_name;
}

static int nul_state;   /* 0 = doing $, 1 = doing %, 2 = done */
static int n_evidx, n_uvidx;
static char *getnvar(char *name, int namelen) {
    if (namelen == 0) {
        if (nul_state == 1) {
            nul_state = 2;
            return "%...";
        }
        else {
            return NULL;
        }
    }
    namelen--;      /* We don't compare the leading $ or % */
    char *mp = name + 1;
    if (name[0] == '$') {
/* -1 at end of list */
        while ((n_evidx = nxti_envvar(n_evidx)) >= 0) {
            if (strncmp(mp, evl[n_evidx].var, namelen) == 0)
                return varcat('$', evl[n_evidx].var);
        }
    }
    if (name[0] == '%') {
        while ((n_uvidx = nxti_usrvar(n_uvidx)) >= 0) {
            if (strncmp(mp, uvnames[n_uvidx], namelen) == 0)
                return varcat('%', uvnames[n_uvidx]);
        }
    }
    return NULL;
}

static char *getfvar(char *name, int namelen) {
    nul_state = 0;
    if (namelen == 0) {
        nul_state = 1;
        return "$...";
    }
/* Set up sort index and array */
    if (name[0] == '$') {
        if (envvar_index == NULL) init_envvar_index();
        n_evidx = -1;
    }
    if (name[0] == '%') {
        sort_user_var();
        n_uvidx = -1;
    }
    return getnvar(name, namelen);
}

/* matcher()
 * A routine to do the matching for completion code.
 * The routine to get the next item to check is determined by mtype.
 */
static int matcher(char *name, int namelen, char *choices,
      enum cmplt_type mtype) {
    char *next;                 /* Next filename to look at */
    int match_length;           /* Maximal match length     */
    char *p, *q;                /* Handy pointers           */
    int max, l, unique;

    match_length = strlen(so_far);

/* Restrict length of returned string to number of columns, so that we
 * don't end up wrapping in the minibuffer line.
 */
    max = term.t_ncol - 1;

    l = (match_length < max) ? match_length : max;
/* We also need to check we are not going to overflow the
 * destination buffer, and we have to allow for the final NUL
 */
    if (l >= NFILEN) l = NFILEN - 1;
    so_far[l] = '\0';           /* Ensure this will fit into choices */
    strcpy(choices, so_far);
    max -= l;
    unique = TRUE;
    while (1) {             /* Rather than try to put the switch in there. */
        switch(mtype) {
        case CMPLT_FILE:
            next = getnfile();
            break;
        case CMPLT_BUF:
        case CMPLT_PROC:
        case CMPLT_PHON:
            next = getnbuffer(name, namelen, mtype);
            break;
        case CMPLT_NAME:
            next = getnname(name, namelen);
            break;
        case CMPLT_VAR:
            next = getnvar(name, namelen);
            break;
        default:            /* If mtype arrives oddly? */
            next = NULL;
        }
        if (next == NULL) break;

        unique = FALSE;
/* This loop updates the maximum common prefix "so far" by comparing
 * it with this latest match.
 */
        for (p = so_far, q = next, match_length = 0;
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
    return unique;
}

/* Entry point for filename completion
 * Looks for things starting with name, and if there are multiple choices
 * builds a catenated string of them in choices.
 */
static int comp_file(char *name, char *choices) {
    char supplied[NFILEN];  /* Maximal match so far */
    char *p;                /* Handy pointer */
    int unique;

    *choices = 0;

    if ((p = getffile(name)) == NULL) {
        close_dir();
        mlwrite(NOMATCH);
        sleep(1);
        return FALSE;
    }
    else
        strcpy(so_far, p);

    strcpy(supplied, name);
    unique = matcher(name, 0, choices, CMPLT_FILE);
    close_dir();

    if (directory[0]) {
        strcpy(name, directory);
        strcat(name, so_far);
    }
    else {
        strcpy(name, so_far);
    }

/* If we found a single entry that begins with '~' and contains no '/'
 * pass it to fixup_fname() so that it can expand any login id to its
 * HOME dir. This mean that ~id gets expanded to $HOME, rather than just
 * looping with ~id in the buffer.
 * If this does happen (the leading ~ goes away) we append a '/'.
 */
    if (unique) {
        if ((name[0] == '~') && (!strchr(name, '/'))) {
            fixup_fname(name);
            if (name[0] != '~') strcat(name, "/");
        }
    }
    return TRUE;
}

/* Generic entry point for completions where the getf* function selects
 * names from an internal list.
 * Use by mtype CMPLT_BUF/CMPLT_PROC and CMPLT_NAME.
 * The getf* function to use is determined by mtype.
 * Looks for things starting with name, and if there are multiple choices
 * builds a catenated string of them in choices.
 */
static int comp_gen(char *name, char *choices, enum cmplt_type mtype) {
    char supplied[NFILEN];  /* Maximal match so far */
    char *p;                /* Handy pointer */
    int namelen;

    *choices = 0;
    namelen = strlen(name);
    switch(mtype) {
    case CMPLT_BUF:
    case CMPLT_PROC:
    case CMPLT_PHON:
        p = getfbuffer(name, namelen, mtype);
        break;
    case CMPLT_NAME:
        p = getfname(name, namelen);
        break;
    case CMPLT_VAR:
        p = getfvar(name, namelen);
        break;
    default:            /* If mtype arrives oddly? */
        p = NULL;
    }
    if (p == NULL) {
        mlwrite(NOMATCH);
        sleep(1);
        return FALSE;
    }
    strcpy(so_far, p);
    strcpy(supplied, name);
    (void)matcher(name, namelen, choices, mtype);
    strcpy(name, so_far);

    return TRUE;
}

/* Entry point for buffer name completion. And userprocs.
 * Front-end to comp_gen().
 */
static int comp_buffer(char *name, char *choices, enum cmplt_type mtype) {
    return comp_gen(name, choices, mtype);
}

/* Entry point for internal command name completion
 * Front-end to comp_gen().
 */
static int comp_name(char *name, char *choices) {
    return comp_gen(name, choices, CMPLT_NAME);
}

/* Entry point for internal environment/user var completion
 * Front-end to comp_gen().
 */
static int comp_var(char *name, char *choices) {
    return comp_gen(name, choices, CMPLT_VAR);
}


/* Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. Used any time a confirmation is required.
 */
int mlyesno(char *prompt) {
    int c;                  /* input character - tgetc() returns unicode */
    int res = -1;           /* NOT ABORT, TRUE or FALSE */
    while(res == -1) {
        mlwrite("%s%s", prompt, " " MLbkt("y/n") "? ");

        c = get1key();        /* get the response */
        switch(c) {
        case 'y':
        case 'Y':
            res = TRUE;
            break;
        case 'n':
        case 'N':
            res = FALSE;
            break;
        default:
            if (c == abortc) res = ABORT;   /* as abortc is a var */
            break;
        }
    }
    mlerase();
    return res;
}

/* Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 * NOTE that when the function is processing macros from a (start-up) file
 * it will get one arg at a time.
 * BUT when run interactively it will return the entire response.
 * So macro-file args need to be quoted...
 * We pass on any expansion-type requested (for, eventually, getstring()).
 */

int mlreply(char *prompt, char *buf, int nbuf, enum cmplt_type ctype) {
    return nextarg(prompt, buf, nbuf, ctype);
}

/* get a command name from the command line. Command completion means
 * that pressing a <TAB> will attempt to complete an unfinished command
 * name if it is unique.
 */
struct name_bind *getname(char *prompt, int new_command) {
    char buf[NSTRING];      /* buffer to hold tentative command name */

/* First get the name... */
    if (clexec == FALSE) {
        no_macrobuf_record = 1;
        int status = mlreply(prompt, buf, NSTRING, CMPLT_NAME);
        no_macrobuf_record = 0;
        if (status != TRUE) return NULL;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, buf, NSTRING - 1);
   }

/* Now check it exists */

    struct name_bind *nbp = name_info(buf);
    if (nbp) {
        if (kbdmode == RECORD) addto_kbdmacro(buf, new_command, 0);
        return nbp;
    }
    mlwrite("No such function: %s", buf);
    return NULL;
}

/* tgetc:   Get a key from the terminal driver.
 *          Resolve any keyboard macro action.
 */
int tgetc(void) {
    int c;                      /* fetched character */

/* If we are playing a keyboard macro back, */
    if (kbdmode == PLAY) {
        if (kbdptr < kbdend)    /* if there is some left... */
            return (int) *kbdptr++;
        if (--kbdrep < 1) {     /* at the end of last repetition? */
            kbdmode = STOP;     /* Leave ctlxe_togo alone */
            if (!vismac) update(FALSE); /* force update now all is done? */
        }
        else {
            kbdptr = kbdm;      /* reset macro to beginning for the next rep */
            f_arg = p_arg;      /* Restore original f_arg */
            return (int) *kbdptr++;
        }
    }

/* Fetch a character from the terminal driver */
    struct sigaction sigact;
    if (remap_c_on_intr) {
        sigaction(SIGWINCH, NULL, &sigact);
        sigact.sa_flags = 0;
        sigaction(SIGWINCH, &sigact, NULL);
        errno = 0;
    }
    c = TTgetc();
    if (remap_c_on_intr) {
        sigaction(SIGWINCH, NULL, &sigact);
        sigact.sa_flags = SA_RESTART;
        sigaction(SIGWINCH, &sigact, NULL);
    }
/* Record it for $lastkey */
    lastkey = c;

    if (c == 0 && errno == EINTR && remap_c_on_intr)
        c = UEM_NOCHAR;         /* Note illegal char */
    else
/* Save it if we need to */
        if (kbdmode == RECORD) {
            *kbdptr++ = c;
            kbdend = kbdptr;
            if (kbdptr == &kbdm[NKBDM - 1]) {   /* don't overrun buffer */
                kbdmode = STOP; /* Must be collecting - leave ctlxe_togo */
                TTbeep();
            }
        }

/* And finally give the char back */
    return c;
}

/* get1key: Get one keystroke.
 *          The only prefixes legal here are SPEC and CONTROL.
 */
int get1key(void) {
    int c;

    c = tgetc();                    /* get a keystroke */
    if (c >= 0x00 && c <= 0x1F)     /* C0 control -> C-     */
        c = CONTROL | (c + '@');
    return c;
}

/* Force a char to uppercase
 * We expect c to mostly be ASCII, so shortcut that case
 * Used several times in getcmd
 */
static int ensure_uppercase(int c) {
    if (c <= 0x7f) {
        if (c >= 'a' && c <= 'z') c ^= DIFCASE;
    }
    else {
        c = utf8proc_toupper(c);    /* Unchanged if not lower/title case */
    }
    return c;
}

/* getcmd: Get a command from the keyboard.
 *         Process all applicable prefix keys
 */

#define CSI 0x9b

int getcmd(void) {
    int c;                  /* fetched keystroke */
    int ctlx = FALSE;
    int meta = FALSE;
    int cmask = 0;

/* Keep going until we return something */

    while ((c = get1key())) {       /* Extra ()s for gcc warniing */
/* Esc-O is really SS3, but cursor keys often send Esc-O A|B|C|D so it
 * helps to add to in here and treat it as CSI.
 * This does mean that if you bind a function to Esc-O you have to use
 * o (lower-case) to invoke it.
 */
        if (meta && ((c == '[') | (c == 'O'))) {
            meta = FALSE;
            break;              /* Drop out to CSI handling */
        }
        if (c == CSI) break;    /* Drop out to CSI handling */
        if (c == (CONTROL|'X')) {
/* Trap Esc-^x and ^x-^x */
            if (meta) return META|CONTROL|'X';
            if (ctlx) return CTLX|CONTROL|'X';
            ctlx = TRUE;
            continue;
        }
        if (c == (CONTROL|'[')) {
/* Trap Esc-Esc. Add in Ctlx prefix, if set */
            if (meta) {
                if (ctlx) return CTLX|META|CONTROL|'[';
                else      return META|CONTROL|'[';
            }
            meta = TRUE;
            continue;
        }
        if (meta) cmask |= META;
        if (ctlx) cmask |= CTLX;
        if (cmask || (c&(CONTROL|META|CTLX))) c = ensure_uppercase(c);
        return c | cmask;
    }

/* Process the Vt220 Control Sequence Introducer (CSI) from here on.
 * Once we get here the control mask is already known, so
 * set it now.
 * NOTE that if META is set we return with 0.
 * This is because a lot of CSI will start with Esc[ (unless it sends
 * the 8-bit 0x9b) and we trap Esc-Esc.
 * stock() ALSO prevents it.
 * Thus META|SPEC can never be set by the user, and the internal handlers
 * are safe from being overwritten by a key binding.
 */
    if (meta) return 0;
    cmask = SPEC;
    if (ctlx) cmask |= CTLX;
    c = get1key();

/* If the next key is '[', just get the next key and return
 * FNa for A, etc....
 * "Linux console" for KDE sends Esc[[A... for F1-F5. Only known instance.
 * Use a quick lowercase...
 */
    if (c == '[') return (cmask | DIFCASE | get1key());

/* uEmacs/PK 4.0 (4.015) from Petri H. Kutvonen, University of Helsinki,
 * which was an "enhanced version of MicroEMACS 3.9e" contained special
 * handling for 'i' and 'c' in this block.
 * However, the code always resulted in returning SPEC | '?' | cmask
 * because c had been over-ridden by the time it was checked again.
 * From looking at ebind.h (FNc and FNi are bound to metafn and cex
 * respectively, both null functions) it is clear that the intention
 * was that they should be treated as Meta and Ctlx prefixes, while
 * just restarting the key input.
 * But since I can't see it being of use (it prevented you binding
 * them to anything else) I've removed it.
 * As a result this code had been moved to follow the loop, since it no
 * longer needs to be within it.
 */

/* If this char is from A to z, just return it */

    if (c >= 'A' && c <= 'z') return cmask | c;

/* Get the next char. If it is ~, return that prev char */

    int d = get1key();          /* ESC [ n ~   P.K. */
    if (d == '~') return cmask | c;

/* If we have 2 digits, all is OK(-ish). If the second is not a digit, but K,
 * return FNk.
 * This handles Shift F2 in Konsole Xfree mode sending CSI2Q,etc..
 */
    if ((d < '0') || (d > '9')) return cmask | d;

/* We should now have a tilde to finish after this second. So get to it
 * - with a limit...
 * The KDE Konsole Xfree keyboard sends CSI15;*~ for F5+Modifier, (CSI15~
 * for bare F5) etc. so we'll just skip the modifier part.
 *
 * It would also be possible to handle the CSI224z, CSI225z, etc sent by
 * F1, F2, ... in xterm's Sun Function-Keys mode.  But there are limits as
 * to what is useful.
 */
    for (int sc = 4; sc > 0; sc--) {
        if (get1key() == '~') break;
    }

/* Might as well return SPEC a-t for what the function keys send.
 * (The Linux Console Fx keys send from 11~ up to 34~ (Shift-F8))
 *
 * The VT220 function keys behaved thus:
 * F1 to F5 - local keys, sent nothing (we can map to FN1..5).
 * F6 sent (CSI)17~, F7 (CSI)18~ etc.
 * There were gaps after 10 (F11 sent (CSI)22~), 14 and 16.
 *
 *  https://vt100.net/docs/vt220-rm/chapter3.html
 *  https://www.xfree86.org/current/ctlseqs.html
 *
 * By mapping them to FNx (x == lowercase) we avoid clashes with the
 * cursor keys (EscO or Esc[ A/B/C/D) and Insert/Home/PgUp (Esc[n~ n == 1-6).
 *
 * KDE Konsole with Xfree4 and macOS settings send EscOP/Q/R/S for F1/2/3/4
 * which will map to FNP/Q/R/S. gnome-terminal does the same.
 */
    int num = (c-'0')*10 + (d-'0');
    switch (num) {          /* ESC [ n n ~ P.K. */
/* It is possible to set up case statements with fall through adjusting
 * an offset from num as you go.
 * But it is somewhat confusing, and gains little, if anything.
 * Ignore it.
 */
    case 11: c = 'a'; break;
    case 12: c = 'b'; break;
    case 13: c = 'c'; break;
    case 14: c = 'd'; break;
    case 15: c = 'e'; break;
    case 17: c = 'f'; break;        /* Skip 16 */
    case 18: c = 'g'; break;
    case 19: c = 'h'; break;
    case 20: c = 'i'; break;
    case 21: c = 'j'; break;
    case 23: c = 'k'; break;        /* Skip 22 */
    case 24: c = 'l'; break;
    case 25: c = 'm'; break;
    case 26: c = 'n'; break;
    case 28: c = 'o'; break;        /* Skip 27 */
    case 29: c = 'p'; break;
    case 31: c = 'q'; break;        /* Skip 30 */
    case 32: c = 'r'; break;
    case 33: c = 's'; break;
    case 34: c = 't'; break;
    default: c = '?'; break;
    }
    return cmask | c;
}

/* GGR
 * A version of getstring in which the minibuffer is a true buffer!
 * Note that this loops for each character, so you can manipulate the
 * prompt etc. by providing updated info and using it after the
 * loop: label.
 */

/* If the window size changes whilst this is running the display will end
 * up incorrect (as the code reckons we are in this buffer, not the one we
 * arrived from).
 * So set up a SIGWINCH handler to get us out the minibuffer before
 * display::newscreensize() runs.
 */

static struct window *mb_winp = NULL;

typedef void (*sighandler_t)(int);

static void sigwinch_handler(int signr) {

    UNUSED(signr);

/* We need to get back to how things were before we arrived in the
 * minibuffer.
 * So we save the current settings, restore the originals, let the
 * resize code run, re-fetch the original (in case they have changed)
 * the restore the ones we arrived with.
 */
    struct buffer *mb_bp = curbp;
    struct window *mb_wp = curwp;
    struct window *mb_hp = wheadp;

    curbp = mb_info.main_bp;
    curwp = mb_info.main_wp;
    wheadp = mb_info.wheadp;
    inmb = FALSE;

/* Do the bits from sizesignal() in display.c that do the work */
    int w, h;

    getscreensize(&w, &h);
    if (h && w && (h != term.t_nrow || w != term.t_ncol))
        newscreensize(h, w, 0);

/* Need to reget the mb_info data now */

    mb_info.main_bp = curbp;
    mb_info.main_wp = curwp;
    mb_info.wheadp = wheadp;

/* Now get back to how we arrived */

    curbp = mb_bp;
    curwp = mb_wp;
    wheadp = mb_hp;
    curwp->w_toprow = term.t_mbline;    /* Set new value */
    inmb = TRUE;
/* Ensure the minibuffer is redrawn */
    mbupdate();

    return;
}

/* The actual getstring() starts now... */

int getstring(char *prompt, char *buf, int nbuf, enum cmplt_type ctype) {
    struct buffer *bp;
    struct buffer *cb;
    char mbname[NBUFN];
    int    c;               /* command character */
    struct line *lp;        /* line to copy */
    int size;
    char *sp;               /* string pointer into line */
    char tstring[NSTRING];
    char choices[1000];     /* MUST be > max likely window width */
    int status;
    int do_evaluate = FALSE;
    func_arg sav;

    short savdoto;
    int prolen;
    char procopy[NSTRING];

/* gcc 4.4.7 is happy to report "may be used initialized" for wsave
 * (which is wrong) even though it doesn't have a -Wmaybe-uninitialized
 * option.
 * Since we aren't using -Winit-self we can initialize it to itself
 * and the optimizer will optimize it away (!?!).
 * From Flexo's answer at:
 *   https://stackoverflow.com/questions/5080848/disable-gcc-may-be-used-uninitialized-on-a-particular-variable
 */
    struct window wsave = wsave;

/* We need to block SIGWINCH until we have set-up all of the variables
 * we need after the longjmp.
 */
    sigset_t sigwinch_set, incoming_set;
    sigemptyset(&sigwinch_set);
    sigaddset(&sigwinch_set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &sigwinch_set, &incoming_set);

/* Create a minibuffer window for use by all minibuffers */
    if (!mb_winp) {
        mb_winp = (struct window *)Xmalloc(sizeof(struct window));
        mb_winp->w_wndp = NULL;               /* Initialize window    */
#if COLOR
/* initialize colors to global defaults */
        mb_winp->w_fcolor = gfcolor;
        mb_winp->w_bcolor = gbcolor;
#endif
        mb_winp->w_toprow = term.t_mbline;
        mb_winp->w_ntrows = 1;
        mb_winp->w.fcol = 0;
        mb_winp->w_force = 0;
        mb_winp->w_flag = WFMODE | WFHARD;    /* Full.                */
    }

/* Expansion commands leave junk in mb */

    mlerase();
    sprintf(mbname, "//minib%04d", mb_info.mbdepth+1);
    cb = curbp;

/* Update main screen before entering minibuffer */

    update(FALSE);

/* Set-up the (incoming) prompt string and clear any prompt update flag,
 * as it should only get set during loop.
 * This means we can *now* initialize an empty return buffer
 * GGR NOTE!!
 * some callers use the return buffer as a temporary buffer for the prompt!
 */
    strcpy(procopy, prompt);
    prolen = strlen(procopy);
    prmpt_buf.update = 0;
    *buf = '\0';            /* Ensure we never return garbage */

    if ((bp = bfind(mbname, TRUE, BFINVS)) == NULL) {
        sigprocmask(SIG_SETMASK, &incoming_set, NULL);
        return FALSE;
    }

/* Save real reexecution history */

    sav = f_arg;

/* Remember the original buffer info if at level 1
 * Get the PHON state from the current buffer, so we can carry it to
 * the next minibuffer.
 * We *don't* carry back any change on return!
 */
    int new_bmode;
    if (curbp->b_mode & MDPHON) new_bmode = MDEXACT | MDPHON;
    else                        new_bmode = MDEXACT;

    if (++mb_info.mbdepth == 1) {   /* Arrival into minibuffer */
        mb_info.main_bp = curbp;    /* For main buffer info in modeline */
        mb_info.main_wp = curwp;    /* Used to position modeline */
        mb_info.wheadp = wheadp;    /* ??? */
        inmb = TRUE;
    }
    else {                          /* Save this minibuffer for recursion */
        wsave = *curwp;             /* Structure copy... */
    }
/* Switch to the minibuffer window */
    curwp = mb_winp;
    wheadp = curwp;

    if (mpresf) mlerase();
    mberase();

    if (!swbuffer(bp, 0)) return FALSE;

    curwp->w_toprow = term.t_mbline;
    curwp->w_ntrows = 1;
    curbp->b_mode = new_bmode;

/* The oldact is restored on exit. */
    struct sigaction sigact, oldact;
    sigact.sa_handler = sigwinch_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sigact, &oldact);
/* Now we can enable the signal */
    sigprocmask(SIG_SETMASK, &incoming_set, NULL);

/* A copy of the main.c command loop from 3.9e, but things are a
 *  *little* different here..
 *
 * We start by ensuring that the minibuiffer display is empty...
 */
loop:

/* Execute the "command" macro...normally null */
    saveflag = lastflag;        /* preserve lastflag through this */
/* Don't start the handler when it is already running as that might
 * just get into a loop...
 */
    if (!meta_spec_active.C) {
        meta_spec_active.C = 1;
        execute(META|SPEC|'C', FALSE, 1);
        meta_spec_active.C = 0;
    }
    lastflag = saveflag;

/* Have we been asked to update the prompt? */

    if (prmpt_buf.update) {
        strcpy(procopy, prmpt_buf.prompt);
        prolen = strlen(procopy);
        prmpt_buf.update = 0;
    }

/* Have we been asked to load a (search/replace) string?.
 * If so, insert it into our buffer (which is the result buffer) now,
 * which inserts it at the "current location".
 */
    if (prmpt_buf.preload) {
        linstr(prmpt_buf.preload);
        prmpt_buf.preload = NULL;    /* One-time usage */
    }

/* Prepend the prompt to the beginning of the visible line (i.e.
 * ahead of any preload.
 */
    savdoto = curwp->w.doto;
    curwp->w.doto = 0;
    linstr(procopy);
    curwp->w.doto = savdoto + prolen;

/* Fix up the screen - we do NOT want horizontal scrolling in the mb */

    int real_hscroll;
    real_hscroll = hscroll;
    hscroll = FALSE;
    curwp->w_flag |= WFMODE;    /* Need to update the modeline... */
    mbupdate();                 /* Will set modeline to prompt... */
    hscroll = real_hscroll;

/* Remove the prompt from the beginning of the buffer for the visible line.
 * This is so that the buffer contents at the end contain just the response.
 */
    curwp->w.doto = 0;
    ldelete((long)prolen, FALSE);
    curwp->w.doto = savdoto;

/* Get the next command (character) from the keyboard */

    remap_c_on_intr = 1;
    c = getcmd();
/* We get UEM_NOCHAR back on a sigwin signal. */
    remap_c_on_intr = 0;
    if (c == UEM_NOCHAR) goto loop;

/* Check for any numeric prefix
 * This looks for Esc<n> prefixes and returns c/f/n in carg.
 * The "c" arg may be the start of a numeric prefix (e.g.Esc2) so
 * from here on it's carg->c that needs to be checked.
 */
    com_arg *carg = multiplier_check(c);

/* Various completion code options
 * Usually a list of matches is temporarily displayed in the minibuffer.
 * THIS ONLY USES THE TEXT ON THE CURRENT LINE, and uses ALL OF IT.
 * (The CMPLT_SRCH handling is a little different. Not really a
 * completion, but it fits nicely into this placement.
 */
    if (carg->c == (CONTROL|'I')) {
        if (ctype == CMPLT_SRCH) {
            rotate_sstr(carg->n);
            goto loop;
        }
        lp = curwp->w.dotp;
        sp = lp->l_text;
/* NSTRING-1, as we need to add a trailing NUL */
        if (lp->l_used < NSTRING-1) {
            int expanded;
            memcpy(tstring, sp, lp->l_used);
            tstring[lp->l_used] = '\0';
            switch(ctype) {
            case CMPLT_FILE:
                expanded = comp_file(tstring, choices);
                break;
            case CMPLT_BUF:
            case CMPLT_PROC:
            case CMPLT_PHON:
                expanded = comp_buffer(tstring, choices, ctype);
                break;
            case CMPLT_NAME:
                expanded = comp_name(tstring, choices);
                break;
            case CMPLT_VAR:
                expanded = comp_var(tstring, choices);
                break;
            default:            /* Do nothing... */
                expanded = 0;
            }
            if (expanded) {
                savdoto = curwp->w.doto;
                curwp->w.doto = 0;
                ldelete((long) lp->l_used, FALSE);
                linstr(tstring);
/* Don't bother with this when playing a macro - that just results
 * in an unnecessary pause from the sleep().
 */
                if (choices[0] && (kbdmode != PLAY)) {
                    mlwrite_one(choices);
                    size = (strlen(choices) < 42) ? 1 : 2;
                    sleep(size);
                    mlerase();
                    mberase();
                }
            }
            else
                TTbeep();
            goto loop;
        }
        else {
            TTbeep();
            goto loop;
        }
    }

/* Some further "hard-wired" key-bindings - aka minibuffer specials. */

    do_evaluate = FALSE;
    switch(carg->c) {           /* The default is to do nothing here */
    case META|CONTROL|'I':      /* Only act for CMPLT_SRCH */
        if (ctype == CMPLT_SRCH) rotate_sstr(-(carg->n));
        goto loop;
    case CTLX|CONTROL|'I':      /* Only act for CMPLT_SRCH */
        if (ctype == CMPLT_SRCH) select_sstr();
        goto loop;
    case CTLX|CONTROL|'M':      /* Evaluate before return */
        do_evaluate = TRUE;
        goto submit;
    case CONTROL|'M':           /* General */
    case CTLX|CONTROL|'C':
        goto submit;
    case CONTROL|'G':           /* General */
        status = ABORT;
        goto abort;
    }

/* ...and execute the command. DON'T MACRO_CAPTURE THIS!!
 * No need to mlerase any numeric arg as either:
 *  o the command will edit the minibuffer, which will be redrawn, or
 *  o the command will execute, so the minibuffer will be erased anyway.
 * However, we do need to unset the flag saying there is something there
 * so that we don't have to wait on things like Esc2a (== "aa").
 */
    mpresf = FALSE;
    execute(carg->c, carg->f, carg->n);
    if (mpresf) {
        sleep(1);
        mlerase();
        mberase();
    }

/* Whatever dumb modes, they've put on, allow only the sensible... */

    curbp->b_mode &= (MDEXACT|MDOVER|MDMAGIC|MDPHON|MDEQUIV|MDRPTMG);
    goto loop;

submit:     /* Tidy up */
    status = TRUE;

/* Find the contents of the current buffer, and its length, including
 * newlines to join lines, but excluding the final newline.
 * Only do this upto the requested return length.
 */

    struct line *mblp = lforw(bp->b_linep);
    int sofar = 0;
    int maxadd = nbuf - 1;
    while (mblp != bp->b_linep && sofar < maxadd) {
        if (sofar != 0) buf[sofar++] = '\n';    /* Add NL if not first */
        int add = llength(mblp);
        if ((sofar + add) > maxadd) add = maxadd - sofar;
        memcpy(buf+sofar, mblp->l_text, add);
        sofar += add;
        mblp = lforw(mblp);
    }
    buf[sofar] = '\0';          /* Add the NUL */

/* Need to copy to return buffer and, if not empty,
 * to save as last minibuffer.
 */
    int retlen = sofar;         /* Without terminating NUL */
    if (retlen) {
        if (retlen >= NSTRING) retlen = NSTRING - 1;
        addto_lastmb_ring(buf); /* Terminating NULL is actually there */
    }
    else status = FALSE;        /* Empty input... */

/* Record the result if we are recording a keyboard macro, but only
 * at first level of the minibuffer (i.e. the "true" result),
 * and only if we have some text.
 */
    if ((*buf != '\0') && (kbdmode == RECORD) &&
        (mb_info.mbdepth == 1) && !no_macrobuf_record)
         addto_kbdmacro(buf, 0, !do_evaluate);

/* If we have to evaluate, do it now.
 * Note that this is *AFTER* we've any logging to the macro.
 * We have to fudge buf into execstr for function evaluating to work.
 */
    if (do_evaluate) {
        char *orig_execstr = execstr;   /* Just in case... */
        char tok1[NLINE], tres[NLINE];
        execstr = buf;
        execstr = token(execstr, tok1, NLINE);
/* GGR - There is the possibility of an illegal overlap of args here.
 *       So it must be done via a temporary buffer.
 */
           strcpy(tres, getval(tok1));
           strcpy(buf, tres);
           execstr = orig_execstr;
    }
abort:

/* If we get here "normally" SIGWINCH will still be enabled, so we need
 * to block it while we tidy up, otherwise we might run through this end
 * code twice.
 */
    sigprocmask(SIG_BLOCK, &sigwinch_set, NULL);

    if (!swbuffer(bp, 0))   /* Make sure we're still in our minibuffer */
        return FALSE;
    unmark(TRUE, 1);

    if (--mb_info.mbdepth) {        /* Back to previous mini-buffer */
        if (!swbuffer(cb, 0))       /* Switch to its buffer... */
            return FALSE;
        *curwp = wsave;             /* ...and its window info */
    }
    else {                              /* Leaving minibuffer */
        if (!swbuffer(mb_info.main_bp, 0))  /* Switch back to main buffer */
            return FALSE;
        curwp = mb_info.main_wp;        /* Reset window info ptr */
        wheadp = mb_info.wheadp;        /* Reset window list */
        inmb = FALSE;                   /* Note we have left mb */
    }
    curwp->w_flag |= WFMODE;        /* Forces modeline redraw */
    zotbuf(bp);                     /* Remove this minibuffer */

/* Restore real reexecution history */

    f_arg = sav;

    mberase();
    if (status == ABORT) {
        ctrlg(FALSE, 0);
        TTflush();
    }

/* We need to re-instate the original handler now... */
    sigaction(SIGWINCH, &oldact, NULL);
    sigprocmask(SIG_SETMASK, &incoming_set, NULL);

    return status;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_input(void) {
    if (mb_winp) Xfree(mb_winp);
    return;
}
#endif
