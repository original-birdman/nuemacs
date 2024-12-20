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

#define INPUT_C

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

static db_strdef(picture);
static db_strdef(directory);

static struct buffer *expandbp;

/* Static variable shared by comp_file(), comp_buff() and matcher() */

static db_strdef(so_far);   /* Maximal match so far */

/* getnfile() and getffile()
 * Handle file name completion
 *
 * Here is what getffile() should do.
 * o Parse out any directory and file parts.
 * o Open that directory, if necessary.
 * o Build a wildcard from the file part.
 * o Call getnfile() to return the first match.
 * o If the system is not case-sensitive, force lower case.
 */
#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>

static DIR *dirptr;
static short allfiles;
static db_strdef(fullname);

static void close_dir(void) {
    if (dirptr != NULL) {
        closedir(dirptr);
        dirptr = NULL;
    }
}

#ifndef STANDALONE
/* We need to remember these across successive calls to getnfile
 * 40 chars should be enough for login names.
 */
static char exp_id[40];
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
            snprintf(exp_id, sizeof(exp_id), "~%s", pwe->pw_name);
            return exp_id;
        }
    }
    if (errno) mlwrite("passwd lookup: %s", strerror(errno));
    endpwent();
    return NULL;
}
#endif

/* What to show in the minibuffer if completion doesn't find anything */

#define NOMATCH "No match"

/* Get the next matching file name
 */
static db_strdef(suggestion);
static const char *getnfile(void) {
    unsigned short type;    /* file type */

/* Handle ~xxx by looking up all matching login ids.
 * NOTE that ~xxx/ has specified a full id, and so is handled by
 * fixup_fname() when we drop into the filename code.
 */
#ifndef STANDALONE
    if (run_id_finder) return id_finder();
#endif

    struct dirent *dp;
    struct stat statbuf;

/* Get the next directory entry, if no more, we finish */
    if ((dp = readdir(dirptr)) == NULL) return NULL;

    if ((allfiles ||
         ((strlen(dp->d_name) >= db_len(picture)) &&
         (!strncmp(dp->d_name, db_val(picture), db_len(picture))))) &&
         (strcmp(dp->d_name, ".")) &&
         (strcmp(dp->d_name, ".."))) {
/* catenate fullname and d_name in glb_db */
        db_set(glb_db, db_val(fullname));
        db_append(glb_db, dp->d_name);
        stat(db_val(glb_db), &statbuf);
        type = (statbuf.st_mode & S_IFMT);
        if ((type == S_IFREG)
#ifdef S_IFLNK
             || (type == S_IFLNK)
#endif
             || (type == S_IFDIR)) {
            db_set(suggestion, dp->d_name);
            if (type == S_IFDIR) db_append(suggestion, "/");
            return db_val(suggestion);
        }
    }
    return getnfile();
}

/* Get the first matching filename.
 * Actually just sets things up so that getnfile() or id_finder()
 * can get it...
 */
static const char *getffile(const char *fspec_in) {
    char *p;                /* handy pointers */

    dirptr = NULL;          /* Initialise things */

/* "'getpwent' in statically linked applications requires at runtime the
 * shared libraries from the glibc version used for linking"
 * But that can't happen and in the likely setting for a STANDALONE
 * version you won't be trying to lookup by id.
 */
#ifndef STANDALONE

/* Handle ~xxx by looking up all matching login ids.
 * NOTE that ~xxx/ has specified a full id, and that is handled by
 * fixup_fname().
 */
    if ((*fspec_in == '~') && (!strchr(fspec_in, '/'))) {
        strncpy(id_to_find, fspec_in+1, 39);
        terminate_str(id_to_find+39);   /* Ensure NUL terminated */
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
#endif

/* From here on we need an editable copy */

    char *fspec = strdupa(fspec_in);

/* fixup_fname will strip any trailing '/'
 * This leads to a "loop", as the rest of this code would just add it back
 * and so we never get to the point of looking for entries within it.
 * So, if one is there on entry, put it back after fixup_fname does its work.
 * NOTE: that we check for the end offset being > 0, as that means "/" isn't
 * treated as a trailing slash.
 */
    int fspec_eoff = strlen(fspec) - 1;
    if (fspec_eoff && (fspec[fspec_eoff] == '/'))
        terminate_str(fspec + fspec_eoff);
    else
        fspec_eoff = 0;         /* Just forget it... */
/* fixup_fname now expands into an internal buffer.
 * Which is fine for us - all we do is (perhaps) append "/" and move one...
 * We can't just use fspec_eoff as the place to put put back a '/' as
 * the length may have changed!
 */
    db_set(glb_db, fixup_fname(fspec));
    if (fspec_eoff) db_addch(glb_db, '/');
    db_set(directory, db_val(glb_db));

    if ((p = strrchr(db_val(directory), '/'))) {
        p++;
        db_set(picture, p);
        *p = 0;
        db_set(fullname, db_val(directory));
    }
    else {
        db_set(picture, db_val(directory));
        db_set(directory, "");
        db_set(fullname,  "");
    }

    if (db_len(directory) == 0) dirptr = opendir(".");
    else                        dirptr = opendir(db_val(directory));
    if (dirptr == NULL) return NULL;

    allfiles = (db_len(picture) == 0);

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

static const char *getnbuffer(const char *bpic, int bpiclen,
     enum cmplt_type mtype) {
    const char *retptr;

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
               expandbp->b_bname[1] == '/')) || /* Catch //kbd_macro */
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
/* getfbuffer is now a #define */
#define getfbuffer(bpic, len, mtype) \
    (expandbp = bheadp, getnbuffer(bpic, len, mtype))

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
static const char *getnname(const char *name, int namelen) {
    int noname_on_nomatch = (n_nidx != -1);
    while ((n_nidx = nxti_name_info(n_nidx)) >= 0) {
        if (strncmp(name, names[n_nidx].n_name, namelen) == 0)
            return names[n_nidx].n_name;
        if (noname_on_nomatch) break;
    }
    return NULL;
}
/* First call has to find first match */
#define getfname(name, len) (n_nidx = -1, getnname(name, len))

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

static char *varcat(char pfx, const char *name) {
    static char pfxd_name[NVSIZE + 1];
    pfxd_name[0] = pfx;
    strcpy(pfxd_name+1, name);
    return pfxd_name;
}

static int nul_state;   /* 0 = doing $, 1 = doing %, 2 = done */
static int n_evidx, n_uvidx;
static const char *getnvar(const char *name, int namelen) {
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
    const char *mp = name + 1;
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

static const char *getfvar(const char *name, int namelen) {
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
static int matcher(const char *name, int namelen, db *choices,
      enum cmplt_type mtype) {
    const char *next;           /* Next filename to look at */
    int match_length;           /* Maximal match length     */
    const char *p, *q;          /* Handy pointers           */
    int max, l, unique;

    match_length = db_len(so_far);

/* Restrict length of returned string to number of columns, so that we
 * don't end up wrapping in the minibuffer line.
 */
    max = term.t_ncol - 1;

    l = (match_length < max) ? match_length : max;

/* We also need to check we are not going to overflow the
 * destination buffer, and we have to allow for the final NUL
 */
    dbp_set(choices, db_val(so_far));
    max -= l;
    unique = TRUE;
    while (1) {         /* Rather than try to put the switch in there. */
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
        default:        /* If mtype arrives oddly? */
            next = NULL;
        }
        if (next == NULL) break;

        unique = FALSE;
/* This loop updates the maximum common prefix "so far" by comparing
 * it with this latest match.
 */
        for (p = db_val(so_far), q = next, match_length = 0;
            (*p && *q && (*p == *q)); p++, q++)
            match_length++;
        db_setcharat(so_far, match_length, 0);

        l = strlen(next);
        if (max == 0) {
            if (match_length == 0)
                break;
        }
        else if (l < max) {
            dbp_append(choices, " ");
            dbp_append(choices, next);
            max -= (l + 1);
        }
        else {
            l = (max - 1);
            dbp_append(choices, " ");
            dbp_appendn(choices, next, l);
            max = 0;
        }
    }
    return unique;
}

/* Entry point for filename completion
 * Looks for things starting with name, and if there are multiple choices
 * builds a catenated string of them in choices.
 */
static int comp_file(db *name, db *choices) {
    const char *p;      /* Handy pointer */
    int unique;

    dbp_set(choices, "");

    if ((p = getffile(dbp_val(name))) == NULL) {
        close_dir();
        mlwrite_one(NOMATCH);
        sleep(1);
        return FALSE;
    }
    else
        db_set(so_far, p);

    unique = matcher(dbp_val(name), 0, choices, CMPLT_FILE);
    close_dir();

    if (db_len(directory) > 0) {
        dbp_set(name, db_val(directory));
        dbp_append(name, db_val(so_far));
    }
    else dbp_set(name, db_val(so_far));

/* If we found a single entry that begins with '~' and contains no '/'
 * pass it to fixup_fname() so that it can expand any login id to its
 * HOME dir. This mean that ~id gets expanded to $HOME, rather than just
 * looping with ~id in the buffer.
 * If this does happen (the leading ~ goes away) we append a '/'.
 */
    if (unique) {
        if ((dbp_charat(name, 0) == '~') && (!strchr(dbp_val(name), '/'))) {
/* Make copy from fixup_fname()'s internal buffer back to the incoming
 * buffer.
 */
            dbp_set(name, fixup_fname(dbp_val(name)));
            if (dbp_charat(name, 0) != '~') dbp_addch(name, '/');
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
static int comp_gen(db *name, db *choices, enum cmplt_type mtype) {
    const char *p;      /* Handy pointer */
    int namelen;

    dbp_set(choices, "");
    namelen = dbp_len(name);
    switch(mtype) {
    case CMPLT_BUF:
    case CMPLT_PROC:
    case CMPLT_PHON:
        p = getfbuffer(dbp_val(name), namelen, mtype);
        break;
    case CMPLT_NAME:
        p = getfname(dbp_val(name), namelen);
        break;
    case CMPLT_VAR:
        p = getfvar(dbp_val(name), namelen);
        break;
    default:            /* If mtype arrives oddly? */
        p = NULL;
    }
    if (p == NULL) {
	mlwrite_one(NOMATCH);
        sleep(1);
        return FALSE;
    }
    db_set(so_far, p);
    (void)matcher(dbp_val(name), namelen, choices, mtype);
    dbp_set(name, db_val(so_far));

    return TRUE;
}

/* tgetc:   Get a key from the terminal driver.
 *          Resolve any keyboard macro action.
 */
int tgetc(void) {
    int c;              /* fetched character */

/* If we are playing a keyboard macro back, */
    if (kbdmode == PLAY) {
        if (kbdptr < kbdend)    /* If there is some left... */
            return (int) *kbdptr++;
        if (--kbdrep < 1) {     /* at the end of last repetition? */
            kbdmode = STOP;     /* Leave ctlxe_togo alone */
            if (!vismac) update(FALSE); /* Force update now all is done? */
        }
        else {
            kbdptr = kbdm;      /*` Reset macro to beginning for the next rep */
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
            if (kbdptr == &kbdm[n_kbdm - 1]) {  /* Don't overrun buffer */
                n_kbdm += 256;
                kbdm = Xrealloc(kbdm, n_kbdm*sizeof(int));
                kbdptr = &kbdm[n_kbdm - 1];     /* Might have moved */
            }
            kbdend = kbdptr;
        }

/* And finally give the char back */
    return c;
}

/* Entry point for buffer name completion. And userprocs.
 * Front-end to comp_gen().
 */
static int comp_buffer(db *name, db *choices, enum cmplt_type mtype) {
    return comp_gen(name, choices, mtype);
}

/* Entry point for internal command name completion
 * Front-end to comp_gen().
 */
static int comp_name(db *name, db *choices) {
    return comp_gen(name, choices, CMPLT_NAME);
}

/* Entry point for internal environment/user var completion
 * Front-end to comp_gen().
 */
static int comp_var(db *name, db *choices) {
    return comp_gen(name, choices, CMPLT_VAR);
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

/* Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. Used any time a confirmation is required.
 */
int mlyesno(const char *prompt) {
    int c;              /* input character - tgetc() returns unicode */
    int res = -1;       /* NOT ABORT, TRUE or FALSE */
    while(res == -1) {
        mlwrite("%s%s", prompt, " " MLbkt("y/n") "? ");

        c = get1key();  /* get the response */
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
            if (c == abortc) res = ABORT;   /* As abortc is a var */
            break;
        }
    }
    mlerase();
    return res;
}

/* Write a prompt into the message line, then read back a response.
 * Actually done by using nextarg, which either gets the next token
 * (when running a macro) or comes back to getstring() (in this file)
 * if interactive. This lets macros run at full speed.
 *
 * NOTE that when the function is processing macros from a (start-up) file
 * it will get one arg at a time.
 * BUT when run interactively it will return the entire response.
 * So macro-file args need to be quoted...
 * We pass on any expansion-type requested (for, eventually, getstring()).
 */
int mlreply(const char *prompt, db *buf, enum cmplt_type ctype) {
    return nextarg(prompt, buf, ctype);
}

/* get a command name from the command line. Command completion means
 * that pressing a <TAB> will attempt to complete an unfinished command
 * name if it is unique.
 */
struct name_bind *getname(const char *prompt, int new_command) {
    struct name_bind *nbp = NULL;

    db_strdef(buf);

/* First get the name... */
    if (clexec == FALSE) {
        no_macrobuf_record = 1;
        int status = mlreply(prompt, &buf, CMPLT_NAME);
        no_macrobuf_record = 0;
        if (status != TRUE) goto exit;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, &buf);
   }

/* Now check it exists */

    nbp = name_info(db_val(buf));
    if (nbp) {
        if (kbdmode == RECORD) addto_kbdmacro(db_val(buf), new_command, 0);
    }
    else {
        mlwrite("No such function: %s", db_val(buf));
    }
exit:
    db_free(buf);
    return nbp;
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
    int c;              /* Fetched keystroke */
    int ctlx = FALSE;
    int meta = FALSE;
    int cmask = 0;

/* Keep going until we return something */

    while ((c = get1key())) {   /* Extra ()s for gcc warniing */
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
    case 17: c = 'f'; break;    /* Skip 16 */
    case 18: c = 'g'; break;
    case 19: c = 'h'; break;
    case 20: c = 'i'; break;
    case 21: c = 'j'; break;
    case 23: c = 'k'; break;    /* Skip 22 */
    case 24: c = 'l'; break;
    case 25: c = 'm'; break;
    case 26: c = 'n'; break;
    case 28: c = 'o'; break;    /* Skip 27 */
    case 29: c = 'p'; break;
    case 31: c = 'q'; break;    /* Skip 30 */
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

/* Evaluate a string as a command.
 * Done by saving the current command buffer, replacing it with what
 * we have then running token() before replacing things.
 * We (now) loop over all of the command string.
 */
void evaluate_cmdb(const char *input, db *result) {
    dbp_set(result, "");                /* Empty it */
    const char *orig_execstr = execstr; /* Just in case... */
    int orig_clexec = clexec;
    char *sep = "";

/* We take a copy of the input, so that we don't overwrite
 * the user input.
 */
    execstr = strdupa(input);
    clexec = TRUE;
    db_strdef(temp);
    while(1) {
        if (!*execstr) break;
        (void)nextarg("", &temp, 0);
        dbp_append(result, sep);
        sep = " ";
        dbp_append(result, db_val(temp));
    }
    db_free(temp);
    execstr = orig_execstr;
    clexec = orig_clexec;
}

/* The actual getstring() starts now... */

int getstring(const char *prompt, db *buf, enum cmplt_type ctype) {
    struct buffer *bp;
    struct buffer *cb;
    char mbname[20];        /* Ample */
    int c;                  /* command character */
    struct line *lp;        /* line to copy */
    int size;
    const char *sp;         /* string pointer into line */
    int status;
    int do_evaluate = FALSE;
    func_arg sav;

    short savdoto;
    int prolen;

    db_strdef(procopy);
    db_strdef(choices);
    db_strdef(tstring);

/* We are about to enter the minibuffer, so all com_flags must
 * be turned off.
 */
    com_flag = CFNONE;

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
        mb_winp->w_wndp = NULL;     /* Initialize window */
#if COLOR
/* initialize colors to global defaults */
        mb_winp->w_fcolor = gfcolor;
        mb_winp->w_bcolor = gbcolor;
#endif
        mb_winp->w_toprow = term.t_mbline;
        mb_winp->w_ntrows = 1;
        mb_winp->w.fcol = 0;
        mb_winp->w_force = 0;
        mb_winp->w_flag = WFMODE | WFHARD;  /* Full */
    }

/* Get a minibuffer name based on the current level */

    sprintf(mbname, "//minib%04d", mb_info.mbdepth+1);
    cb = curbp;

/* Update main screen before entering the minibuffer */

    update(FALSE);

/* Set-up the (incoming) prompt string and clear any prompt update flag,
 * as it should only get set during loop.
 * This means we can *now* initialize an empty return buffer
 * GGR NOTE!!
 * some callers use the return buffer as a temporary buffer for the prompt!
 */
    db_set(procopy, prompt);
    prolen = db_len(procopy);
    prmpt_buf.update = 0;
    dbp_set(buf, "");           /* Ensure we never return garbage */

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

/* Clear the bottom line... */

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
 * We start by ensuring that the minibuffer display is refreshed,
 * in case it has been overwritten by a message.
 */
loop:
    mbupdate();

/* Execute the "command" macro...normally null
 * Don't start the handler when it is already running as that might
 * just get into a loop...
 */
    if (!meta_spec_active.C) {
        meta_spec_active.C = 1;
        execute(META|SPEC|'C', FALSE, 1);
        meta_spec_active.C = 0;
    }

/* Have we been asked to update the prompt? */

    if (prmpt_buf.update) {
        db_set(procopy, db_val(prmpt_buf.prompt));
        prolen = db_len(procopy);
        prmpt_buf.update = 0;
    }

/* Have we been asked to load a (search/replace) string?.
 * If so, insert it into our buffer (which is the result buffer) now,
 * which inserts it at the "current location".
 */
    if (prmpt_buf.preload) {
        linstr(prmpt_buf.preload);
        prmpt_buf.preload = NULL;   /* One-time usage */
    }

/* Prepend the prompt to the beginning of the visible line (i.e.
 * ahead of any preload.
 */
    savdoto = curwp->w.doto;
    curwp->w.doto = 0;
    linstr(db_val(procopy));
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
    ldelete((ue64I_t)prolen, FALSE);
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
        sp = ltext(lp);
/* NSTRING-1, as we need to add a trailing NUL */
        int expanded;
        db_setn(tstring, sp, lused(lp));
        switch(ctype) {
        case CMPLT_FILE:
            expanded = comp_file(&tstring, &choices);
            break;
        case CMPLT_BUF:
        case CMPLT_PROC:
        case CMPLT_PHON:
            expanded = comp_buffer(&tstring, &choices, ctype);
            break;
        case CMPLT_NAME:
            expanded = comp_name(&tstring, &choices);
            break;
        case CMPLT_VAR:
            expanded = comp_var(&tstring, &choices);
            break;
        default:            /* Do nothing... */
            expanded = 0;
        }
        if (expanded) {
            savdoto = curwp->w.doto;
            curwp->w.doto = 0;
            ldelete((ue64I_t) lused(lp), FALSE);
            linstr(db_val(tstring));
/* Don't bother with this when playing a macro - that just results
 * in an unnecessary pause from the sleep().
 */
            if ((db_len(choices) > 0) && (kbdmode != PLAY)) {
                mlwrite_one(db_val(choices));
                size = (db_len(choices) < 42) ? 1 : 2;
                sleep(size);
                mberase();
            }
        }
        else TTbeep();
        goto loop;
    }

/* Some "hard-wired" key-bindings - aka minibuffer specials. */

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
    dbp_set(buf, "");  /* Empty it */
    int first = 1;
    while (mblp != bp->b_linep) {
        if (first) first = 0;
        else       dbp_addch(buf, '\n');    /* Add NL if not first */
        dbp_appendn(buf, ltext(mblp), lused(mblp));
        mblp = lforw(mblp);
    }

/* Need to copy to return buffer and, if not empty,
 * to save as last minibuffer.
 */
    if (dbp_len(buf)) {
        addto_lastmb_ring(dbp_val(buf));    /* End NULL is actually there */
    }
    else status = FALSE;        /* Empty input... */

/* Record the result if we are recording a keyboard macro, but only
 * at first level of the minibuffer (i.e. the "true" result),
 * and only if we have some text.
 */
    if ((dbp_len(buf) > 0) && (kbdmode == RECORD) &&
        (mb_info.mbdepth == 1) && !no_macrobuf_record)
         addto_kbdmacro(dbp_val(buf), 0, !do_evaluate);

/* If we have to evaluate, do it now.
 * Note that this is *AFTER* we've done any logging to the macro.
 * We have to fudge buf into execstr for function evaluating to work.
 */
    if (do_evaluate) evaluate_cmdb(dbp_val(buf), buf);

abort:

/* If we get here "normally" SIGWINCH will still be enabled, so we need
 * to block it while we tidy up, otherwise we might run through this end
 * code twice.
 */
    sigprocmask(SIG_BLOCK, &sigwinch_set, NULL);

    if (!swbuffer(bp, 0))   /* Make sure we're still in our minibuffer */
        return FALSE;
    unmark(TRUE, 1);

    if (--mb_info.mbdepth) {    /* Back to previous mini-buffer */
        if (!swbuffer(cb, 0))   /* Switch to its buffer... */
            return FALSE;
        *curwp = wsave;         /* ...and its window info */
    }
    else {                      /* Leaving minibuffer */
        if (!swbuffer(mb_info.main_bp, 0))  /* Switch back to main buffer */
            return FALSE;
        curwp = mb_info.main_wp;    /* Reset window info ptr */
        wheadp = mb_info.wheadp;    /* Reset window list */
        inmb = FALSE;               /* Note we have left mb */
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

    db_free(procopy);
    db_free(choices);
    db_free(tstring);

/* If this is a CMPLT_FILE type then we have a filename.
 * So run fixup_fname() on  it for consistent returns - if no
 * tabbing was done it won't have ever been expanded and we might
 * have something like ~/x/test.
 */
    if (ctype == CMPLT_FILE) {
        db_set(glb_db, fixup_fname(dbp_val(buf)));
        dbp_set(buf, db_val(glb_db));
    }

    return status;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_input(void) {
    Xfree(mb_winp);

    db_free(directory);
    db_free(picture);
    db_free(so_far);
    db_free(fullname);
    db_free(suggestion);
    db_free(prmpt_buf.prompt);

    return;
}
#endif
