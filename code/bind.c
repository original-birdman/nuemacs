/*      bind.c
 *
 *      This file is for functions having to do with key bindings,
 *      descriptions, help commands and startup file.
 *
 *      Written 11-feb-86 by Daniel Lawrence
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

int help(int f, int n) {    /* give me some help!!!!
                               bring up a fake buffer and read the help file
                               into it with view mode                 */
    UNUSED(f); UNUSED(n);
    struct window *wp;      /* scaning pointer to windows */
    struct buffer *bp;      /* buffer pointer to help */
    char *fname = NULL;     /* ptr to file returned by flook() */

/* First check if we are already here */
    bp = bfind(init_files.help, FALSE, BFINVS); /* GGR - epath.h setting */

    if (bp == NULL) {
        fname = flook(init_files.help, FALSE, INTABLE);
        if (fname == NULL) {
            mlwrite_one(MLbkt("Help file is not online"));
            return FALSE;
            }
    }

/* Split the current window to make room for the help stuff */
    if (splitwind(FALSE, 1) == FALSE) return FALSE;

    if (bp == NULL) {   /* and read the stuff in */
        pathexpand = FALSE; /* GGR - unset pathexpand around call */
        int res = getfile(fname, FALSE, FALSE);
        pathexpand = TRUE;
        if (res == FALSE) return(FALSE);
    } else
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

/*
 * stock:
 *      String key name TO Command Key
 *      We need to be careful about signedness in comparisons!!
 *
 * char *keyname;       name of key to translate to Command key from
 */
static unsigned int stock(char *keyname) {
    unsigned int c; /* key sequence to return */

/* GGR - allow different bindings for 1char UPPER and lower */
    int kn_len = strlen(keyname);
    int noupper = (kn_len == 1);
    char *kn_end = keyname + kn_len;

/* Parse it up */
    c = 0;

/* First, the META prefix */
    if (*keyname == 'M' && *(keyname + 1) == '-') {
        c = META;
        keyname += 2;
    }
/* Next the function prefix */
    if (*keyname == 'F' && *(keyname + 1) == 'N') {
        c |= SPEC;
        keyname += 2;
    }
/* Control-x as well... (but not with FN *OR* META!!) */
    if (*keyname == '^' && *(keyname + 1) == 'X'
          && !(c & SPEC) && !(c & META)) {
        c |= CTLX;
        keyname += 2;
    }

/* A control char? */
    if (*keyname == '^' && *(keyname + 1) != 0) {
        if (*(keyname + 1) == '?')  /* Special case ^? for Delete */
            c = 0x7f;
        else
            c |= CONTROL;
        ++keyname;
    }
    if (ch_as_uc(*keyname) < 32) {
        c |= CONTROL;
        *keyname += 'A';
    }
/* GGR - allow SP for space by putting it there... */
    if (*keyname == 'S' && *(keyname + 1) == 'P') {
        ++keyname;
        *keyname = ' ';
    }

/* Make sure we are not lower case (not with function keys) */
    if (ch_as_uc(*keyname) >= 'a' && ch_as_uc(*keyname) <= 'z' && !(c & SPEC)
          && !(noupper))            /* GGR */
        *keyname -= 32;

    if (ch_as_uc(*keyname) >= 0xc0) {   /* We have a utf-8 string... */
        unsigned int uc;
        int kn_left = kn_end - keyname;
        utf8_to_unicode(keyname, 0, kn_left, &uc);
        uc = utf8proc_toupper(uc);      /* Unchanged if not lower/title case */
        c |= uc;        /* The final tagged char... */
    }
    else
        c |= *keyname;  /* The final sequence... */

    return c;
}

/*
 * get a command key sequence from the keyboard
 *
 * int mflag;           going for a meta sequence?
 */
static unsigned int getckey(int mflag) {
    unsigned int c;         /* character fetched */
    char tok[NSTRING];      /* command incoming */

/* Check to see if we are executing a command line */
    if (clexec) {
        macarg(tok);    /* get the next token */
        return stock(tok);
    }

/* Or the normal way */
    if (mflag) c = get1key();
    else       c = getcmd();
    return c;
}

int deskey(int f, int n) {      /* describe the command for a certain key */
    UNUSED(f); UNUSED(n);
    int c;          /* key to describe */
    char *ptr;      /* string pointer to scan output strings */
    char outseq[NSTRING];   /* output buffer for command sequence */

/* Prompt the user to type us a key to describe */
    mlwrite_one(": describe-key ");

/* Get the command sequence to describe.
 * Change it to something we can print as well and dump it out.
 */
    cmdstr(c = getckey(FALSE), outseq);
    mlputs(outseq);
    mlputs(" ");

/* Find the right function */
    struct key_tab *ktp = getbind(c);
    if (!ktp) ptr = "Not Bound";
    else      ptr = ktp->fi->n_name;
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

static void index_bindings(void) {
    if (key_index_allocated < keytab_alloc_ents) {
        key_index = Xrealloc(key_index, keytab_alloc_ents*sizeof(int));
        key_index_allocated = keytab_alloc_ents;
    }
    struct fields fdef;
    fdef.offset = offsetof(struct key_tab, k_code);
    fdef.type = 'I';
    fdef.len = sizeof(int);

/* The allocated keytab contains markers at the end, which we do not
 * want to index...
 * The last one will be a ENDS_KMAP, and we skip over any preceding
 * ENDL_KMAP entries.
 */

    int ki = key_index_allocated - 2;
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
struct key_tab *next_getbyfnc(struct key_tab *cp) {
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


/*
 * This function looks a key binding up in the binding table
 * and returns the key_tab entry address.
 * This now returns the key_tab entry address, and callers have
 * been adjusted to work with this.
 *
 * int c;               key to find what is bound to it
 */
struct key_tab *getbind(int c) {

/* We don't want to re-index for each keybinding change when processing
 * start-up files.
 * So we have a flag for whether the index is up-to-date, and another
 * one as to whether we should update it or just do a linear seach.
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
    int middle = (first + last)/2;

    while (first <= last) {
        if (keytab[key_index[middle]].k_code < c) first = middle + 1;
        else if (keytab[key_index[middle]].k_code == c) break;
        else last = middle - 1;
        middle = (first + last)/2;
    }
    if (first > last) {
        return NULL;        /* No such binding */
    }
    struct key_tab *res = &keytab[key_index[middle]];
    current_command = res->fi->n_name;
    return res;
}

/*
 * unbindchar()
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
    if (ktp->k_type == PROC_KMAP) free(ktp->hndlr.pbp);

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

/*
 * bindtokey:
 *      add a new key to the key binding table
 *
 * int f, n;            command arguments [IGNORED]
 */
int bindtokey(int f, int n) {
    UNUSED(f); UNUSED(n);
    unsigned int c;         /* command key to bind */
    fn_t kfunc;             /* ptr to the requested function to bind to */
    struct key_tab *ktp;    /* pointer into the command table */
    char outseq[80];        /* output buffer for keystroke sequence */
    struct key_tab *destp;  /* Where to copy the name and type */
    int mflag;              /* Are we handling a prefix key? */

/* Get the function name to bind it to */

    struct name_bind *nm_info = getname(": bind-to-key ");
    if (nm_info == NULL) {
        if (!clexec) mlwrite_one(MLbkt("No such function"));
        return FALSE;
    }
    kfunc = nm_info->n_func;
    if (!clexec) mlputs(" ");

/* Get the command sequence to bind */
    mflag = ((kfunc == metafn) || (kfunc == cex) ||
             (kfunc == unarg)  || (kfunc == ctrlg));
    c = getckey(mflag);

/* Change it to something we can print as well */
    cmdstr(c, outseq);

/* And dump it out */
    if (!clexec) mlputs(outseq);

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

/* Search the table to see whether it exists */

    ktp = getbind(c);
    if (ktp) {              /* If it exists, change it... */
        if (ktp->k_type == PROC_KMAP) { /* Need to free the name */
            free(ktp->hndlr.pbp);       /* Free the name */
            ktp->hndlr.pbp = NULL;
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
        ktp = &keytab[ki+1];
        ktp->k_code = c;    /* set keycode */
        destp = ktp;

/* The next entry will alway be an End-of-List.
 * If the list is not exhausted the one after will also be an End-of-List.
 * If it is an End-of-Structure we need to extend, and if we do that we
 * need to handle destp, in case the Xrealloc() moves things.
 * extend_keytab() fills in ENDL_KMAP and ENDS_KMAP entries.
 */
        ktp += 2;
        if (ktp->k_type == ENDS_KMAP) {
            ktp->k_type = ENDL_KMAP;        /* Change to end-of-list */
            int destp_offs = destp - keytab;
            int ktp_offs = ktp - keytab;
            extend_keytab(0);
            destp = keytab + destp_offs;
            ktp = keytab + ktp_offs;
        }
    }
    destp->k_type = FUNC_KMAP;      /* Set the type */
    destp->hndlr.k_fp = kfunc;      /* and the function pointer */
    destp->fi = nm_info;

    mpresf = TRUE;                  /* GGR */
    TTflush();

    key_index_valid = 0;            /* Rebuild index before using it. */

    return TRUE;
}

/*
 * unbindkey:
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

/*
 * build a binding list (limited or full)
 *
 * int type;            true = full list,   false = partial list
 * char *mstring;       match string if a partial list
 */
static int buildlist(int type, char *mstring) {
    struct window *wp;         /* scanning pointer to windows */
    struct key_tab *ktp;       /* pointer into the command table */
    struct buffer *bp;         /* buffer to put binding list into */
    int cpos;                  /* current position to use in outseq */
    char outseq[80];           /* output buffer for keystroke sequence */

/* Split the current window to make room for the binding list */
    if (splitwind(FALSE, 1) == FALSE) return FALSE;

/* and get a buffer for it */
    bp = bfind("/Binding list", TRUE, 0);
    if (bp == NULL || bclear(bp) == FALSE) {
        mlwrite_one("Cannot display binding list");
        return FALSE;
    }

/* Let us know this is in progress */
    mlwrite_one(MLbkt("Building binding list"));

/* Disconnect the current buffer */
    if (--curbp->b_nwnd == 0) {     /* Last use.            */
        curbp->b_dotp = curwp->w_dotp;
        curbp->b_doto = curwp->w_doto;
        curbp->b_markp = curwp->w_markp;
        curbp->b_marko = curwp->w_marko;
        curbp->b_fcol = curwp->w_fcol;
    }

/* Connect the current window to this buffer */
    curbp = bp;             /* make this buffer current in current window */
    bp->b_mode = 0;         /* no modes active in binding list */
    bp->b_nwnd++;           /* mark us as more in use */
    wp = curwp;
    wp->w_bufp = bp;
    wp->w_linep = bp->b_linep;
    wp->w_flag = WFHARD | WFFORCE;
    wp->w_dotp = bp->b_dotp;
    wp->w_doto = bp->b_doto;
    wp->w_markp = NULL;
    wp->w_marko = 0;

    for (int ni = nxti_name_info(-1); ni >= 0; ni = nxti_name_info(ni)) {
/* Add in the command name */
        strcpy(outseq, names[ni].n_name);
        cpos = strlen(outseq);

/* If we are executing an apropos command.....
 * ...and current string doesn't include the search string */
        if (type == FALSE && strstr(outseq, mstring) == FALSE) goto fail;

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

/* ...and on to the next name */
fail:   ;
    }

/* Now we go through all the key_table looking for proc buf bindings.
 * There's no point trying to do these in any specific order, as they
 * aren't sorted by procedure buffer name (only by the char * that
 * points to it).
 */
    cpos = 0;
    int found = 0;
    for (ktp = keytab; ktp->k_type != ENDL_KMAP; ++ktp) {
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

    curwp->w_bufp->b_mode |= MDVIEW;    /* put this buffer view mode */
    curbp->b_flag &= ~BFCHG;            /* don't flag this as a change */
    wp->w_dotp = lforw(bp->b_linep);    /* back to the beginning */
    wp->w_doto = 0;
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
        buildlist(TRUE, "");
        return TRUE;
}

/* Apropos (List functions that match a substring) */
int apro(int f, int n) {

    UNUSED(f); UNUSED(n);
    char mstring[NSTRING];  /* string to match cmd names to */
    int status;             /* status return */

    status = mlreply("Apropos string: ", mstring, NSTRING - 1, EXPNONE);
    if (status != TRUE) return status;

    return buildlist(FALSE, mstring);
}

/*
 * execute the startup file
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
static char path_sep =
#if BSD | USG
    '/'
#else
    '\0'
#endif
;

#if ENVFUNC
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
#endif

/* GGR - function to set pathname from the command-line
 * This overides the compiled-in defaults
 */
void set_pathname(char *cl_string) {
    int slen;
    int add_sep = 0;
    if (path_sep != '\0') {     /* Ensure it ends with it... */
        slen = strlen(cl_string);
        if ((slen > 0) && (cl_string[slen-1] != path_sep)) {
            add_sep = 1;
        }
    }
    pathname[0] = strdup(cl_string);
    if (add_sep) {
        pathname[0] = Xrealloc(pathname[0], slen+2); /* incl. NULL! */
        pathname[0][slen] = path_sep;
        pathname[0][slen+1] = '\0';
    }
    pathname[1] = NULL;
    return;
}

/*
 * Look up the existence of a file along the normal or PATH
 * environment variable. Look first in the HOME directory if
 * asked and possible.
 * GGR - added mode flag detemines whether to look along PATH
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

#if ENVFUNC
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
#endif

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

/*
 * change a key command to a string we can print out
 *
 * int c;               sequence to translate
 * char *seq;           destination string for sequence
 */
void cmdstr(int c, char *seq) {
    char *ptr;      /* pointer into current position in sequence */

    ptr = seq;

    if (c & META) {     /* apply meta sequence if needed */
        *ptr++ = 'M';
        *ptr++ = '-';
    }
    if (c & CTLX) {     /* apply ^X sequence if needed */
        *ptr++ = '^';
        *ptr++ = 'X';
    }
    if (c & SPEC) {     /* apply SPEC sequence if needed */
        *ptr++ = 'F';
        *ptr++ = 'N';
    }
    if (c & CONTROL) {  /* apply control sequence if needed */
        *ptr++ = '^';
    }

/* And output the final sequence */

    if ((c & 255) == ' ') { /* Handle the "SP" for space which we allow */
        *ptr++ = 'S';
        *ptr++ = 'P';
    }
    else if (c == 0x7f) {   /* Handle "^?" for delete */
        *ptr++ = '^';
        *ptr++ = '?';
    }
    else
        *ptr++ = c & 255;   /* strip the prefixes */

    *ptr = 0;               /* terminate the string */
}

/*
 * string key name to binding name....
 *
 * char *skey;          name of key to get binding for
 */
char *transbind(char *skey) {
    struct key_tab *ktp = getbind(stock(skey));
    if (!ktp) return "ERROR";
    return ktp->fi->n_name;
}

/* GGR added
 * buffertokey:
 *      Add a new key to the key binding table to invoke a buffer.
 *      Much copied from bindtokey B()and execproc()
 *
 * int f, n;            command arguments [IGNORED]
 */
int buffertokey(int f, int n) {
    UNUSED(f); UNUSED(n);
    unsigned int c;         /* command key to bind */
    struct key_tab *ktp;    /* pointer into the command table */
    char bname[NBUFN+1];    /* buffer name */
    char outseq[80];        /* output buffer for keystroke sequence */
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    struct key_tab *destp;  /* Where to copy the name and type */

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
 * we have two different names ending up the same.
 */
    bname[0] = '/';
    if ((status = mlreply("macro buffer: ", bname+1, NBUFN, EXPBUF)) != TRUE)
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

    if (!clexec) mlputs("key sequence: ");

/* get the command sequence to bind */

    c = getckey(0);

/* Change it to something we can print as well and dump it out */

    cmdstr(c, outseq);
    if (!clexec) mlputs(outseq);

/* Search the table to see whether it exists */

    ktp = getbind(c);
    if (ktp) {              /* If it exists, change it... */
        if (ktp->k_type == FUNC_KMAP) /* Need to allocate space for name */
            ktp->hndlr.pbp = Xmalloc(NBUFN);
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
        ktp->k_code = c;        /* set keycode */
        ktp->hndlr.pbp = Xmalloc(NBUFN);
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
            int ktp_offs = ktp - keytab;
            extend_keytab(0);
            destp = keytab + destp_offs;
            ktp = keytab + ktp_offs;
        }
    }
    destp->k_type = PROC_KMAP;
    destp->fi = func_info(execproc);
    strcpy(destp->hndlr.pbp, bname+1); /* ...and copy in name */

    mpresf = TRUE;                  /* GGR */
    TTflush();

    key_index_valid = 0;    /* Rebuild index before using it. */

    return TRUE;
}
