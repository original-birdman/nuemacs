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

/* A set of functions (cmplt_*) to do with filename/buffer/name completions.
 * Part of the GGR additions (but now using loops, not recursion).
 * Looks for things starting with name, and if there are multiple choices
 * builds a catenated string of them in choices.
 * Returns a ptr to a malloc'ed struct cmpl_info.
 * It is the CALLERS RESPONSIBILITY to free both this AND its members.
 */

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>

/* What to show in the minibuffer if completion doesn't find anything */

#define NOMATCH "No match"

/* Handle internal env/user variable completions.
 *
 * We build the env variable sort index in getfvar once
 * We build the user variable sort index in getfvar each time.
 * However iff the incoming text is blank we just want to show
 * $... %..., and if it doesn't start with $ or % we want to show no match.
 */
extern int *envvar_index;
extern char *uvnames[];
extern struct evlist evl[];

/* The screen width will be constant for each completion run */
static struct cmpl_info {
    db_dcl(match);
    db_dcl(choices);    /* matches, OR error text */
    db_dcl(mprefix);
    int found;          /* -1 (error), 0, 1 or > 1 */
    int choices_max;    /* In colums, not chars */
    int full;
} res = { db_str_initval, db_str_initval, db_str_initval, 0, 0, 0 };

/* The 4 cmplt_* routines cannot run at the same time, so
 * declare some control information to be static and initialize
 * it for each one, which saves passign this around and alloc/freeing
 * things.
 */
static void init_cmplt(void) {
    res.choices_max = term.t_ncol - 1;
    db_set(res.mprefix, "");
    db_set(res.choices, "");
    res.found = 0;
    res.full = 0;
}

/* All 4 completion functions have a common way to work out the maximum
 * matching prefix, and the choices prompt.
 */
static int update_prompts(const char *np) {

    if (res.full < 0) return res.full;  /* In case caller forgot to check */

/* Work out the maximum matching prefix with this one found */

    if (res.found == 0) {
        db_set(res.mprefix, np);
    }
    else if (db_len(res.mprefix) > 0) {
        const char *ap = db_val(res.mprefix);
        const char *bp = np;
        while (*ap == *bp) {
            if (!*++ap) break;
            if (!*++bp) break;
        }
        db_truncate(res.mprefix, (int)(ap - db_val(res.mprefix)));
    }
    res.found++;

/* Add to choices, unless that is full */
    if (!res.full) {
        db_append(res.choices, np);
        db_append(res.choices, " ");
        if (utf8_to_uclen(db_val(res.choices),  TRUE,
            db_len(res.choices)) > res.choices_max) {
            db_uctruncate(res.choices, res.choices_max);
/* Append … elipsis */
            char utf8[6];
            int uclen = unicode_to_utf8(0x2026, utf8);
            db_appendn(res.choices, utf8, uclen);
            res.full = 1;
        }
    }

/* Iff mprefix is now empty (no common prefix) AND the choices list
 * is full then there is no point in looking at any other entries,
 * as they can't change anything (excpet the res.found counter,
 * which really only has a 0, 1, many value.
 * So note this by returning -1.
 * NOTE that this means all callers MUST check for this.
 */
    if (res.full && (db_len(res.mprefix) == 0)) {
        res.full = -1;
    }
    return res.full;
}

/* Entry point for filename completion
 */
static db_strdef(dirmark);
#include <libgen.h>
static void cmplt_file(db *name) {
    int allents = 0;

    init_cmplt();

#ifndef STANDALONE

/* Handle ~xxx by looking up all matching login ids.
 * NOTE that ~xxx/ has specified a full id, and that is handled by
 * fixup_fname() later.
 */
    if ((*dbp_val(name) == '~') && (!strchr(dbp_val(name), '/'))) {
        char *id_to_find = strdupa(dbp_val(name)+1);
        int id_to_find_len = istrlen(id_to_find);
        allents = (id_to_find_len == 0);

/* Handle ~xxx by looking up all login ids that match the prefix. */
        struct passwd *pwe;
        setpwent();                 /* Start it off */
        errno = 0;
        while ((pwe = getpwent())) {
            if (allents ||
                 !strncmp(pwe->pw_name, id_to_find, (size_t)id_to_find_len)) {
                if (update_prompts(pwe->pw_name) < 0) break;
            }
        }
        endpwent();
        if (errno) {
            db_sprintf(res.choices, "passwd lookup: %s", strerror(errno));
            db_set(res.match, dbp_val(name));
            res.found = -1;
            return;
        }
        db_set(res.match, "~");
        db_append(res.match, db_val(res.mprefix));
        return;
    }
#endif

/* Is the user denoting name as a directory?.
 * i.e, does it end with /, /. or /..
 */

    int want_dir = FALSE;
    const char *ep = dbp_val(name) + dbp_len(name) - 1;
    int count = 3;
    while (count-- && ep >= dbp_val(name)) {
        if (*ep == '.') {
            ep--;
            continue;
        }
        if (*ep == '/') want_dir = TRUE;
    }

/* Canonicalize what we have. */

    db_strdef(lookat);
    db_set(lookat, fixup_fname(dbp_val(name)));

/* Did the user say this was a directory?
 * If so, append /., as fixup_fname will have stripped any trailing marker
 */
    if (want_dir) db_append(lookat, "/.");

    char *dir = dirname(strdupa(db_val(lookat)));
    char *stem = basename(strdupa(db_val(lookat)));

    DIR *dirptr  = opendir(dir);
    if (!dirptr) {
        db_set(res.match, dir);
        db_set(res.choices, strerror(errno));
        res.found = -1;
        return;
    }
    if (0 == strcmp(stem, ".")) {
        stem = strdupa("");
        allents = 1;
    }
    if (0 == strcmp(dir, ".")) {
        db_set(res.match, "");
    }
    else {
        db_set(res.match, dir);
        db_append(res.match, "/");
    }

/* We now have the directory open, so loop through the entries
 * finding matches to "stem".
 * Once choices is more then the line length we can stop:
 * if we have found more than one entry AND
 * the longest common prefix is "".
 */
    struct dirent *dp;
    struct stat statbuf;
    int dir_found = 0;
    int dfd = dirfd(dirptr);
    while ((dp = readdir(dirptr)) != NULL) {
        if ( (strcmp(dp->d_name, ".") == 0) ||
             (strcmp(dp->d_name, "..") == 0) ) continue;
        if (!allents &&
             (strncmp(dp->d_name, stem, strlen(stem)))) continue;

/* If this is a directory append / to the name that will show in choices */

        const char *np;
        if (fstatat(dfd, dp->d_name, &statbuf, 0) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                db_set(dirmark, dp->d_name);
                db_append(dirmark, "/");
                np = db_val(dirmark);
            }
            else {
                np = dp->d_name;
            }
            if (update_prompts(np) < 0) break;
        }
    }
    closedir(dirptr);
    if (res.found == 0) db_set(res.choices, NOMATCH);
    else                db_append(res.match, db_val(res.mprefix));

/* This allows us to find a directory and then expand it with one <tab>,
 * rather than needing two.
 */
    if (dir_found && (res.found == 1)) db_append(res.match, "/");
    return;
}

/* Entry point for buffer name (etc.) completion.
 */
static void cmplt_buffer(db *name, enum cmplt_type mtype) {

    init_cmplt();

    for (struct buffer *bp = bheadp; bp != NULL; bp = bp->b_bufp) {

/* We NEVER match minibuffer buffers (//minibnnnn), and we return internal
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
              (bp->b_type != BTPROC || bp->b_bname[0] != '/' ||
               bp->b_bname[1] == '/')) ||   /* Catch //kbd_macro */
            (mtype == CMPLT_PHON &&
              (bp->b_type != BTPHON || bp->b_bname[0] != '/')) ||
            (strncmp(dbp_val(name), bp->b_bname + offset, (size_t)dbp_len(name))) ||
            (!strncmp(bp->b_bname, "//minib", 7)) ||
            ((bp->b_bname[0] == '[') && dbp_len(name) == 0)) {
             continue;
        }

        const char *np = bp->b_bname + offset;
        if (update_prompts(np) < 0) break;
    }
    if (res.found == 0) db_set(res.choices, NOMATCH);
    else                db_set(res.match, db_val(res.mprefix));
    return;
}

/* Entry point for name and var completion.
 * These are very similar.
 * In particular they look through sorted values by index.
 */
static void cmplt_name_or_var(db *name, enum cmplt_type ctype) {
    init_cmplt();

    const char *np;
    int nlen;
    char first_ch;
    if (ctype == CMPLT_VAR) {
        first_ch = dbp_charat(name, 0);
        if ((dbp_len(name) == 0) ||
             ((first_ch != '$') && (first_ch != '%'))) {
            db_set(res.match, "");
            db_set(res.choices, "Input must start with $ or %");
            res.found = -1;
            return;
        }

/* Set up sort index and array */
        if (first_ch == '$') {
            if (envvar_index == NULL) init_envvar_index();
        }
        else {  /* Sorted so we can exit loop early */
            sort_user_var();
        }
        np = dbp_val(name) + 1; /* Don't compare the leading $ or % */
        nlen = (dbp_len(name) - 1);
    }
    else {  /* So CMPLT_NAME */
        np = dbp_val(name);     /* Compare all of name */
        nlen = dbp_len(name);
    }

/* nxti_* returns -1 at end of list.
 * Variable names can take any value of character.
 * But, if we increment the final byte of the stem we are looking for then
 * anything >= to that cannot match the stem, as the names are sorted.
 * A byte of 0xff is illegal in utf8...
 */
    char *limit;
    if (nlen != 0) {    /* Fudge code to increment the final character. */
        limit = strdupa(np);
        char *lp = limit + nlen - 1;
        (*lp)++;
    }

    int (*nvar_get)(int);
    db_strdef(vn);      /* We have to prepend $ or % for update_prompts() */
    if (ctype == CMPLT_VAR) {   /* first_ch already set */
        if (first_ch == '$') nvar_get = nxti_envvar;
        else                 nvar_get = nxti_usrvar;
        db_set(vn, "");
        db_addch(vn, first_ch);
    }
    else {
                             nvar_get = nxti_name_info;
    }
    int vidx = -1;
    while ((vidx = nvar_get(vidx)) >= 0) {
        const char *vp;
        if (ctype == CMPLT_VAR) {
            if (nvar_get == nxti_envvar) vp = evl[vidx].var;
            else                         vp = uvnames[vidx];
        }
        else {
                                         vp = names[vidx].n_name;
        }
        if ((nlen != 0) && (strcmp(vp , limit) > 0)) break;
        if (strncmp(np, vp, (size_t)nlen) != 0) continue;
        const char *upstr;
        if (ctype == CMPLT_VAR) {
            db_retailstr_at(vn, vp, 1);
            upstr = db_val(vn);
        }
        else {
            upstr = vp;
        }
        if (update_prompts(upstr) < 0) break;
    }
    if (res.found == 0) db_set(res.choices, NOMATCH);
    else                db_set(res.match, db_val(res.mprefix));
    return;
}

/* tgetc:   Get a key from the terminal driver.
 *          Resolve any keyboard macro action.
 */
unicode_t tgetc(void) {
    unicode_t c;              /* fetched character */

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
            return *kbdptr++;
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
                kbdm = Xrealloc(kbdm, (size_t)n_kbdm*sizeof(int));
                kbdptr = &kbdm[n_kbdm - 1];     /* Might have moved */
            }
            kbdend = kbdptr;
        }

/* And finally give the char back */
    return c;
}

/* get1key: Get one keystroke.
 *          The only prefixes legal here are SPEC and CONTROL.
 */
unicode_t get1key(void) {
    unicode_t c;

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
        token(execstr, &buf);
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
static unicode_t ensure_uppercase(unicode_t c) {
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

unicode_t getcmd(void) {
    unicode_t c;        /* Fetched keystroke */
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
    dbp_dcl(orig_execstr) = execstr;
    int orig_clexec = clexec;
    const char *sep = "";

/* We take a copy of the input, so that we don't overwrite
 * the user input.
 */
    db_upstrdef(nexecstr);
    db_set(nexecstr, input);    /* Updateable copy */
    execstr = &nexecstr;;
    clexec = TRUE;
    db_strdef(temp);
    while(dbp_len(execstr) > 0) {
        (void)nextarg("", &temp, 0);
        dbp_append(result, sep);
        sep = " ";
        dbp_append(result, db_val(temp));
    }
    db_free(temp);
    db_free(nexecstr);
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
    const char *sp;         /* string pointer into line */
    int status;
    int do_evaluate = FALSE;
    func_arg sav;

    int savdoto;
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
 * NOTE that this means we need to restore thigs on EVERY return!
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

    if (!swbuffer(bp, 0)) {
        sigprocmask(SIG_SETMASK, &incoming_set, NULL);
        return FALSE;
    }

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
    if (db_blen(prmpt_buf.preload)) {
        lins_dynbuf(&prmpt_buf.preload);
        db_clear(prmpt_buf.preload);    /* One-time usage */
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
/* We get UEM_NOCHAR back on a SIGWINCH signal. */
    remap_c_on_intr = 0;
    if (c == UEM_NOCHAR) goto loop;

/* Check for any numeric prefix
 * This looks for Esc<n> prefixes and returns c/f/n in carg.
 * The "c" arg may be the start of a numeric prefix (e.g.Esc2) so
 * from here on it's carg->c that needs to be checked.
 */
    com_arg *carg = multiplier_check(c);

/* Some "hard-wired" key-bindings - aka minibuffer specials. */

    do_evaluate = FALSE;
    switch(carg->c) {           /* The default is to do nothing here */
    case CONTROL|'I': {
        if (ctype == CMPLT_SRCH) {
            rotate_sstr(carg->n);
            goto post_exec;
        }
/* Various completion code options
 * Usually a list of matches is temporarily displayed in the minibuffer.
 * THIS ONLY USES THE TEXT ON THE CURRENT LINE, and uses ALL OF IT.
 */
        lp = curwp->w.dotp;
        sp = ltext(lp);
        db_setn(tstring, sp, lused(lp));
        int do_display = 1;
        switch(ctype) {
        case CMPLT_FILE: {
            cmplt_file(&tstring);
            break;
        }
        case CMPLT_BUF:
        case CMPLT_PROC:
        case CMPLT_PHON: {
            cmplt_buffer(&tstring, ctype);
            break;
        }
        case CMPLT_NAME:
        case CMPLT_VAR: {
            cmplt_name_or_var(&tstring, ctype);
            break;
        }
        default:            /* Do nothing... */
            do_display = 0;
            break;
        }
        if (do_display) {
/* NOTE that we leave what was there if nothing matches */
            if (res.found != 0) {
                savdoto = curwp->w.doto;
                curwp->w.doto = 0;
                ldelete((ue64I_t)lused(lp), FALSE);
                linstr(db_val(res.match));
            }
/* Don't bother with this when playing a macro - that just results
 * in an unnecessary pause from the sleep().
 */
            if ((db_len(res.choices) > 0) && (kbdmode != PLAY)) {
                mlwrite_one(db_val(res.choices));
/* We're about to goto post_exec, which will pause as mpresf will
 * be set, Add an extra pause if we're over a certain length.
 */
                if (db_len(res.choices) >= 42) sleep(1);
            }
        }
        else TTbeep();
        goto post_exec;
    }
    case META|CONTROL|'I':      /* Only act for CMPLT_SRCH */
        if (ctype == CMPLT_SRCH) rotate_sstr(-(carg->n));
        goto post_exec;
    case CTLX|CONTROL|'I':      /* Only act for CMPLT_SRCH */
        if (ctype == CMPLT_SRCH) select_sstr();
        goto post_exec;
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
post_exec:
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
/* ltext_chk() as it might ne empty */
        dbp_appendn(buf, ltext_chk(mblp), lused(mblp));
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

    if (!swbuffer(bp, 0)) { /* Make sure we're still in our minibuffer */
        status = FALSE;
        goto rewinch_and_exit;
    }
    unmark(TRUE, 1);

    if (--mb_info.mbdepth) {    /* Back to previous mini-buffer */
        if (!swbuffer(cb, 0)) { /* Switch to its buffer... */
            status = FALSE;
            goto rewinch_and_exit;
        }
        *curwp = wsave;         /* ...and its window info */
    }
    else {                      /* Leaving minibuffer */
        if (!swbuffer(mb_info.main_bp, 0)) {/* Switch back to main buffer */
            status = FALSE;
            goto rewinch_and_exit;
        }
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

    db_free(procopy);
    db_free(choices);
    db_free(tstring);

/* If this is a CMPLT_FILE type then we have a filename.
 * So run fixup_fname() on  it for consistent returns - if no
 * tabbing was done (or text was added after the last tabbing) then it
 * won't have ever been expanded and we might have something like ~/x/test.
 * This does mean that any "/" we've appended to indicate a directory
 * will disappear, but that's OK as that is only for visual marking in the
 * minibuffer.
 */
    if (ctype == CMPLT_FILE) {
        db_set(glb_db, fixup_fname(dbp_val(buf)));
        dbp_set(buf, db_val(glb_db));
    }

rewinch_and_exit:
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
    Xfree(mb_winp);

    db_free(prmpt_buf.prompt);

    db_free(res.match);
    db_free(res.choices);
    db_free(res.mprefix);

    return;
}
#endif
