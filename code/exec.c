/*      exec.c
 *
 *      This file is for functions dealing with execution of
 *      commands, command lines, buffers, files and startup files.
 *
 *      written 1986 by Daniel Lawrence
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#define EXEC_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

#include "utf8proc.h"

/* Buffer to store macro text to. Recursion is not allowed */
struct buffer *bstore = NULL;

static int execlevel = 0;
static int macro_level = 0;

/* We only need one of these, as it is only set as we leave,
 * and there is only one "previous Line".
 */
static char *prev_line_seen = NULL;

#ifdef DO_FREE
/* We only need these for valgrind testing */

/** pending_line_seen doesn't seem to need it an array*/
static char *pending_line_seen = NULL;
static db *pending_docmd_tknp = NULL;
static db *pending_dobuf_tknp = NULL;
static db *pending_golabel = NULL;

static linked_items *pending_einit_headp = NULL;
static linked_items *pending_whlist_headp = NULL;

#endif

/* The next two functions have a **arg1 so that they can update the
 * headp themselves.
 */

/* add_to_head
 * Put the item in at the head of a linked_items list
 */
static void add_to_head(linked_items **headp, void *item) {
    linked_items *np = Xmalloc(sizeof(linked_items));   /* Get new one */
    np->next = *headp;                                  /* Link orig first */
    *headp = np;                                        /* Make new first */
    np->item = item;                                    /* Add data */
}
/* pop_head
 * Remove the head item from a linked_items list
 * This does NOT free any item data!
 */
static void pop_head(linked_items **headp) {
    linked_items *op = *headp;              /* Remember first */
    *headp = op->next;                      /* Make second the new first */
    Xfree(op);                              /* Free old first */
}

/* token:
 *      chop a token off a string
 *      return a pointer past the token
 *
 * char *src,       source string
 * db *tok;       destination token dynamic string
 */
char *token(char *src, db *tok) {
    int quotef;     /* is the current string quoted? */
    char c;         /* temporary character */

    dbp_clear(tok); /* Start with nothing */

/* First scan past any whitespace in the source string */
    while (*src == ' ' || *src == '\t') ++src;

/* Scan through the source string.
 * DO record a " IFF the first character
 * Then getval() knows it is a string (TKSTR) and returns
 * the rest of the buffer as the string value.
 * So "$fillcol" -> "$fillcol, and getval() will treat it (without the
 *  leading ") as a TKSTR
 * But %"My Var" -> %My Var and getval() will treat it as a user variable
 * with a space in its name.
 * The terminating " is NOT added...neither are any "s along the way.
 */
    if (*src == '"') {
        quotef = TRUE;
        src++;
        dbp_addch(tok, '"');
    }
    else quotef = FALSE;
    while (*src) {      /* process special characters */
        if (*src == '~') {
            src++;
            if (*src == 0) break;
            switch (*src++) {
            case 'r':   c = 13; break;
            case 'n':   c = 10; break;
            case 't':   c = 9;  break;
            case 'b':   c = 8;  break;
            case 'f':   c = 12; break;
            default:    c = *(src - 1);
            }
        }
        else {      /* check for the end of the token */
            c = *src++;
            if (quotef) {
                if (c == '"') break;
            }
            else {
                if (c == ' ' || c == '\t') break;
            }

/* Set quote mode if quote found */
            if (c == '"') {
                quotef = TRUE;
                continue;   /* Don't record it...only in pos1 is it kept */
            }

/* Record the character */
        }
        dbp_addch(tok, c);
    }

    return src;
}

/* nextarg:
 *      get the next argument
 *
 * char *prompt             prompt to use if we must be interactive
 * char *buffer             buffer to put token into
 * int size                 size of the buffer
 * int ctype                type of context completion if we prompt
 */
int nextarg(char *prompt, db *buffer, enum cmplt_type ctype) {

/* If we are interactive, go get it! */
    if (clexec == FALSE) return getstring(prompt, buffer, ctype);

/* Grab token and advance past */
    execstr = token(execstr, buffer);

/* Evaluate it */
/* GGR - There is the possibility of an illegal overlap of args here.
 *       Or a copy to iself.
 *       So it must be done via a temporary buffer.
 */
    dbp_set(buffer, strdupa(getval(dbp_val(buffer))));
    return TRUE;
}

/* get a macro line argument
 *
 * char *tok;           buffer to place argument
 */
int macarg(db *tok) {
    int savcle;             /* buffer to store original clexec */
    int status;

    savcle = clexec;        /* save execution mode */
    clexec = TRUE;          /* get the argument */
    status = nextarg("", tok, CMPLT_NONE);
    clexec = savcle;        /* restore execution mode */
    return status;
}

/* docmd:
 *      take a passed string as a command line and translate
 *      it to be executed as a command. This function will be
 *      used by execute-command-line and by all source and
 *      startup files.
 *
 *      format of the command line is:
 *
 *              {# arg} <command-name> {<argument string(s)>}
 *
 * char *cline;         command line to execute
 */
static int docmd(char *cline) {
    int f;                  /* default argument flag */
    ue64I_t n;              /* numeric repeat value */
    int status;             /* return status of function */
    int oldcle;             /* old contents of clexec flag */
    char *oldestr;          /* original exec string */
    db_def(tkn);            /* next token off of command line */

/* If we are scanning and not executing..go back here */
    if (execlevel) return TRUE;

    oldestr = execstr;      /* save last ptr to string to execute */
    execstr = cline;        /* and set this one as current */

/* We need to take a copy of the current command line now.
 * The token parsing writes NULs into the buffer as it goes...
 * This means we need a single point of exit, to ensure that
 * this gets freed correctly.
 */
    char *this_line_seen = Xstrdup(cline);

/* First set up the default command values */
    f = FALSE;
    n = 1;

    if ((status = macarg(&tkn)) != TRUE) {  /* and grab the first token */
        goto final_exit;
    }

/* If we have an interactive arg, variable or function, evaluate it.
 * We may wish to send in a numeric arg that way!
 *
 * NOTE!!! We can only pass in ONE token this way!
 */
    int ttype = gettyp(db_val(tkn));
    switch(ttype) {
    case TKARG:
    case TKENV:
    case TKVAR:
    case TKFUN:
    case TKBVR:
        db_set(tkn, strdupa(getval(db_val(tkn))));
        ttype = gettyp(db_val(tkn));    /* What we have in tkn now... */
    };

/* Process leading argument */
    if (ttype == TKLIT) {
        f = TRUE;
        n = strtoll(db_val(tkn), NULL, 10);

/* and now get the command to execute */
        if ((status = macarg(&tkn)) != TRUE) {
            goto final_exit;
        }
    }

/* If the command is "reexecute" we need to recurse with the previous
 * command line.
 * We also need to preserve the previous line as the current line,
 * which switch_lsb_lines() does - the reexecute gets sent to prev and
 * is then removed rather than the command we actually want.
 * Any command we execute *could* reset prev_line_seen (by running
 * another buffer of commands...) so make sure that prev_line_seen
 * is set correctly at the end by swapping our copy into place.
 * We actually run with prev_line_seen rather than our this_line_seen copy
 * as that will get tokenized...
 */
    if (strcmp(db_val(tkn), "reexecute") == 0) {
        Xfree(this_line_seen);   /* Drop the "reexecute" */
        this_line_seen = Xstrdup(prev_line_seen);
        status = TRUE;
        while (n-- && status) status = docmd(prev_line_seen);
        goto remember_cmd;
    }

/* And match the token to see if it exists */
    struct name_bind *nbp = name_info(db_val(tkn));
    if (nbp == NULL) {
        mlwrite("No such Function: %s", db_val(tkn));
        status = FALSE;
        goto final_exit;
    }

/* Save the arguments and go execute the command */
    oldcle = clexec;        /* save old clexec flag */
    clexec = TRUE;          /* in cline execution */
    current_command = nbp->n_name;

/* If this command is exit-emacs we'll never get back and
 * never free this_line_seen.
 * So valgrind reports it.
 * Add it to the free_exec list...
 */
#ifdef DO_FREE
    pending_line_seen = this_line_seen;
    pending_docmd_tknp = &tkn;
#endif
    status = (nbp->n_func)(f, n); /* call the function */
#ifdef DO_FREE
    pending_line_seen = NULL;
    pending_docmd_tknp = NULL;
#endif
    if (!nbp->opt.search_ok) srch_can_hunt = 0;
    com_flag &= nbp->keep_flags;
    cmdstatus = status;     /* save the status */
    clexec = oldcle;        /* restore clexec flag */

/* "Remember command" exit.
 * {} needed because we start with a declaration, not statement.
 *
 * NOTE that we drop into the remember_cmd: block from the "normal"
 * code above.
 *
 * this_line_seen is now the command that any reexecute should reexecute
 * in this function, so set that....
 * Swap these two so that the one we want is saved and the other is freed.
 */
remember_cmd:
    { char *ts = prev_line_seen;
      prev_line_seen = this_line_seen;
      this_line_seen = ts;
    }
/* "Just tidy up..." exit */
final_exit:
    Xfree(this_line_seen);
    db_free(tkn);
    execstr = oldestr;
    return status;
}

/* Execute a named command even if it is not bound.
 */
static fn_t last_ncfunc = NULL;
static int this_can_hunt = 0;
static int this_keep_flags = CFNONE;
int namedcmd(int f, int n) {
    fn_t kfunc;     /* ptr to the requested function to bind to */

/* Re-use last obtained function? */
    if (inreex && last_ncfunc && RXARG(namedcmd))
        kfunc = last_ncfunc;
    else {          /* Prompt the user to get the function name to execute */
        struct name_bind *nm_info = getname("name: ", TRUE);
        if (nm_info == NULL) return FALSE;
        kfunc = nm_info->n_func;
        this_can_hunt = nm_info->opt.search_ok;

/* Check whether the given command is allowed in the minibuffer, if that
 * is where we are...
 */
        if (inmb) {
            if (nm_info->opt.not_mb) {
                not_in_mb.funcname = nm_info->n_name;
                not_in_mb.keystroke = -1;   /* No keystroke... */
                kfunc = not_in_mb_error;    /* Change what we call... */
            }
        }
        if (!clexec) {
            if (nm_info->opt.not_interactive) {
                not_interactive_fname = nm_info->n_name;
                kfunc = not_interactive;    /* Change what we call... */
            }
        }
        if (!nm_info->opt.search_ok) srch_can_hunt = 0;
        this_keep_flags = nm_info->keep_flags;
    }

/* ...and then execute the command */
    int status = kfunc(f, n);
    com_flag &= this_keep_flags;
    if (!this_can_hunt) srch_can_hunt = 0;
    last_ncfunc = kfunc;        /* Now we remember this... */
    return status;
}

/* Buffer name for reexecute - shared by all command-callers */

static db_def(prev_cmd);

/* execcmd:
 *      Execute a command line command to be typed in
 *      by the user
 *
 * int f, n;            default Flag and Numeric argument
 */
int execcmd(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;             /* status return */
    db_def(thecmd);         /* string holding command to execute */

/* Re-use last obtained command? */
    if (inreex && (db_charat(prev_cmd, 0) != '\0') && RXARG(execcmd))
        db_set(thecmd, db_val(prev_cmd));
    else {
/* Get the line wanted */
        if ((status =
             mlreply("command: ", &thecmd, CMPLT_NONE)) != TRUE)
            goto exit;
    }
    execlevel = 0;
    status = docmd(db_val(thecmd));
    db_set(prev_cmd, db_val(thecmd)); /* Now we remember this... */

exit:
    db_free(thecmd);
    return status;
}

/* storemac:
 *      Set up a macro buffer and flag to store all
 *      executed command lines there
 *
 * int f;               default flag
 * int n;               macro number to use
 */
#ifdef NUMBERED_MACROS
int storemac(int f, int n) {
    struct buffer *bp;      /* pointer to macro buffer */
    char bufn[10];          /* name of buffer to use */

/* Must have a numeric argument to this function */
    if (f == FALSE) {
        mlwrite_one("No macro specified");
        return FALSE;
    }

/* Range check the macro number */
    if (n < 1 || n > 40) {
        mlwrite_one("Macro number out of range");
        return FALSE;
    }

/* Construct the macro buffer name */
    strcpy(bufn, "/Macro xx");
    bufn[7] = '0' + (n / 10);
    bufn[8] = '0' + (n % 10);

/* Set up the new macro buffer */
    if ((bp = bfind(bufn, TRUE, BFINVS)) == NULL) {
        mlwrite_one("Cannot create macro");
        return FALSE;
    }

/* And make sure it is empty */
    bclear(bp);

/* And set the macro store pointers to it */
    bstore = bp;
    return TRUE;
}
#endif

/* GGR
 * Free up any current ptt_ent allocation
 */
void ptt_free(struct buffer *bp) {

    struct ptt_ent *fwdptr;
    struct ptt_ent *ptr = bp->ptt_headp;
    while(ptr) {
        fwdptr = ptr->nextp;
        Xfree(ptr->from);
        Xfree(ptr->to);
        Xfree(ptr);
        ptr = fwdptr;
    }
    bp->ptt_headp = NULL;
    if (ptt == bp) ptt = NULL;      /* Clear if was in use */
    return;
}

/* GGR
 * Sets the pttable to use if PHON mode is on, and flags to redisplay it.
 * Now just a quick #define.
 */
#define use_pttable(bp) {ptt = (bp); curwp->w_flag |= WFMODE;}

/* Get the 2 grapheme display code for modeline display when this PTT
 * is active.
 */
static char* get_display_code(char *buf) {
    static char ml_display_code[32];

    int mlen = strlen(buf);
    int offs = next_utf8_offset(buf, 0, mlen, 1);
    offs = next_utf8_offset(buf, offs, mlen, 1);
    strncpy(ml_display_code, buf, offs);
    terminate_str(ml_display_code+offs);
    return ml_display_code;
}

/* GGR
 * Compile the contents of a buffer into a ptt_remap structure
 */
static int ptt_compile(struct buffer *bp) {
    char *ml_display_code;

    db_def(lbuf);
    db_def(tok);
    db_def(from_string);

/* Free up any previously-compiled table and get a default display code */

    ptt_free(bp);

    ml_display_code = get_display_code(bp->b_bname+1);  /* Skip '/' */

/* Read through the lines of the buffer */

    int caseset = 1;        /* Default is on */
    struct line *hlp = bp->b_linep;
    struct ptt_ent *lastp = NULL;
    for (struct line *lp = hlp->l_fp; lp != hlp; lp = lp->l_fp) {
/* The rest of the handling expects text, not binary, so no bcopy here */
        db_setn(lbuf, ltext(lp), lused(lp));
/* Provided we don't change lbuf, we can use rp */
        char *rp = db_val(lbuf);
        rp = token(rp, &tok);
/* Ignore any entry with a newline in the from text */
        if (strchr(db_val(tok), '\n')) continue;
        int bow;
        char *from_start;
        if (db_charat(tok, 0) == '^') {
            bow = 1;
            from_start = db_val(tok)+1;
        }
        else {
            bow = 0;
            from_start = db_val(tok);
        }
        db_set(from_string, from_start);
        if (!db_cmpn(from_string, "caseset-", strlen("caseset-"))) {
            char *test_opt = db_val(from_string) + strlen("caseset-");
            if (!strcmp("on", test_opt)) {
                caseset = CASESET_ON;
                continue;
            }
            if (!strcmp("capinit1", test_opt)) {
                caseset = CASESET_CAPI_ONE;
                continue;
            }
            if (!strcmp("capinitall", test_opt)) {
                caseset = CASESET_CAPI_ALL;
                continue;
            }
            if (!strcmp("lowinit1", test_opt)) {
                caseset = CASESET_LOWI_ONE;
                continue;
            }
            if (!strcmp("lowinitall", test_opt)) {
                caseset = CASESET_LOWI_ALL;
                continue;
            }
            if (!strcmp("off", test_opt)) {
                caseset = CASESET_OFF;
                continue;
            }
        }
        if (!db_cmp(from_string, "display-code")) {
            rp = token(rp, &tok);
            if (db_charat(tok, 0) != '\0')
                 ml_display_code = get_display_code(db_val(tok));
            continue;
        }
        db_clear(glb_db);
        while(*rp != '\0') {
            rp = token(rp, &tok);
            if (db_charat(tok, 0) == '\0') break;
            if (!strncmp(db_val(tok), "0x", 2)) {
                long add = strtol(db_val(tok)+2, NULL, 16);
/* This is only for a single byte */
                if ((add < 0) || (add > 0xFF)) {
                    mlwrite("Oxnn syntax is only for a single byte (%s)", db_val(tok));
                    continue;
                }
                db_addch(glb_db, add);
            }
            else if (db_charat(tok, 0) == 'U' &&
                     db_charat(tok, 1) == '+') {
                int val = strtol(db_val(tok)+2, NULL, 16);
                char abuf[8];
                int incr = unicode_to_utf8(val, abuf);
                db_appendn(glb_db, abuf, incr);
            }
            else {
                db_append(glb_db, db_val(tok));
            }
        }
        if (db_len(glb_db) == 0) continue;
        struct ptt_ent *new = Xmalloc(sizeof(struct ptt_ent));
        if (lastp == NULL) {
            bp->ptt_headp = new;
            strcpy(bp->ptt_headp->display_code, "P-");
            strcpy(bp->ptt_headp->display_code+2, ml_display_code);
        }
        else {
            lastp->nextp = new;
        }
        lastp = new;
        new->nextp = NULL;
/* Force to lower case once now...if handling case-matching.
 * Note that upper- and lower-case characters may have different
 * utf8 byte counts!
 */
        new->from_len = db_len(from_string);
        if (caseset != CASESET_OFF) {
            struct mstr ex_mstr;
            utf8_recase(UTF8_LOWER, db_val(from_string), new->from_len,
                 &ex_mstr);
            new->from = ex_mstr.str;        /* malloc()ed, so OK */
            new->from_len = ex_mstr.utf8c;
            new->from_len_uc = ex_mstr.uc;
        }
        else {
            new->from = Xmalloc(new->from_len+1);
            strcpy(new->from, db_val(from_string));
            new->from_len_uc = uclen_utf8(new->from);
        }
/* Input comes in as unicode chars, so we need to save the last one
 * *as unicode*!.
 * We don't save the full grapheme, as you can only type one character
 * at a time.
 */
        int start_at = prev_utf8_offset(new->from, new->from_len, FALSE);
        (void)utf8_to_unicode(new->from, start_at, new->from_len,
              &(new->final_uc));
        new->to = Xmalloc(db_len(glb_db)+1);
        strncpy(new->to, db_val(glb_db), db_len(glb_db));
        terminate_str(new->to + db_len(glb_db));
        new->to_len_uc = uclen_utf8(new->to);
        new->to_len_gph = glyphcount_utf8(new->to);
        new->bow_only = bow;
        new->caseset = caseset;
    }
    db_free(from_string);
    db_free(lbuf);
    db_free(tok);
    if (lastp == NULL) return FALSE;
    use_pttable(bp);
    return TRUE;
}

/* storeproc:
 *  Set up a procedure buffer and flag to store all executed command
 *  lines there.
 *  NOTE that this only sets up a buffer (in bstore) to store the
 *  commands. The reading into that buffer continues to be done by dobuf().
 *
 * int f;               default flag
 * int n;               macro number to use
 */
struct func_opts null_func_opts = { 0, 0, 0, 0, 0, 0 };
int storeproc(int f, int n) {
    struct buffer *bp;      /* pointer to macro buffer */
    int status;             /* return status */
    db_def(bufn);           /* name of buffer to use */
    db_def(pbufn);          /* name of proc buf to use */

#ifdef NUMBERED_MACROS
/* A numeric argument means its a numbered macro */
    if (f == TRUE) return storemac(f, n);
#else
    UNUSED(f); UNUSED(n);
#endif

/* Append the procedure name to the buffer marker tag */
    if ((status =
         mlreply("Procedure name: ", &bufn, CMPLT_BUF)) != TRUE)
        return status;

/* Set up the new macro buffer */

    db_set(pbufn, "/");
    db_appendn(pbufn, db_val(bufn), db_len(bufn));
    if ((bp = bfind(db_val(pbufn), TRUE, BFINVS)) == NULL) {
        mlwrite_one("Cannot create macro");
        status = FALSE;
        goto exit;
    }

/* Add any options */

    bp->btp_opt = null_func_opts;
    db_def(optstr);
    while (1) {
        mlreply("opts: ", &optstr, CMPLT_BUF);
        if (db_charat(optstr, 0) == '\0') break;
        if (!db_cmp(optstr, "skip_in_macro"))   bp->btp_opt.skip_in_macro = 1;
        if (!db_cmp(optstr, "not_mb"))          bp->btp_opt.not_mb = 1;
        if (!db_cmp(optstr, "not_interactive")) bp->btp_opt.not_interactive = 1;
        if (!db_cmp(optstr, "one_pass"))        bp->btp_opt.one_pass = 1;
        if (!db_cmp(optstr, "no_macbug"))       bp->btp_opt.no_macbug = 1;
    }
    db_free(optstr);

/* Individual commands in the procedure will determine the "search_ok"
 * status, so set it to true here.
 */
    bp->btp_opt.search_ok = 1;

/* And make sure it is empty
 * If we are redefining a procedure then we need to remove any variables
 * that the previous version had defined.
 * This is done by bclear().
 */
    bclear(bp);

/* And set the macro store pointers to it */
    bp->b_type = BTPROC;    /* Mark the buffer type */
    bstore = bp;

exit:
    db_free(bufn);
    db_free(pbufn);
    return status;
}

/* GGR
 * Store a phonetic translation table.
 * This starts by storing (starting) a procedure, so we just use that code.
 */
static int ptt_storing = 0;
int storepttable(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status = storeproc(0, 0);
    if (status != TRUE) return status;

/* Mark this as a translation buffer (storeproc will have set BTPROC) */

    bstore->b_type = BTPHON;

/* Mark that we need to compile the buffer when we get to !endm */

    ptt_storing = 1;
    return TRUE;
}

/* GGR
 * Let the user set which translation table to use.
 * Doesn't (yet?) check that it is a translation table.
 */
int set_pttable(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;
    struct buffer *bp;
    db_def(pttbuf);
    db_def(pbufn);

/* As soon as a table is defined ptt gets set, so if it isn't
 * we know that there are no translation tables.
 */

    if (!ptt) {
        mlforce("No phonetic translation tables are yet defined!");
        return FALSE;
    }

    status = mlreply("Translation table to use? ", &pttbuf, CMPLT_PHON);
    if (status != TRUE) return status;

/* Find the ptt buffer */

    db_set(pbufn, "/");
    db_appendn(pbufn, db_val(pttbuf), db_len(pttbuf));
    if ((bp = bfind(db_val(pbufn), FALSE, BFINVS)) == NULL) {
        mlforce("Table %s was not found", db_val(pbufn));
        status = FALSE;
        goto exit;
    }

/* Check that it is a translation buffer */

    if (bp->b_type != BTPHON) {
        mlforce("Buffer %s is not a translation buffer.", pbufn);
        sleep(1);
        status = TRUE;  /* Don't abort start-up file */
        goto exit;
    }
    use_pttable(bp);    /* This does not actually activate it */
exit:
    db_free(pbufn);
    db_free(pttbuf);
    return status;
}

/* GGR
 * Move to the next pttable.
 * A bit like nextbuffer().
 */
int next_pttable(int f, int n) {

    if (f == FALSE) n = 1;
    if (n < 1) return FALSE;
    if (ptt == NULL) return FALSE;

    struct buffer *bp, *tmp_ptt = ptt;
    while (n-- > 0) {
        bp = tmp_ptt;
        bp = bp->b_bufp;                    /* Start at next buffer */
        if (bp == NULL) bp = bheadp;
        while (bp != ptt && (bp->b_type != BTPHON)) {
            bp = bp->b_bufp;                /* Onto next buffer */
            if (bp == NULL) bp = bheadp;    /* Loop around */
        }
        tmp_ptt = bp;
    }
    use_pttable(tmp_ptt);
    return TRUE;
}

/* GGR
 * Toggle the Phonetic Translation table on/off for the current buffer.
 */
int toggle_ptmode(int f, int n) {
    if (!ptt) {
        mlforce("No phonetic translation tables are yet defined!");
        return FALSE;
    }

/* With a -ve arg we force it off, +ve forces it on and 0 toggles it */
    int toggle_off = 0;
    if (f == 0) n = 0;
    if (n == 0) toggle_off = (curbp->b_mode & MDPHON);
    if (toggle_off || (n < 0))
        curbp->b_mode &= ~MDPHON;
    else
        curbp->b_mode |= MDPHON;
    curwp->w_flag |= WFMODE;
    update(TRUE);
    return TRUE;
}

/* GGR
 * Handle a typed-character (unicode) when PHON mode is on
 * Optionally remove the character if no ptt match found
 */
int ptt_handler(int c, int remove_on_no_match) {

    if (!ptt) return FALSE;
    if (!ptt->ptt_headp && !ptt_compile(ptt))
        return FALSE;

/* We are sent unicode characters
 * So insert the unicode character before testing...
 */

    int orig_doto = curwp->w.doto;
    if (linsert_uc(1, c) != TRUE) return FALSE;

    for (struct ptt_ent *ptr = ptt->ptt_headp; ptr; ptr = ptr->nextp) {
        unicode_t wc = c;       /* A working copy */
        if (ptr->caseset != CASESET_OFF) wc = utf8proc_tolower(wc);
        if (ptr->final_uc != wc) continue;

/* We need to know where <n> unicode chars back starts */
        int start_at = unicode_back_utf8(ptr->from_len_uc,
             ltext(curwp->w.dotp), curwp->w.doto);
        if (start_at < 0) continue; /* Insufficient chars */
        if (ptr->caseset != CASESET_OFF) {
/* Need a unicode-case insensitive strncmp!!!
 * Also, since we can't guarantee that a case-changed string will be
 * the same length, we need to step back the right number of unicode
 * chars first.
 */
            if (nocasecmp_utf8(ltext(curwp->w.dotp),
                 start_at, lused(curwp->w.dotp),
                 ptr->from, 0, ptr->from_len)) continue;
        }
        else {
/* We need to check there are sufficient chars to check, then
 * just compare the bytes.
 */
            if (strncmp(ltext(curwp->w.dotp)+start_at,
                 ptr->from, ptr->from_len)) continue;
        }
        if (ptr->bow_only && (curwp->w.doto > ptr->from_len)) { /* Not BOL */
/* Need to step back to the start of the preceding grapheme and get the
 * base Unicode char from there.
 */
            int offs = prev_utf8_offset(ltext(curwp->w.dotp),
                 start_at, TRUE);
            unicode_t prev_uc;
            (void)utf8_to_unicode(ltext(curwp->w.dotp),
                 offs, lused(curwp->w.dotp), &prev_uc);

            const char *uc_class =
                 utf8proc_category_string((utf8proc_int32_t)prev_uc);
            if (uc_class[0] == 'L') continue;
        }

/* We have to replace the string with the translation.
 * If we are doing case-setting on the replacement then we
 * need to know the case of the first character.
 */
        curwp->w.doto = start_at;
        struct line *start_lp = curwp->w.dotp;
        int edit_case = 0;
        int set_case = UTF8_CKEEP;
        if (ptr->caseset != CASESET_OFF) {
            unicode_t fc;
            (void)utf8_to_unicode(ltext(curwp->w.dotp), start_at,
                 lused(curwp->w.dotp), &fc);
            utf8proc_category_t  need_edit_type;
            if (ptr->caseset == CASESET_LOWI_ALL ||
                ptr->caseset == CASESET_LOWI_ONE) {
                need_edit_type = UTF8PROC_CATEGORY_LL;
                set_case = UTF8_LOWER;
            }
            else {
                need_edit_type = UTF8PROC_CATEGORY_LU;
                set_case = UTF8_UPPER;
            }
            if (utf8proc_category(fc) == need_edit_type)
                edit_case = 1;
        }
        ldelete(ptr->from_len, FALSE);
        linstr(ptr->to);

/* ptt_expand() (in eval.c) needs to know this */
        no_newline_in_pttex = (*((ptr->to)+(strlen(ptr->to)-1)) != '\n');

/* We might need to re-case the just-added string.
 * NOTE that we have added everything in the case that the user supplied,
 * and we only change things if the user "from" started with a capital.
 */
        if (edit_case) {
            int count = ptr->to_len_gph;
            curwp->w.doto = start_at;
            curwp->w.dotp = start_lp;
            int was_inword = 0;
            int now_in_word;
            int l_caseset = ptr->caseset;
            while (count--) {
                switch (l_caseset) {
                case CASESET_CAPI_ALL:
                case CASESET_LOWI_ALL:
                    now_in_word = inword(NULL);
                    if (now_in_word && !was_inword) {
                        if (ensure_case(set_case) == UEM_NOCHAR) count = 0;
                    }
                    else {
                        if (forw_grapheme(1) <= 0) count = 0;
                    }
                    was_inword = now_in_word;
                    break;
                case CASESET_CAPI_ONE:
                case CASESET_LOWI_ONE:
                    if (ensure_case(set_case) == UEM_NOCHAR) count = 0;
                    l_caseset = CASESET_OFF;    /* forw_grapheme from now on */
                    break;
                case CASESET_ON:
                    if (ensure_case(set_case) == UEM_NOCHAR) count = 0;
                    break;
                default:
                    if (forw_grapheme(1) <= 0) count = 0;
                    break;
                }
            }
        }
        return TRUE;
    }
/* We have to delete the added character before returning.
 * NOTE!!!  ldelgrapheme() will (clearly) delete a grapheme!
 * But we've just inserted the unichar we are deleting, so it can't
 * consist of multiple unicode chars - so we'll only delete the one.
 */
    if (remove_on_no_match) {
        curwp->w.doto = orig_doto;
        ldelgrapheme(1, FALSE);
    }
    return FALSE;
}

/* free a list of while block pointers
 *
 * struct while_block *wp;              head of structure to free
 */
static void freewhile(struct while_block *wp) {
    struct while_block *fwp = wp;
    while (fwp) {
         struct while_block *next = fwp->w_next;
         Xfree(fwp);
         fwp = next;
    }
}

/* dobuf:
 *      execute the contents of the buffer pointed to
 *      by the passed BP
 *
 *      Directives start with a "!" and are:
 *
 *      !endm           End a macro
 *      !if (cond)      conditional execution
 *      !else
 *      !endif
 *      !return         Return (terminating current macro)
 *      !goto <label>   Jump to a label in the current macro
 *      !force          Force macro to continue...even if command fails
 *      !while (cond)   Execute a loop if the condition is true
 *      !break
 *      !endwhile
 *      !finish [True]  End execution. Return False (unless True given).
 *
 *      Line Labels begin with a "*" as the first nonblank char, like:
 *
 *      *LBL01
 *
 * NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE! NOTE
 * This routine sets the buffer to be read-only while running it,
 * so it is IMPORTANT to ensure that any exit goes via the code
 * to restore the original setting - by "goto single_exit" not "return".
 * This now also resets inreex to its incoming value.
 *
 * struct buffer *bp;           buffer to execute
 */

int dobuf(struct buffer *bp) {
    int status;             /* status return */
    struct line *lp;        /* pointer to line to execute */
    struct line *hlp;       /* pointer to line header */
    struct line *glp;       /* line to goto */
    int dirnum;             /* directive index */
    int linlen;             /* length of line to execute */
    int i;                  /* index */
    int c;                  /* temp character */
    int force;              /* force TRUE result? */
    struct window *wp;              /* ptr to windows to scan */
    struct while_block *whlist;     /* ptr to !WHILE list */
    struct while_block *scanner;    /* ptr during scan */
    struct while_block *whtemp;     /* temporary ptr to a struct while_block */
    char *einit = NULL; /* Initial val of eline - allocate on first call */
    char *eline;        /* text of line to execute */
    int return_stat = TRUE; /* What we expect to do */
    int orig_pause_key_index_update;    /* State on entry - to be restored */

    db_def(tkn);            /* buffer to evaluate an expresion in */
    db_def(golabel);

/* GGR - Only allow recursion up to a certain level... */

#define MAX_RECURSE 10

    if (bp->b_exec_level > MAX_RECURSE) {
        mlwrite("Maximum recursion level, %d, exceeded!", MAX_RECURSE);
        return FALSE;
    }

    macro_level++;

/* We may be reexecing the buffer, but we aren't reexecing individual
 * commands within it!
 * Remember its entry value (for restoring on exit) and unset it now.
 */
    int init_inreex = inreex;
    inreex = FALSE;
    struct buffer *init_execbp = execbp;
    execbp = bp;

/* Mark an executing buffer as read-only while it is being executed */
    int orig_view_bit = bp->b_mode & MDVIEW;
    bp->b_mode |= MDVIEW;
    bp->b_exec_level++;

    orig_pause_key_index_update = pause_key_index_update;
    pause_key_index_update = 1;

/* Clear IF level flags/while ptr */
    execlevel = 0;
    whlist = NULL;
    scanner = NULL;

/* First scan of the buffer to execute, building WHILE header blocks.
 * But DON'T add these whilst we are scanning a store-procedure etc.
 * definition as they are nothing to do with us at the moment.
 */
    hlp = bp->b_linep;
    lp = hlp->l_fp;
    int in_store_mode = FALSE;
    while (lp != hlp) {
/* Scan the current line */
        eline = ltext(lp);
        i = lused(lp);

/* Trim leading whitespace */
        while (i-- > 0 && (*eline == ' ' || *eline == '\t')) ++eline;

/* If there's nothing here - don't bother */
        if (i <= 0) goto nxtscan;

/* Is it store-procedure/pttable/macro? if so, ignore lines
 * until the matching !endm
 * This means we don't store the info for lines we won't actually
 * execute on this call.
 * We only have one bstore variable, so can't recurse.
 */
        if (!strncmp(eline, "store-", 6)) {
            if (in_store_mode) {
                mlforce("Nested store-* commands are not supported");
                goto failexit2;
            }
            in_store_mode = TRUE;
        }

/* If there's no directive, don't bother */
        if (eline[0] != '!') goto nxtscan;

/* Which directive is it? */
        for (dirnum = 0; dirnum < NUMDIRS; dirnum++) {
            if (strncmp(eline+1, dname[dirnum],
                 strlen(dname[dirnum])) == 0)   break;
        }
        if (dirnum == NUMDIRS) {    /* bitch if it's illegal */
            mlwrite_one("Unknown Directive");
            goto failexit2;
        }
        if (dirnum == DENDM) in_store_mode = FALSE; /* Now left the macro */
        if (in_store_mode) goto nxtscan;            /* Still in a macro */
        switch(dirnum) {    /* Only interested in a subset at the moment */
        case DWHILE:        /* Make a block... */
            whtemp = Xmalloc(sizeof(struct while_block));
            whtemp->w_begin = lp;
            whtemp->w_type = BTWHILE;
            whtemp->w_next = scanner;
            scanner = whtemp;
            break;

        case DBREAK:        /* Make a block... */
            if (scanner == NULL) {
                mlwrite_one("!BREAK outside of any !WHILE loop");
                goto failexit2;
            }
            whtemp = Xmalloc(sizeof(struct while_block));
            whtemp->w_begin = lp;
            whtemp->w_type = BTBREAK;
            whtemp->w_next = scanner;
            scanner = whtemp;
            break;

        case DENDWHILE:     /* Record the spot... */
            if (scanner == NULL) {
                mlwrite("!ENDWHILE with no preceding !WHILE in '%s'",
                     bp->b_bname);
                goto failexit2;
            }
/* Move top records from the scanner list to the whlist until we have
 * moved all BREAK records and one WHILE record.
 */
            do {
                scanner->w_end = lp;
                whtemp = whlist;
                whlist = scanner;
                scanner = scanner->w_next;
                whlist->w_next = whtemp;
            } while (whlist->w_type == BTBREAK);
            break;
        default:        /* Nothing yet for the rest...*/
            ;
        }
nxtscan:          /* on to the next line */
        lp = lp->l_fp;
    }

/* While and endwhile should match! */
    if (scanner != NULL) {
        mlwrite("!WHILE with no matching !ENDWHILE in '%s'", bp->b_bname);
        goto failexit2;
    }

/* Starting at the beginning of the buffer again.
 * This time we'll actually be doing things...
 */
    hlp = bp->b_linep;
    lp = hlp->l_fp;
    int eilen = 0;
    while (lp != hlp) {
/* Allocate eline and copy macro line to it */
        linlen = lused(lp);
        if (linlen == 0) goto onward;
        if (linlen > eilen) {
            einit = Xrealloc(einit, linlen + 1);
            eline = einit;
            eilen = linlen;
        }
        eline = einit;
        memcpy(eline, ltext(lp), linlen);
        terminate_str(eline+linlen);    /* Make sure it ends */

/* Trim leading whitespace */
        while (*eline == ' ' || *eline == '\t') ++eline;

/* Dump comments and blank lines */
        if (*eline == ';' || *eline == '\0') goto onward;

/* If $debug & 0x01, every assignment will be reported in the minibuffer.
 *      The user then needs to press a key to continue.
 *      If that key is abortc (ctl-G) the macro is aborted
 *      If that key is metac (Esc) then ALL debug is turned off.
 * If $debug & 0x02, every assignment will be reported in //Debug buffer
 *
 * NOTE that a user-macro can turn this off while running.
 *      This is used by the ones which set macbug and clear //Debug.
 */
        if (macbug && !macbug_off) {    /* More likely failure first */
            db_def(outline);
            db_sprintf(outline, "<%s:%s:%s>", bp->b_bname,
                ue_itoa(execlevel), eline);

/* Write out the debug line to //Debug? */
            if (macbug & 0x2) {
                addline_to_anyb(db_val(outline), bdbgp);
            }
/* Write out the debug line to the message line? */
            if (macbug & 0x1) {
                mlforce_one(db_val(outline));
                update(TRUE);

/* And get the keystroke */
                if ((c = get1key()) == abortc) {
                    mlforce_one(MLbkt("Macro aborted"));
                    goto failexit3;
                }
                if (c == metac) macbug = 0;
            }
            db_free(outline);
        }

/* Parse directives here.... */
        dirnum = -1;
        if (*eline == '!') {
/* Find out which directive this is...
 * We've already checked in the first pass for legal directives, so this
 * can't fail now.
 */
            for (dirnum = 0; dirnum < NUMDIRS; dirnum++) {
                if (strncmp(eline+1, dname[dirnum],
                     strlen(dname[dirnum])) == 0)   break;
            }

/* Service only the !ENDM macro here */
            if (dirnum == DENDM) {
                if (ptt_storing) {
                    ptt_compile(bstore);
                    ptt_storing = 0;
                }
                bstore = NULL;
                goto onward;
            }
        }

/* If macro store is on, just salt this away */
        if (bstore) {
            addline_to_anyb(eline, bstore);
            goto onward;
        }

/* Dump comments
 * Although these are actually targets for gotos!!
 * But goto just searches for the label from the start of the file each time.
 */
        if (*eline == '*') goto onward;
        force = FALSE;

/* If we process exit-emacs, we won't get to free any used tkn, so
 * mark this for handling by free_exec.
 * We have to save a pointer to the structure, as the actual text val
 * can be altered by a realloc.
 */
#ifdef DO_FREE
    pending_dobuf_tknp = &tkn;
    pending_golabel = &golabel;
#endif

/* Now, execute directives */
        if (dirnum != -1) {
/* Skip past the directive */
            while (*eline && *eline != ' ' && *eline != '\t') ++eline;
            execstr = eline;

            switch (dirnum) {
            case DIF:       /* IF directive */
/* Grab the value of the logical exp */
                if (execlevel == 0) {
                    if (macarg(&tkn) != TRUE) goto eexec;
                    if (stol(db_val(tkn)) == FALSE) ++execlevel;
                }
                else ++execlevel;
                goto onward;

            case DWHILE:    /* WHILE directive */
/* Grab the value of the logical exp */
                if (execlevel == 0) {
                    if (macarg(&tkn) != TRUE) goto eexec;
                    if (stol(db_val(tkn)) == TRUE) goto onward;
                }
/* Drop down and act just like !BREAK */
                /* Falls through */
            case DBREAK:    /* BREAK directive */
                if (dirnum == DBREAK && execlevel) goto onward;

/* Jump down to the endwhile
 * Find the right while loop
 */
                for (whtemp = whlist; whtemp; whtemp = whtemp->w_next) {
                    if (whtemp->w_begin == lp) break;
                }
                if (whtemp == NULL) {
                    mlwrite_one("Internal While loop error");
                    goto failexit3;
                }

/* Reset the line pointer back.. */
                lp = whtemp->w_end;
                goto onward;

            case DELSE:     /* ELSE directive */
                if (execlevel == 1) --execlevel;
                else if (execlevel == 0) ++execlevel;
                goto onward;

            case DENDIF:    /* ENDIF directive */
                if (execlevel) --execlevel;
                goto onward;

            case DGOTO:     /* GOTO directive */
/* .....only if we are currently executing */
                if (execlevel == 0) {
/* Grab label to jump to.  Allow it to be evaulated. */
                    eline = token(eline, &golabel);
/* Via temp copy, to avoid overwrite of own value */
                    db_set(golabel, strdupa(getval(db_val(golabel))));
                    linlen = db_len(golabel);
                    for (glp = hlp->l_fp; glp != hlp; glp = glp->l_fp) {
/* We need at least 2 chars on the line for a label... */
                        if (lused(glp) < 2) continue;
                        if (*ltext(glp) == '*' &&
                            (db_cmpn(golabel, ltext(glp)+1, linlen) == 0)) {
                            lp = glp;
                            goto onward;
                        }
                    }
                    mlwrite("No such label: %s", db_val(golabel));
                    goto failexit3;
                }
                goto onward;

            case DRETURN:   /* RETURN directive */
                if (execlevel == 0) goto eexec;
                goto onward;

            case DENDWHILE: /* ENDWHILE directive */
                if (execlevel) {
                    --execlevel;
                    goto onward;
                }
                else {
/* Find the right while loop */
                    for (whtemp = whlist; whtemp; whtemp = whtemp->w_next) {
                        if (whtemp->w_type == BTWHILE &&
                            whtemp->w_end == lp) break;
                    }
                    if (whtemp == NULL) {
                        mlwrite_one("Internal While loop error");
                        goto failexit3;
                    }

/* reset the line pointer back.. */
                    lp = whtemp->w_begin->l_bp;
                    goto onward;
                }

            case DFORCE:    /* FORCE directive */
                force = TRUE;
                break;      /* GGR: Must not drop down!! */

            case DFINISH:   /* FINISH directive. Abort with exit FALSE. */
                if (execlevel == 0) {
                    return_stat = FALSE;
                    if (macarg(&tkn))
                        return_stat = (db_casecmp(tkn, "True") == 0);
                    goto eexec;
                }
                goto onward;
            }
        }

#if DO_FREE
/* Push allocated entries for valgrind cleanup.
 * In case we do not return from the docmd() call.
 */
        add_to_head(&pending_einit_headp, einit);
        add_to_head(&pending_whlist_headp, whlist);
#endif

/* Execute the statement. */
        status = docmd(eline);

#if DO_FREE
/* Now pop any saved allocates.
 * We should be freeing the originals ourself from here.
 */
        pop_head(&pending_einit_headp);
        pop_head(&pending_whlist_headp);
#endif
        if (force) {                /* Set force_status, so we can check */
            if (status == TRUE) {
                force_status = "PASSED";
            }
            else {
                force_status = "FAILED";
                status = TRUE;      /* force the status */
            }
        }

/* Check for a command error */
        if (status != TRUE) {
/* Look if buffer is showing */
            for (wp = wheadp; wp; wp = wp->w_wndp) {
                if (wp->w_bufp == bp) { /* And point it */
                    wp->w.dotp = lp;
                    wp->w.doto = 0;
                    wp->w_flag |= WFHARD;
                }
            }
/* In any case set the buffer . */
            bp->b.dotp = lp;
            bp->b.doto = 0;
            execlevel = 0;
            pause_key_index_update = orig_pause_key_index_update;
            Xfree(einit);
            goto single_exit;
        }

onward:                 /* On to the next line */
        lp = lp->l_fp;
    }

eexec:                  /* Exit the current function */
    Xfree(einit);
    execlevel = 0;

    status = return_stat;
    goto failexit;

/* This sequence is used for several exits, so only write it once.
 * And have one which frees einit as well, for loop exits.
 */
failexit3:
    Xfree(einit);

failexit2:
    status = FALSE;

failexit:
    freewhile(whlist);      /* Run this here for all exits */
    pause_key_index_update = orig_pause_key_index_update;

/* If this was (meant to be) a procedure buffer, we must switch that off
 * so that we don't try to run it...
 * Just set the bit off for all errors...
 */
    if (bstore) bstore->b_type |= ~BTPROC;  /* Forcibly unmark macro type */

single_exit:

/* Restore the original inreex value before leaving */
    inreex = init_inreex;
    execbp = init_execbp;

/* Decrement the macro level counter but first, if we allocated a
 * per-macro-level pin at this level, remove it.
 */
    if (macro_pin_headp && (mmi(macro_pin_headp, mac_level) == macro_level)) {
        Xfree(macro_pin_headp->item);
        pop_head(&macro_pin_headp);
    }
    macro_level--;
    bp->b_exec_level--;

/* Revert to the original read-only status if it wasn't originally set
 * i.e. restore any writeability!
 */
    if (!orig_view_bit) bp->b_mode &= ~MDVIEW;
    db_free(golabel);
    db_free(tkn);
#ifdef DO_FREE
    pending_dobuf_tknp = NULL;
    pending_golabel = NULL;
#endif
    return status;
}

/* Run (execute) a user-procedure stored in a buffer */

int run_user_proc(char *procname, int forced, int rpts) {
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    db_def(bufn);

/* Construct the buffer name */
    db_set(bufn, "/");
    db_append(bufn, procname);

/* Find the pointer to that buffer */
    if ((bp = bfind(db_val(bufn), FALSE, 0)) == NULL) {
        mlwrite("No such procedure: %s", db_val(bufn));
        status = FALSE;
        goto exit;
    }

/* Check that it is a procedure buffer */

    if (bp->b_type != BTPROC) {
        mlforce("Buffer %s is not a procedure buffer.", db_val(bufn));
        sleep(1);
        status = TRUE;  /* Don't abort start-up file */
        goto exit;
    }

/* and now execute it as asked.
 * Let the user-proc know which pass it is on (starting at one)
 * out of the total expected.
 * Pass on the requested repeats (in uproc_lptotal) even for a one_pass
 * function, in case it wishes to handle it during its one_pass.
 */
    int this_total = rpts;                  /* The real value */
    if (rpts <= 0) rpts = 1;                /* Have to loop at least once */
    if (bp->btp_opt.one_pass) rpts = 1;     /* and possibly only once */
    int this_count = 0;
    status = TRUE;
    int orig_macbug_off = macbug_off;
    if (bp->btp_opt.no_macbug) macbug_off = 1;
    while (rpts-- > 0) {
/* Since one user-proc can call another we have to remember the current
 * setting, install our current ones, then restore the originals
 * after we're done.
 * Same for which buffer is being used....
 */
        int saved_count = uproc_lpcount;
        int saved_total = uproc_lptotal;
        int saved_forced = uproc_lpforced;
        uproc_lpcount = ++this_count;
        uproc_lptotal = this_total;
        uproc_lpforced = forced;
        status = dobuf(bp);
        uproc_lpcount = saved_count;
        uproc_lptotal = saved_total;
        uproc_lpforced = saved_forced;
        if (!status) break;
    }
    macbug_off = orig_macbug_off;
exit:
    db_free(bufn);
    return status;
}

/* User-accessible functions to save/restore/maintain the pins in a macro */

int drop_pin(int f, int n) {
    UNUSED(f); UNUSED(n);

/* Are we running buffer code? */
    if (!macro_level) return FALSE;

    struct mac_pin *sp = Xmalloc(sizeof(struct mac_pin));
    sp->bp = curbp;
    sp->lp = curwp->w.dotp;
    sp->offset = curwp->w.doto;
    sp->mac_level = macro_level;
    add_to_head(&macro_pin_headp, sp);

    return TRUE;
}

static int goto_pin(int do_switch) {

/* Are we running buffer code? */
    if (!macro_level) return FALSE;

/* Was a pin saved at this level */
    if (!macro_pin_headp || (mmi(macro_pin_headp, mac_level) != macro_level))
         return FALSE;

/* We might be outside the visible range of a narrowed buffer (which
 * can't happen to a mark, as that is used to define the narrowed region).
 * Need to make this check before possibly switching buffer.
 */
    if (mmi(macro_pin_headp, bp)->b_flag & BFNAROW) {
        int ok = 0;
        struct line *tlp = mmi(macro_pin_headp, bp)->b_linep;
        for (struct line *lp = lforw(tlp); lp != tlp; lp = lforw(lp)) {
            if (lp == mmi(macro_pin_headp, lp)) {
                ok = 1;
                break;
            }
        }
        if (!ok) return FALSE;
    }

/* If we've been asked to switch position, remember where we are
 * so we can update the pin after the move.
 */
    struct mac_pin old_pos;
    if (do_switch) {
        old_pos.bp = curbp;
        old_pos.lp = curwp->w.dotp;
        old_pos.offset = curwp->w.doto;
        old_pos.mac_level = mmi(macro_pin_headp, mac_level);
        old_pos.col_offset = 0;     /* Not used for pins */
    }

/* The general dot/mark code ensures the values we have are still valid.
 * The pin is removed from the list if the buffer is deleted.
 */
    if (curbp != mmi(macro_pin_headp, bp))
         swbuffer(mmi(macro_pin_headp, bp), TRUE);
    curwp->w.dotp = mmi(macro_pin_headp, lp);
    curwp->w.doto = mmi(macro_pin_headp, offset);
    if (do_switch) *((struct mac_pin *)(macro_pin_headp->item)) = old_pos;
    return TRUE;
}

int back_to_pin(int f, int n) {
    UNUSED(f); UNUSED(n);
    return goto_pin(FALSE);
}
int switch_with_pin(int f, int n) {
    UNUSED(f); UNUSED(n);
    return goto_pin(TRUE);
}

/* Buffer name for reexecute - shared by all buffer-callers */

static db_def(prev_bufn);

/* execproc:
 *      Execute a procedure
 *
 * int f, n;            default flag and numeric arg
 */
int execproc(int f, int n) {
    UNUSED(f);
    db_def(bufn);           /* name of buffer to execute */
    int status;             /* status return */

/* Handle a reexecute */

/* Re-use last obtained buffer? */
    if (inreex && (db_len(prev_bufn) > 0) && RXARG(execproc))
        db_set(bufn, db_val(prev_bufn));
    else {
        if (input_waiting != NULL) {
            db_set(bufn, input_waiting);
            input_waiting = NULL;   /* We've used it */
        }
        else {
            if ((status = mlreply("Execute procedure: ", &bufn,
                 CMPLT_PROC)) != TRUE)
                goto exit;
        }
    }

    status = run_user_proc(db_val(bufn), f, n);

/* dobuf() could contain commands that change prev_bufn, so reinstate
 * it here to allow for recursion.
 */
    if (status == TRUE) db_set(prev_bufn, db_val(bufn));

exit:
    db_free(bufn);
    return status;
}

/* execbuf:
 *      Execute the contents of a buffer of commands
 *
 * int f, n;            default flag and numeric arg
 */
int execbuf(int f, int n) {
    UNUSED(f);
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    db_def(bufn);           /* name of buffer to execute */

/* Handle a reexecute */

/* Re-use last obtained buffer? */
    if (inreex && (db_len(prev_bufn) > 0) && RXARG(execbuf))
        db_set(bufn, db_val(prev_bufn));
    else {
/* Find out what buffer the user wants to execute */
        if ((status = mlreply("Execute buffer: ", &bufn,
             CMPLT_BUF)) != TRUE)
            goto exit;

        if (kbdmode != STOP && (db_cmp(bufn, kbdmacro_buffer) == 0)) {
            mlwrite_one("Cannot run keyboard macro whilst collecting it");
            status = FALSE;
            goto exit;
        }
    }

/* Find the pointer to that buffer */
    if ((bp = bfind(db_val(bufn), FALSE, 0)) == NULL) {
        mlwrite("No such buffer: %s", db_val(bufn));
        status = FALSE;
        goto exit;
    }

/* And now execute it as asked */

    status = TRUE;
    while (n-- > 0) if ((status = dobuf(bp)) != TRUE) break;

/* dobuf() could contain commands that change prev_bufn, so reinstate
 * it here to allow for recursion.
 */
    db_set(prev_bufn, db_val(bufn));

exit:
    db_free(bufn);
    return status;
}

/* dofile:
 *      yank a file into a buffer and execute it
 *      if there are no errors, delete the buffer on exit
 *
 * char *fname;         file name to execute
 */
int dofile(char *fname) {
    struct buffer *bp;      /* buffer to place file to exeute */
    struct buffer *cb;      /* temp to hold current buf while we read */
    int status;             /* results of various calls */

    db_def(bufn);           /* name of buffer */

    makename(&bufn, fname, TRUE);       /* derive unique name for buffer */
    bp = bfind(db_val(bufn), TRUE, 0);  /* get the needed buffer */
    db_free(bufn);
    if (!bp) return FALSE;
    cb = curbp;             /* save the old buffer */
    curbp = bp;             /* make this one current */
    if (silent)             /* GGR */
        pathexpand = FALSE;

/* And try to read in the file to execute
 * readin() takes copies of the fname, as required, so we can just send
 * the fixup_fname() output (its internal buffer) directly.
 */
    if ((status = readin(fixup_fname(fname), FALSE)) != TRUE) {
        curbp = cb;     /* restore the current buffer */
        pathexpand = TRUE;  /* GGR */
        return status;
    }
    pathexpand = TRUE;          /* GGR */

/* Go execute it! */
    curbp = cb;             /* restore the current buffer */
    if ((status = dobuf(bp)) != TRUE) return status;

/* If not displayed, remove the now unneeded buffer and exit */
    if (bp->b_nwnd == 0) zotbuf(bp);
    return TRUE;
}

/* Filename for reexecute - shared by all file-callers */

static db_def(prev_fname);

/* execute a series of commands in a file
 * If given fname starts with "^", remove that character and don't
 * return any error status.
 * int f, n;            default flag and numeric arg to pass on to file
 */
static int include_level = 0;
#define MAX_INCLUDE_LEVEL 8
int execfile(int f, int n) {
    UNUSED(f);
    int status;             /* return status of name query */
    char *fspec;            /* full file spec */
    int fail_ok = 0;
    int fns = 0;

    db_def(fname);          /* name of file to execute */

/* Re-use last obtained filename? */
    if (inreex && (db_len(prev_fname) > 0) && RXARG(execfile))
        db_setn(fname, db_val(prev_fname), db_len(prev_fname));
    else {
        if ((status =
          mlreply("File to execute: ", &fname, CMPLT_FILE)) != TRUE)
            goto exit;

        if (include_level >= MAX_INCLUDE_LEVEL) {
            mlwrite("Include depth too great (%d)!",  include_level+1);
            status = FALSE;
            goto exit;
        }
    }

    if (db_charat(fname, 0) == '^') {
        fail_ok = 1;
        fns = 1;
    }
/* Don't look in HOME!
 * This allows you to have uemacs.rc in HOME that includes a system one
 * by using "execute-file uemacs.rc"
 */
    fspec = flook(db_val(fname)+fns, FALSE, INTABLE);

/* If it isn't around */
    if (fspec == NULL) {
        mlwrite("Include file %s not found!", db_val(fname)+fns);
        status = fail_ok? TRUE: FALSE;
        goto exit;
    }

/* Otherwise, execute it */
    while (n-- > 0) {
        include_level++;
        status = dofile(fspec);
        include_level--;
        if (status != TRUE) {
            if (fail_ok) status = TRUE;
            goto exit;
        }
    }

/* dofile() could contain commands that change prev_fname, so reinstate
 * it here to allow for recursion.
 */
    db_set(prev_fname, db_val(fname));

exit:
    db_free(fname);
    return status;
}

/* We no longer need these, now yuo can use storeproc to
 * give a name to a procedure, and bind it with buffer-to-key
 */
#ifdef NUMBERED_MACROS
/* cbuf:
 *      Execute the contents of a numbered buffer
 *
 * int f, n;            default flag and numeric arg
 * int bufnum;          number of buffer to execute
 */
static int cbuf(int f, int n, int bufnum) {
    UNUSED(f);
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    static char bufname[] = "/Macro xx";

/* Make the buffer name */
    bufname[7] = '0' + (bufnum / 10);
    bufname[8] = '0' + (bufnum % 10);

/* Find the pointer to that buffer */
    if ((bp = bfind(bufname, FALSE, 0)) == NULL) {
        mlwrite_one("Macro not defined");
        return FALSE;
    }

/* and now execute it as asked */
    while (n-- > 0)
        if ((status = dobuf(bp)) != TRUE)
            return status;
    return TRUE;
}

/* Declare the historic 40 numbered macro buffers */
#define NMAC(nmac) \
   int cbuf ## nmac(int f, int n) { return cbuf(f, n, nmac); }

NMAC(1)     NMAC(2)     NMAC(3)     NMAC(4)     NMAC(5)
NMAC(6)     NMAC(7)     NMAC(8)     NMAC(9)     NMAC(10)
NMAC(11)    NMAC(12)    NMAC(13)    NMAC(14)    NMAC(15)
NMAC(16)    NMAC(17)    NMAC(18)    NMAC(19)    NMAC(20)
NMAC(21)    NMAC(22)    NMAC(23)    NMAC(24)    NMAC(25)
NMAC(26)    NMAC(27)    NMAC(28)    NMAC(29)    NMAC(30)
NMAC(31)    NMAC(32)    NMAC(33)    NMAC(34)    NMAC(35)
NMAC(36)    NMAC(37)    NMAC(38)    NMAC(39)    NMAC(40)
#endif

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 * Freeing NULL is a no-op.
 */
void free_exec(void) {
    Xfree(prev_line_seen);
    Xfree(pending_line_seen);
    linked_items *lp;
    while ((lp = pending_einit_headp)) {
        Xfree(lp->item);
        pop_head(&pending_einit_headp);
    }
    while ((lp = pending_whlist_headp)) {
        freewhile(lp->item);
        pop_head(&pending_whlist_headp);
    }
    while ((lp = macro_pin_headp)) {
        Xfree(lp->item);
        pop_head(&macro_pin_headp);
    }

    db_free(prev_cmd);
    db_free(prev_fname);
    db_free(prev_bufn);
    if (pending_docmd_tknp) dbp_free(pending_docmd_tknp);
    if (pending_dobuf_tknp) dbp_free(pending_dobuf_tknp);
    if (pending_golabel) dbp_free(pending_golabel);
    return;
}
#endif
