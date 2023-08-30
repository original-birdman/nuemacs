/*      bind.c
 *
 *      This file is for functions having to do with key bindings,
 *      descriptions, help commands and startup file.
 *
 *      Written 11-Feb-86 by Daniel Lawrence
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "epath.h"
#include "line.h"
#include "util.h"

#include "utf8proc.h"

#define HELP_BUF "//Help"

int help(int f, int n) {    /* give me some help!!!!
                               bring up a fake buffer and read the help file
                               into it with view mode                 */
    UNUSED(f); UNUSED(n);
    struct window *wp;      /* scaning pointer to windows */
    struct buffer *bp;      /* buffer pointer to help */
    char *fname = NULL;     /* ptr to file returned by flook() */

/* First check if we are already here */
    bp = bfind(HELP_BUF, FALSE, BFINVS); /* GGR - epath.h setting */

    if (bp == NULL) {
        fname = flook(init_files.help, FALSE, INTABLE);
        if (fname == NULL) {
            mlwrite_one(MLbkt("Help file is not online"));
            return FALSE;
        }
    }

/* Split the current window to make room for the help stuff,
 * before reading in/switching buffer.
 */
    if (splitwind(FALSE, 1) == FALSE) return FALSE;

    if (bp == NULL) {
        bp = bfind(HELP_BUF, TRUE, BFINVS);
        if (!swbuffer(bp, 0)) return FALSE;
        pathexpand = FALSE; /* GGR - unset pathexpand around call */
        int res = readin(fname, FALSE);
        pathexpand = TRUE;
        if (res == FALSE) return(FALSE);
        bp->b_fname[0] = '\0';  /* remove filename */
    }
    else
        if (!swbuffer(bp, 0)) return FALSE;

/* Make this window in VIEW mode, update all mode lines */
    curwp->w_bufp->b_mode |= MDVIEW;
    curwp->w_bufp->b_flag |= BFINVS;
    wp = wheadp;
    while (wp != NULL) {
        wp->w_flag |= WFMODE;
        wp = wp->w_wndp;
    }
    return TRUE;
}

/* stock:
 *      String key name TO Command Key
 *      We need to be careful about signedness in comparisons!!
 *
 * char *keyname;       name of key to translate to Command key from
 */
static unsigned int stock(char *keyname) {
    unsigned int c; /* key sequence to return */

/* GGR - allow different bindings for 1char UPPER and lower.
 * If anyone wants to bind them...
 */
    int kn_len = strlen(keyname);
    int noupper = (kn_len == 1);
    char *kn_end = keyname + kn_len;
    int special = 0;

/* Parse it up */
    c = 0;

/* First, the CtlX prefix */
    if (*keyname == '^' && *(keyname + 1) == 'X') {
        c |= CTLX;
        keyname += 2;
    }
/* Then the META prefix */
    if (*keyname == 'M' && *(keyname + 1) == '-') {
        c |= META;
        keyname += 2;
    }
/* Next the function prefix, but not if we have a META.
 * getcmd() also disallows this.
 * Thus META|SPEC can never be set by the user, and the internal handlers
 * are safe from being overwritten by a key binding.
 */
    if (!(c & META) && *keyname == 'F' && *(keyname + 1) == 'N') {
        c |= SPEC;
        noupper = TRUE;             /* Don't uppercase final char */
        keyname += 2;
    }

/* What is left? A control char?
 * Allow ^ on its own (by not marking CONTROL or stepping over it).
 * Don't allow trying to set a Control < @.
 */
    if (*keyname == '^' && *(keyname + 1) != 0) {
        ++keyname;                  /* Step to the controlled char... */
        if (*(keyname) == '?')      /* Special case ^? for Delete */
            special = 0x7f;         /* Don't add the Control */
        else {                      /* Check it is valid to be a Control */
            if (*(keyname) < '@' || *(keyname) > 'z') return 0;
            c |= CONTROL;
        }
    }

/* If we have nothing more, return nothing - we should be at a bind char... */

    if (!*keyname) return 0;

/* GGR - allow SP for space by putting it there... */
    if (*keyname == 'S' && *(keyname + 1) == 'P') {
        ++keyname;                  /* Advance to P for e-o-string tests */
        special = ' ';
    }

/* Make sure we are not lower case with only Control or Meta */
    if (ch_as_uc(*keyname) >= 'a' && ch_as_uc(*keyname) <= 'z' &&
         !(noupper))            /* GGR */
        *keyname -= 32;

/* NOTE that any char beyond the ASCII range is NO LONGER disallowed
 * by bindtokey() and buffertokey().
 * If the user can type it they may well wish to use it.
 */
    if (ch_as_uc(*keyname) >= 0x80) {   /* We have a utf-8 string... */
        unsigned int uc;
        int kn_left = kn_end - keyname;
        keyname += utf8_to_unicode(keyname, 0, kn_left, &uc);
        uc = utf8proc_toupper(uc);      /* Unchanged if not lower/title case */
        c |= uc;        /* The final tagged char... */
    }
    else {              /* The final sequence... */
        if (special) c |= special;
        else c |= *keyname;
        keyname++;
    }

/* If we aren't at end-of string, or a white-space, something is wrong */
    switch(*keyname) {
        case '\0':
        case ' ':
        case '\t':
        case '\n':
            break;
        default:
            return 0;
    }
    return c;
}

/* get a command key sequence from the keyboard
 *
 * int mflag;           going for a meta sequence?
 */
static unsigned int getckey(int mflag) {
    char tok[NSTRING];      /* command incoming */

/* Check to see if we are executing a command line */
    if (clexec) {
        macarg(tok);    /* get the next token */
        return stock(tok);
    }

/* Or from user input */

    return (mflag)? get1key(): getcmd();
}

/* Describe the command for a certain key
 */
int deskey(int f, int n) {
    UNUSED(f); UNUSED(n);
    int c;          /* key to describe */
    char *ptr;      /* string pointer to scan output strings */
    char outseq[NSTRING];   /* output buffer for command sequence */

/* Prompt the user to type us a key to describe */
    mlwrite_one(": describe-key ");

/* Get the command sequence to describe.
 * Change it to something we can print as well and dump it out.
 */
    c = getckey(FALSE);
    if (c == 0) {
        mlwrite_one("Can't parse key string!");
        return FALSE;
    }
    cmdstr(c, outseq);
    mlputs(outseq);
    mlputs(" ");

/* Find the right function */
    struct key_tab *ktp = getbind(c);
    if (!ktp) ptr = "Not Bound";
    else {
        ptr = ktp->fi->n_name;
/* Display a possible multiplier */
        if (ktp->bk_multiplier != 1) {
            char tbuf[16];
            sprintf(tbuf, "{%d}", ktp->bk_multiplier);
            mlputs(tbuf);
        }
    }

/* Output the function name */
    mlputs(ptr);

/* Add buffer-name, if relevant */
    if (ktp && ktp->k_type == PROC_KMAP) {
        mlputs(" ");
        mlputs(ktp->hndlr.pbp);
    }
    if (inmb) TTflush();    /* Need this if we are in the minibuffer */
    mpresf = TRUE;          /* GGR */
    return TRUE;
}

/* (Re)Index the key bindings...
 */
#include <stddef.h>
#include "idxsorter.h"

static int *key_index = NULL;
static int key_index_allocated = 0;
static int kt_ents;     /* Actual populated entries */

static int keystr_index_valid = 0;

/* Note that we sort the keys as UNSIGNED!
 * This is so that we can step through them in order when dumping them
 * in desbind() and have the SPEC entries at the end.
 * It does also mean that when we do a lookup (in getbind()) we need
 * that to do an unsigned comparison during the binary chop.
 */
static void index_bindings(void) {
    if (key_index_allocated < keytab_alloc_ents) {
        key_index = Xrealloc(key_index, keytab_alloc_ents*sizeof(int));
        key_index_allocated = keytab_alloc_ents;
    }
    struct fields fdef;
    fdef.offset = offsetof(struct key_tab, k_code);
    fdef.type = 'U';
    fdef.len = sizeof(int);

/* The allocated keytab contains markers at the end, which we do not
 * want to index...
 * The last one will be a ENDS_KMAP, and we skip over any preceding
 * ENDL_KMAP entries. Also allow for 0-based indexing, so -3.
 */

    int ki = key_index_allocated - 3;
    while (ki >= 0 && keytab[ki].k_type == ENDL_KMAP) ki--;
    kt_ents = ki + 1;
    idxsort_fields((unsigned char *)keytab, key_index,
         sizeof(struct key_tab), kt_ents, 1, &fdef);
    key_index_valid = 1;    /* This index is now usable... */
    keystr_index_valid = 0; /* ... But the other one isn't */
    return;
}

/* Dumping the key bindings needs to map the function to the keystroke
 * We'll index it rather than it having to do a linear search of each
 * item for every function.
 * Once we get here we know there are kt_ents entries.
 */
static int *keystr_index = NULL;
static int *next_keystr_index = NULL;
static void index_keystr(void) {
    keystr_index = Xrealloc(keystr_index, kt_ents*sizeof(int));
    struct fields fdef;
    fdef.offset = offsetof(struct key_tab, hndlr.k_fp);
    fdef.type = 'P';
    fdef.len = sizeof(fn_t);

    idxsort_fields((unsigned char *)keytab, keystr_index,
             sizeof(struct key_tab), kt_ents, 1, &fdef);

/* Having the index lets you find a matching entry. But we also want to
 * be able to find the next item after the one we have.
 * For that you need another index, with the key being the physical
 * (not logical) record number of your current item.
 * We can generate this from the index we've just made.
 * The final entry has a next of -1 to indicate "no further entry".
 */
    next_keystr_index = Xrealloc(next_keystr_index, kt_ents*sizeof(int));
    make_next_idx(keystr_index, next_keystr_index, kt_ents);
    keystr_index_valid = 1; /* This index is now usable */
    return;
}

/* A function to allow you to step through the index in order.
 * For each input record pointer, it returns the next one.
 * If given NULL, it will return the first item and when there are no
 * further entries it will return NULL.
 * If the pointer is out of range it will also return NULL.
 */
static struct key_tab *next_getbyfnc(struct key_tab *cp) {
    if (!keystr_index_valid) index_keystr();
    if (cp == NULL) return &keytab[keystr_index[0]];
/* Convert pointer to index and work from that... */
    int ci = cp - keytab;
    if ((ci >= 0) && (ci < kt_ents)) {
        int ni = next_keystr_index[ci];
        if (ni >= 0) return &keytab[ni];
    }
    return NULL;
}

/* This function looks up a function call in the binding table.
 */
struct key_tab *getbyfnc(fn_t func) {
    if (!keystr_index_valid) index_keystr();

/* Lookup by function call.
 * NOTE: that we use a binary chop that ensures we find the first
 * entry of any multiple ones.
 */
    int first = 0;
    int last = kt_ents - 1;
    int middle;

    while (first != last) {
        middle = (first + last)/2;
/* middle is too low, so try from middle + 1 */
        if ((void*)keytab[keystr_index[middle]].hndlr.k_fp < (void*)func)
            first = middle + 1;
/* middle is at or beyond start, so set last here */
        else last = middle;
    }
    if (keytab[keystr_index[first]].hndlr.k_fp != func) return NULL;
    return &keytab[keystr_index[first]];
}

/* This function looks a key binding up in the binding table
 * and returns the key_tab entry address.
 * This now returns the key_tab entry address, and callers have
 * been adjusted to work with this.
 * Note that we sort the keys as UNSIGNED (in index_bindings)
 * so we must do an unsigned comparison during the binary chop.
 *
 * int c;               key to find what is bound to it
 */
struct key_tab *getbind(int c) {

/* We don't want to re-index for each keybinding change when processing
 * start-up files.
 * So we have a flag for whether the index is up-to-date, and another
 * one as to whether we should update it or just do a linear search.
 */
    if (!key_index_valid) {
        if (pause_key_index_update) {
/* Just do a linear look through the key table. */
            struct key_tab *ktp;
            for (ktp = keytab; ktp->k_type != ENDL_KMAP; ++ktp) {
                if (ktp->k_code == c) return ktp;
            }
            return NULL;
        }
        index_bindings();
    }
/* Look through the key table. We can now binary-chop this...
 * NOTE: that we don't do a binary chop that ensures we find the first
 * entry of any multiple ones, as there can't be such entries!
 */
    int first = 0;
    int last = kt_ents - 1;
    int middle;

    while (first <= last) {
        middle = (first + last)/2;
        if ((unsigned)keytab[key_index[middle]].k_code < (unsigned)c)
             first = middle + 1;
        else if (keytab[key_index[middle]].k_code == c) break;
        else last = middle - 1;
    }
    if (first > last) {
        return NULL;        /* No such binding */
    }
    struct key_tab *res = &keytab[key_index[middle]];
    current_command = res->fi->n_name;
    return res;
}

/* unbindchar()
 *
 * int c;               command key to unbind
 */
static int unbindchar(int c) {
    struct key_tab *ktp;   /* pointer into the command table */
    struct key_tab *sktp;  /* saved pointer into the command table */

/* Search the table to see whether the key exists */

    ktp = getbind(c);

/* If it isn't bound, bitch */
    if (!ktp) return FALSE;

/* If this was a procedure mapping, free the buffer reference */
    if (ktp->k_type == PROC_KMAP) Xfree(ktp->hndlr.pbp);

/* save the pointer and scan to the end of the table */
    sktp = ktp;
    while (ktp->k_type != ENDL_KMAP) ++ktp;
    --ktp;                  /* backup to the last legit entry */

/* copy the last entry to the current one */
    *sktp = *ktp;           /* Copy the whole structure */

/* null out the last one */
    ktp->k_type = ENDL_KMAP;
    ktp->k_code = 0;
    ktp->hndlr.k_fp = NULL;
    ktp->fi = NULL;

    key_index_valid = 0;    /* Rebuild index before using it. */

    return TRUE;
}

/* ************************************************************
 * Actually add/update the key handler.
 * Used by buffertokey() and switch_internal()
 * We expect to be given a bname *including* the leading '/'.
 */
static int update_keybind(int c, int ntimes, int internal_OK,
     fn_t kfunc, char *bname) {
    struct key_tab *ktp;    /* pointer into the command table */
    struct key_tab *destp;  /* Where to copy the name and type */

/* switch_internal sets internal_OK on (and will have sent a valid
 * character).
 * So if this is off, check that no-one is trying to bind an internal
 * key "inadvertently".
 * Also, the non-internal calls may need to log something.
 */
    if (!internal_OK) {
/* Change it to something we can print as well and dump it out.
 * NOTE: that since desbind can now dump the entire binding table in
 * order, regardless of character ranges, we now allow non-ASCII
 * characters to be bound.
 */
        char outseq[80];
        cmdstr(c, outseq);
        switch(c) {
        case META|SPEC|'C':
        case META|SPEC|'R':
        case META|SPEC|'W':
        case META|SPEC|'X':
            mlwrite("%s is an internal binding. Use switch-internal.", outseq);
            return FALSE;
        }
        if (!clexec) mlputs(outseq);
        if (kbdmode == RECORD) addto_kbdmacro(outseq, 0, 0);
    }

/* Search the table to see whether it exists */

    ktp = getbind(c);
    if (ktp) {              /* If it exists, change it... (k_code is OK) */
        if (bname) {        /* user-proc install */
            if (ktp->k_type == FUNC_KMAP) /* Need to allocate space for name */
                ktp->hndlr.pbp = Xmalloc(NBUFN);
        }
        else {              /* function install */
            if (ktp->k_type == PROC_KMAP) { /* Need to free the name */
                Xfree(ktp->hndlr.pbp);      /* Free the name */
                ktp->hndlr.pbp = NULL;
            }
        }
        destp = ktp;
    }
    else {  /* ...else add a new one at the end */
        int ki;
        if (key_index_valid) {  /* We know the valid entry count */
            ki = kt_ents - 1;   /* The last used entry */
        }
        else {                  /* re-indexing is paused ... */
/* We have to search - quicker to search backwards from end */
            ki = keytab_alloc_ents - 2;   /* Skip ENDS_KMAP */
            while (ki >= 0 && keytab[ki].k_type == ENDL_KMAP) ki--;
        }
        ktp = &keytab[ki+1];    /* Step forward to an ENDL_KMAP to use... */
        destp = ktp;

/* If the list is not exhausted the next one will also be an End-of-List.
 * If it is an End-of-Structure we need to extend, and if we do that we
 * need to handle destp, in case the Xrealloc() moves things.
 * extend_keytab() fills in ENDL_KMAP and ENDS_KMAP entries.
 */
        ktp += 2;
        if (ktp->k_type == ENDS_KMAP) {
            ktp->k_type = ENDL_KMAP;        /* Change to end-of-list */
            int destp_offs = destp - keytab;
            extend_keytab(0);
            destp = keytab + destp_offs;
        }
        destp->k_code = c;          /* set keycode */
        if (bname)                  /* user-proc install */
            destp->hndlr.pbp = Xmalloc(NBUFN);
    }
    destp->bk_multiplier = ntimes;
    if (bname) {
        destp->k_type = PROC_KMAP;
        destp->fi = func_info(execproc);
        strcpy(destp->hndlr.pbp, bname+1);  /* Skip the leading '/' */
    }
    else {
        destp->k_type = FUNC_KMAP;      /* Set the type */
        destp->hndlr.k_fp = kfunc;      /* and the function pointer */
        destp->fi = func_info(kfunc);
    }
    mpresf = TRUE;                      /* GGR */
    TTflush();

    key_index_valid = 0;    /* Rebuild index before using it. */
    return TRUE;
}

/* Check for a procedure buffer and complain if not */
static int check_procbuf(struct buffer *cbp, char *bufn) {
    if (cbp->b_type != BTPROC) {
        mlforce("Buffer %s is not a procedure buffer.", bufn);
        sleep(1);
        return FALSE;
    }
    return TRUE;
}

/* GGR added
 * buffertokey:
 *      Add a new key to the key binding table to invoke a buffer.
 *      Much copied from bindtokey() and execproc()
 *
 * int f, n;            command arguments [IGNORED]
 */
int buffertokey(int f, int n) {
    UNUSED(f); UNUSED(n);
    unsigned int c;         /* command key to bind */
    char bname[NBUFN+1];    /* buffer name */
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */

/* Prompt the user to type in a key to bind */

    if (!clexec) {
        mlwrite_one(": buffer-to-key ");
        mpresf = TRUE;          /* GGR */
    }

/* Fudge in the tag, then get the name of the buffer to invoke.
 * The maximum length is 16 chars *including* the NUL.
 * mlreply (via either token or getstring) includes the NUL in the size
 * and we are actually writing this in from offset 1, so we allow NBUFN
 * chars for that and complain if the reply fills the buffer (as that will
 * make the name too long, and we don't want unexpected truncation meaning
 * we have two different names ending up the same).
 * Note that we DO NOT SEND the leading '/', which means that lookups
 * (the CMPLT_BUF) currently fail. This is so that command macro files
 * can use the same name as on the store-procedure line.
 */
    bname[0] = '/';
    if ((status =
         mlreply("user-proc name: ", bname+1, NBUFN, CMPLT_PROC)) != TRUE)
        return status;
    if (strlen(bname) >= NBUFN) {
         mlforce("Procedure name too long: %s. Ignored.", bname);
         sleep(1);
         return TRUE;       /* Don't abort start-up file */
    }

/* Check that this buffer exists */
    if ((bp = bfind(bname, FALSE, 0)) == NULL) {
        mlwrite("No such exec procedure %s", bname);
        return FALSE;
    }
    if (check_procbuf(bp, bname) != TRUE) return FALSE;

    if (!clexec) mlwrite_one("key sequence: ");

/* get the command sequence to bind */

    c = getckey(FALSE);
    if (c == 0) {
        mlwrite("Can't parse key for: %s: ", bname);
        return FALSE;
    }

    return update_keybind(c, n, FALSE, NULL, bname);
}

/* GGR addition to allow swapping in a user-procedure for one of the 4
 * "internal" ones. (META|SPEC|'x' for x = W, C, R or X.
 * This mechanism (execute a bound key) is probably simpler than trying
 * to remember a function and its option flags, for something that will
 * have infrequent usage at most.
 */
int switch_internal(int f, int n) {
    UNUSED(f); UNUSED(n);
    char bind_char[2];
    char uproc[NBUFN+1];
    int s;
    int bind_key;
    fn_t rpl_func;

/* Get char to change */

    if ((s = mlreply("Char to change [WCRX]: ", bind_char, 2, 0)) != TRUE) {
        return s;
    }
    if (bind_char[0] == '\0') return FALSE;
    char set_char = bind_char[0] & ~DIFCASE;    /* Quick ASCII upcase */
    switch(set_char) {
    case 'C':       /* Start of each "loop" in getstring()/main() */
    case 'R':       /* Before reading a file into a buffer (readin()) */
    case 'W':       /* When we need a word-wrap (main()/insert_newline()). */
    case 'X':       /* From cknewwindow(). Called when window switches. */
        break;
    default:
        mlwrite("Invalid choice: %c", set_char);
        return FALSE;
    };

/* Both getcmd() and stock() prevent META|SPEC being, so these are safe
 * from being overwritten by a key binding.
 */
    bind_key = META|SPEC|set_char;

/* Get user-proc to install */

    uproc[0] = '/';
    if ((s = mlreply("user-proc (dflt for reset): ",
             uproc+1, NBUFN, CMPLT_BUF)) != TRUE) {
        return s;
    }
    if (strcmp(uproc+1, "dflt") == 0) {     /* Reset */
        if (set_char == 'W') rpl_func = wrapword;
        else                 rpl_func = nullproc;
        s = update_keybind(bind_key, n, TRUE, rpl_func, NULL);
    }
    else {
        struct buffer *upb = bfind(uproc, FALSE, 0);
        if (!upb) {
            mlwrite("No such user procedure: %s", uproc+1);
            return FALSE;
        }
        if (check_procbuf(upb, uproc) != TRUE) return FALSE;
        s = update_keybind(bind_key, n, TRUE, NULL, uproc);
    }
    return s;
}

/* bindtokey:
 *      add a new key to the key binding table
 *
 * int f, n;            command arguments [IGNORED]
 */
int bindtokey(int f, int n) {
    UNUSED(f); UNUSED(n);
    unsigned int c;         /* command key to bind */
    fn_t kfunc;             /* ptr to the requested function to bind to */
    struct key_tab *ktp;    /* pointer into the command table */
    int mflag;              /* Are we handling a prefix key? */

/* Get the function name to bind it to */

    struct name_bind *nm_info = getname("bind-to-key - function: ", FALSE);
    if (nm_info == NULL) {
        if (!clexec) mlwrite_one(MLbkt("No such function"));
        return FALSE;
    }

/* You can't bind functions that can't be run interactively */

    if (nm_info->opt.not_interactive) {
        mlwrite("%s is non-interactive so may not be bound", nm_info->n_name);
        return FALSE;
    }

    kfunc = nm_info->n_func;
    if (!clexec) {
        char pbuf[NSTRING];
        sprintf(pbuf, "key for %s: ", nm_info->n_name);
        mlwrite(pbuf);
    }

/* Get the command sequence to bind */
    mflag = ((kfunc == metafn) || (kfunc == cex) ||
             (kfunc == unarg)  || (kfunc == ctrlg));
    c = getckey(mflag);
    if (c == 0) {
        mlwrite("Can't parse key for: %s: ", nm_info->n_name);
        return FALSE;
    }

/* If the function is a prefix key */
    if (mflag) {

/* Search for any/all existing bindings for the prefix key.
 * This is not indexed (rare search, and possible multiple entries per key,
 * so do a linear search.
 */
        ktp = keytab;
        while (ktp->k_type != ENDL_KMAP) {
            if ((ktp->k_type == FUNC_KMAP) && (ktp->hndlr.k_fp == kfunc))
                unbindchar(ktp->k_code);
            ++ktp;
        }

/* Reset the appropriate global prefix variable */

        if (kfunc == metafn)     metac = c;
        else if (kfunc == cex)   ctlxc = c;
        else if (kfunc == unarg) reptc = c;
        else                    abortc = c;    /* Only other option */
    }

    return update_keybind(c, n, FALSE, kfunc, NULL);
}

/* unbindkey:
 *      delete a key from the key binding table
 *
 * int f, n;            command arguments [IGNORED]
 */
int unbindkey(int f, int n) {
    UNUSED(f); UNUSED(n);
    int c;                  /* command key to unbind */
    char outseq[80];        /* output buffer for keystroke sequence */

/* Prompt the user to type in a key to unbind */
    if (!clexec) {
        mlwrite_one(": unbind-key ");
        mpresf = TRUE;      /* GGR */
    }

/* Get the command sequence to unbind */
    c = getckey(FALSE);     /* get a command sequence */
    if (c == 0) {
        mlwrite_one("Can't parse key string!");
        return FALSE;
    }

/* Change it to something we can print as well */
    cmdstr(c, outseq);

/* And dump it out */
    if (!clexec) mlputs(outseq);

/* If it isn't bound, bitch */
    if (unbindchar(c) == FALSE) {
        mlwrite_one(MLbkt("Key not bound"));
        mpresf = TRUE;  /* GGR */
        return FALSE;
    }
    TTflush();              /* GGR */

    return TRUE;
}

/* Function used to show the binding of a key */
static int show_key_binding(unicode_t key) {
    char outseq[80];
    int cpos;
    struct key_tab *ktp;
    if ((ktp = getbind(key))) {
        cmdstr(key, outseq);
        cpos = strlen(outseq);
/* Pad out some spaces */
        while (cpos < 12) outseq[cpos++] = ' ';
        if (ktp->bk_multiplier != 1) {  /* Mention a non-default multiplier */
            char tbuf[16];
            sprintf(tbuf, "{%d}", ktp->bk_multiplier);
            strcpy(outseq+cpos, tbuf);
            cpos += strlen(tbuf);
        }
        strcpy(outseq+cpos, ktp->fi->n_name);
/* Is this an execute-procedure? If so, say which procedure... */
        if (ktp->k_type == PROC_KMAP) {
            strcat(outseq, " ");
            strcat(outseq, ktp->hndlr.pbp);
        }
        strcat(outseq, "\n");
        if (linstr(outseq) != TRUE) return FALSE;
    }
    return TRUE;
}

/* build a binding list (limited or full)
 *
 * char *mstring;       match string for a partial list
 */
static int buildlist(char *mstring) {
    struct window *wp;         /* scanning pointer to windows */
    struct key_tab *ktp;       /* pointer into the command table */
    struct buffer *bp;         /* buffer to put binding list into */
    int cpos;                  /* current position to use in outseq */
    char outseq[80];           /* output buffer for keystroke sequence */

/* Split the current window to make room for the binding list */
    if (splitwind(FALSE, 1) == FALSE) return FALSE;

/* and get a buffer for it */
    bp = bfind("//Binding list", TRUE, BFINVS);
    if (bp == NULL || bclear(bp) == FALSE) {
        mlwrite_one("Cannot display binding list");
        return FALSE;
    }

/* Let us know this is in progress */
    mlwrite_one(MLbkt("Building binding list"));

/* Disconnect the current buffer */
    if (--curbp->b_nwnd == 0) curbp->b = curwp->w;  /* Last use */

/* Connect the current window to this buffer */
    curbp = bp;             /* make this buffer current in current window */
    bp->b_mode = 0;         /* no modes active in binding list */
    bp->b_nwnd++;           /* mark us as more in use */
    wp = curwp;
    wp->w_bufp = bp;
    wp->w_linep = bp->b_linep;
    wp->w_flag = WFHARD | WFFORCE;
    wp->w.dotp = bp->b.dotp;
    wp->w.doto = bp->b.doto;
    wp->w.markp = NULL;
    wp->w.marko = 0;

    for (int ni = nxti_name_info(-1); ni >= 0; ni = nxti_name_info(ni)) {

/* If we are executing an apropos command.....
 * ...and current string doesn't include the search string
 */
        if (mstring && !strstr(names[ni].n_name, mstring)) continue;

/* Add in the command name */
        strcpy(outseq, names[ni].n_name);
        cpos = strlen(outseq);

/* Search down for any keys bound to this. */
        ktp = getbyfnc(names[ni].n_func);
        while (ktp) {
/* Pad out some spaces */
            while (cpos < 28) outseq[cpos++] = ' ';

/* Add in the command sequence */
            cmdstr(ktp->k_code, outseq+cpos);
            strcat(outseq, "\n");

/* and add it as a line into the buffer */
            if (linstr(outseq) != TRUE) return FALSE;
            cpos = 0;       /* and clear the line */
/* Look for any more keybindings ot this function */
            ktp = next_getbyfnc(ktp);
            if (ktp->hndlr.k_fp != names[ni].n_func) ktp = NULL;
        }

/* if no key was bound, we need to dump it anyway */
        if (cpos > 0) {
            outseq[cpos++] = '\n';
            outseq[cpos] = 0;
            if (linstr(outseq) != TRUE) return FALSE;
        }
    }

/* Now we go through all the key_table looking for proc buf bindings.
 * There's no point trying to do these in any specific order, as they
 * aren't sorted by procedure buffer name (only by the char * that
 * points to it).
 */
    cpos = 0;
    int found = 0;
    for (ktp = keytab; ktp->k_type != ENDL_KMAP; ++ktp) {

/* If we are executing an apropos command.....
 * ...and current string doesn't include the search string
 */

        if (mstring && !strstr(ktp->hndlr.pbp, mstring)) continue;

        if (ktp->k_type == PROC_KMAP) {
            if (!found) {
                if (linstr("\nProcedure bindings\n") != TRUE) return FALSE;
                found = 1;
            }
        }
        else continue;
        strcpy(outseq, ktp->hndlr.pbp);
        cpos = strlen(outseq);
        while (cpos < 28) outseq[cpos++] = ' ';
/* Add in the command sequence */
        cmdstr(ktp->k_code, outseq+cpos);
        strcat(outseq, "\n");

/* Add the line into the buffer */
        if (linstr(outseq) != TRUE) return FALSE;
        cpos = 0;       /* and clear the line */
    }

/* Now, if this is not apropos, list everything in key-binding order,
 * with a heading for each "prefix-type".
 */
    if (!mstring) {
        if (linstr("\n\nAll key bindings:\n") != TRUE) return FALSE;

/* Note that getcmd() and stock() both prevent SPEC|META being set,
 * so mask any such as (internal)
 */
static char* hdr[] = {
    "Bare char", "Control",        "Meta",           "Meta+Ctrl",
    "CtlX",      "Ctlx+Ctrl",      "Ctlx+Meta",      "Ctlx+Meta+Ctrl",
    "SPEC",      "SPEC+Ctrl",      "Meta+SPEC(int)", "Meta+SPEC+Ctrl(int)",
    "CtlX+SPEC", "Ctlx+SPEC+Ctrl", "Ctlx+SPEC+Meta",
    "Ctlx+Meta+SPEC+Control(int)",
};

        unsigned int prev_cmask = -1;
        for (int i = 0; i < kt_ents; i++) {
            unicode_t key = keytab[key_index[i]].k_code;
            unsigned int cmask = 0xf0000000 & key;
            if (cmask != prev_cmask) {
                int hdri = cmask >> 28;
                linstr(" > ");
                linstr(hdr[hdri]);
                lnewline();
                prev_cmask = cmask;
            }
            int status = show_key_binding(key);
            if (!status) return status;
        }
    }

    curwp->w_bufp->b_mode |= MDVIEW;    /* put this buffer view mode */
    curbp->b_flag &= ~BFCHG;            /* don't flag this as a change */
    wp->w.dotp = lforw(bp->b_linep);    /* back to the beginning */
    wp->w.doto = 0;
    wp = wheadp;                        /* and update ALL mode lines */
    while (wp != NULL) {
        wp->w_flag |= WFMODE;
        wp = wp->w_wndp;
    }
    mlwrite_one("");                    /* clear the mode line */
    return TRUE;
}

/* describe bindings
 * bring up a fake buffer and list the key bindings
 * into it with view mode
 */
int desbind(int f, int n) {
        UNUSED(f); UNUSED(n);
        buildlist(NULL);
        return TRUE;
}

/* Apropos (List functions that match a substring) */
int apro(int f, int n) {

    UNUSED(f); UNUSED(n);
    char mstring[NSTRING];  /* string to match cmd names to */
    int status;             /* status return */

    status = mlreply("Apropos string: ", mstring, NSTRING - 1, CMPLT_NONE);
    if (status != TRUE) return status;

    return buildlist(mstring);
}

/* execute the startup file
 *
 * char *sfname;        name of startup file (null if default)
 */
int startup(char *sfname) {
    char *fname;            /* resulting file name to execute */

/* Look up the startup file */
    if (*sfname != 0) fname = flook(sfname, TRUE, INTABLE);
    else              fname = flook(init_files.startup, TRUE, INTABLE);

/* If it isn't around, don't sweat it */
    if (fname == NULL) return TRUE;

/* Otherwise, execute the sucker */
    return dofile(fname);
}

/* GGR - Define a path-separator
 */
static char path_sep = '/';

static int along_path(char *fname, char *fspec) {
    char *path;     /* environmental PATH variable */
    char *sp;       /* pointer into path spec */

/* get the PATH variable */
    path = getenv("PATH");
    if (path != NULL)
        while (*path) {

/* Build next possible file spec */
            sp = fspec;
            while (*path && (*path != PATHCHR)) *sp++ = *path++;

/* Add a terminating dir separator if we need it */
            if (sp != fspec) *sp++ = path_sep;
            *sp = 0;
            strcat(fspec, fname);

/* And try it out */
            if (ffropen(fspec) == FIOSUC) {
                ffclose();
                return TRUE;
            }

            if (*path == PATHCHR) ++path;
        }
    return FALSE;
}

/* GGR - function to set pathname from the command-line
 * This overrides the compiled-in defaults
 */
static int free_path_reqd = 0;
void set_pathname(char *cl_string) {
    int slen;
    int add_sep = 0;
    if (path_sep != '\0') {     /* Ensure it ends with it... */
        slen = strlen(cl_string);
        if ((slen > 0) && (cl_string[slen-1] != path_sep)) {
            add_sep = 1;
        }
    }
/* Have we been here before?
 * If so, free what we got then.
 * NOTE: that you can only end up with 1 path set by using the -d
 * command-line option (even though the compiled in default has two).
 * The last one wins.
 */
    if (free_path_reqd) Xfree(pathname[0]);
    pathname[0] = Xstrdup(cl_string);
    free_path_reqd = 1;
    if (add_sep) {
        pathname[0] = Xrealloc(pathname[0], slen+2); /* incl. NULL! */
        pathname[0][slen] = path_sep;
        pathname[0][slen+1] = '\0';
    }
    pathname[1] = NULL;
    return;
}

/* Look up the existence of a file along the normal or PATH
 * environment variable. Look first in the HOME directory if
 * asked and possible.
 * GGR - added mode flag determines whether to look along PATH
 * or in the set list of directories.
 *
 * char *fname;         base file name to search for
 * int hflag;           Look in the HOME environment variable first?
 */
char *flook(char *fname, int hflag, int mode) {
    char *home;                     /* path to home directory */
    int i;                          /* index */
    static char fspec[NSTRING];     /* full path spec to search */

    pathexpand = FALSE;             /* GGR */

/* If we've been given a pathname, rather than a filename, just use that */

    if (strchr(fname, path_sep)) {
        char *res = NULL;   /* Assume the worst */
        if (ffropen(fname) == FIOSUC) {
            ffclose();
            res = fname;
        }
        pathexpand = TRUE;  /* GGR */
        return res;
    }

    if (hflag) {
        home = getenv("HOME");
        if (home != NULL) {     /* build home dir file spec */
            strcpy(fspec, home);
            char psbuf[2];
            psbuf[0] = path_sep;
            psbuf[1] = '\0';
            strcat(fspec, psbuf);
            strcat(fspec, fname);

            if (ffropen(fspec) == FIOSUC) { /* and try it out */
                ffclose();
                pathexpand = TRUE;  /* GGR */
                return fspec;
            }
        }
    }

/* Always try the current directory first - if allowed */
    if (allow_current && ffropen(fname) == FIOSUC) {
        ffclose();
        pathexpand = TRUE;                  /* GGR */
        return fname;
    }

/* GGR - use PATH or TABLE according to mode.
 * You want to use ONPATH when looking for a command, but INTABLE
 * when looking for a config files.
 * The caller knows which...
 */
    if (mode == ONPATH) {
        if (along_path(fname, fspec) == TRUE) {
            pathexpand = TRUE;                  /* GGR */
            return fspec;
        }
    }

    if (mode == INTABLE) {  /* look it up via the old table method */
        for (i = 0;; i++) {
            if (pathname[i] == NULL) break;
            strcpy(fspec, pathname[i]);
            strcat(fspec, fname);
            if (ffropen(fspec) == FIOSUC) {    /* and try it out */
                ffclose();
                pathexpand = TRUE;          /* GGR */
                return fspec;
            }
        }
    }

    pathexpand = TRUE;                          /* GGR */
    return NULL;            /* no such luck */
}

/* change a key command to a string we can print out
 *
 * int c;               sequence to translate
 * char *seq;           destination string for sequence
 */
void cmdstr(int c, char *seq) {
    char *ptr;      /* pointer into current position in sequence */

    ptr = seq;

    if (c & CTLX) {     /* apply ^X sequence if needed */
        *ptr++ = '^';
        *ptr++ = 'X';
    }
    if (c & META) {     /* apply meta sequence if needed */
        *ptr++ = 'M';
        *ptr++ = '-';
    }
    if (c & SPEC) {     /* apply SPEC sequence if needed */
        *ptr++ = 'F';
        *ptr++ = 'N';
    }
    if (c & CONTROL) {  /* apply control sequence if needed */
        *ptr++ = '^';
    }

/* Strip the prefixes and output the final sequence */

    c &= 0x0fffffff;
    if (c  == ' ') {        /* Handle the "SP" for space which we allow */
        *ptr++ = 'S';
        *ptr++ = 'P';
    }
    else if (c == 0x7f) {   /* Handle "^?" for delete */
        *ptr++ = '^';
        *ptr++ = '?';
    }
    else {
/* If we have a non-ASCII char we need to convert it to utf8.... */
        if (c < 0x80)  *ptr++ = c & 255;
        else            ptr += unicode_to_utf8(c, ptr);
    }
    *ptr = 0;               /* terminate the string */
}

/* string key name to binding name....
 *
 * char *skey;          name of key to get binding for
 */
char *transbind(char *skey) {
    struct key_tab *ktp = getbind(stock(skey));
    if (!ktp) return "ERROR";
    return ktp->fi->n_name;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_bind(void) {
/* Free any hndlr.pbp for BTPROC entries before we get rid of any
 * indexes
 */
    for (struct key_tab *ktp = keytab; ktp->k_type != ENDL_KMAP; ++ktp) {
        if (ktp->k_type == PROC_KMAP) Xfree(ktp->hndlr.pbp);
    }
    if (keytab_alloc_ents) Xfree(keytab);
    if (key_index) Xfree(key_index);
    if (keystr_index) Xfree(keystr_index);
    if (next_keystr_index) Xfree(next_keystr_index);
    if (free_path_reqd) Xfree(pathname[0]);
    return;
}
#endif
