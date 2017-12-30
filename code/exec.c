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

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

#include "utf8proc.h"

/*
 * Execute a named command even if it is not bound.
 */
int namedcmd(int f, int n)
{
        fn_t kfunc;     /* ptr to the requexted function to bind to */

        /* prompt the user to type a named command */
        mlwrite(": ");

        /* and now get the function name to execute */
        kfunc = getname();
        if (kfunc == NULL) {
                mlwrite(MLpre "No such function" MLpost);
                return FALSE;
        }

/* Check whether the given command is allowed in the minibuffer, if that
 * is where we are
 */
        if (inmb) {
            struct name_bind *fi = func_info(kfunc);
            if (fi && fi->opt.not_mb) {
                not_in_mb.funcname = fi->n_name;
                not_in_mb.keystroke = -1;   /* No keystroke... */
                kfunc = not_in_mb_error;    /* Change what we call... */
            }
        }

        /* and then execute the command */
        return kfunc(f, n);
}

/*
 * execcmd:
 *      Execute a command line command to be typed in
 *      by the user
 *
 * int f, n;            default Flag and Numeric argument
 */
int execcmd(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int status;             /* status return */
        char cmdstr[NSTRING];   /* string holding command to execute */

        /* get the line wanted */
        if ((status = mlreply(": ", cmdstr, NSTRING)) != TRUE)
                return status;

        execlevel = 0;
        return docmd(cmdstr);
}

/*
 * docmd:
 *      take a passed string as a command line and translate
 *      it to be executed as a command. This function will be
 *      used by execute-command-line and by all source and
 *      startup files. Lastflag/thisflag is also updated.
 *
 *      format of the command line is:
 *
 *              {# arg} <command-name> {<argument string(s)>}
 *
 * char *cline;         command line to execute
 */
int docmd(char *cline)
{
        int f;                  /* default argument flag */
        int n;                  /* numeric repeat value */
        int status;             /* return status of function */
        int oldcle;             /* old contents of clexec flag */
        char *oldestr;          /* original exec string */
        char tkn[NSTRING];      /* next token off of command line */
        char tbuf[NSTRING];     /* string buffer for some workings */
        /* if we are scanning and not executing..go back here */
        if (execlevel)
                return TRUE;

        oldestr = execstr;      /* save last ptr to string to execute */
        execstr = cline;        /* and set this one as current */

        /* first set up the default command values */
        f = FALSE;
        n = 1;
        lastflag = thisflag;
        thisflag = 0;

        if ((status = macarg(tkn)) != TRUE) {   /* and grab the first token */
                execstr = oldestr;
                return status;
        }

        /* process leadin argument */
        if (gettyp(tkn) != TKCMD) {
                f = TRUE;
/* GGR - There is the possibility of an illegal overlap of args here.
 *       So it must be done via a temporary buffer.
 *              strcpy(tkn, getval(tkn));
 */
                strcpy(tbuf, getval(tkn));
                strcpy(tkn, tbuf);
                n = atoi(tkn);

                /* and now get the command to execute */
                if ((status = macarg(tkn)) != TRUE) {
                        execstr = oldestr;
                        return status;
                }
        }

        /* and match the token to see if it exists */
        struct name_bind *nbp = name_info(tkn);
        if (nbp == NULL) {
                mlwrite(MLpre "No such Function" MLpost);
                execstr = oldestr;
                return FALSE;
        }

        /* save the arguments and go execute the command */
        oldcle = clexec;        /* save old clexec flag */
        clexec = TRUE;          /* in cline execution */
        status = (nbp->n_func)(f, n); /* call the function */
        cmdstatus = status;     /* save the status */
        clexec = oldcle;        /* restore clexec flag */
        execstr = oldestr;
        return status;
}

/*
 * token:
 *      chop a token off a string
 *      return a pointer past the token
 *
 * char *src, *tok;     source string, destination token string
 * int size;            maximum size of token
 */
char *token(char *src, char *tok, int size)
{
        int quotef;     /* is the current string quoted? */
        char c; /* temporary character */

        /* first scan past any whitespace in the source string */
        while (*src == ' ' || *src == '\t')
                ++src;

        /* scan through the source string */
        quotef = FALSE;
        while (*src) {
                /* process special characters */
                if (*src == '~') {
                        ++src;
                        if (*src == 0)
                                break;
                        switch (*src++) {
                        case 'r':
                                c = 13;
                                break;
                        case 'n':
                                c = 10;
                                break;
                        case 't':
                                c = 9;
                                break;
                        case 'b':
                                c = 8;
                                break;
                        case 'f':
                                c = 12;
                                break;
                        default:
                                c = *(src - 1);
                        }
                        if (--size > 0) {
                                *tok++ = c;
                        }
                } else {
                        /* check for the end of the token */
                        if (quotef) {
                                if (*src == '"')
                                        break;
                        } else {
                                if (*src == ' ' || *src == '\t')
                                        break;
                        }

                        /* set quote mode if quote found */
                        if (*src == '"') {
                                quotef = TRUE;
                                src++;
                                continue;   /* Don't record it... */
                        }

                        /* record the character */
                        c = *src++;
                        if (--size > 0)
                                *tok++ = c;
                }
        }

        /* terminate the token and exit */
        if (*src)
                ++src;
        *tok = 0;
        return src;
}

/*
 * get a macro line argument
 *
 * char *tok;           buffer to place argument
 */
int macarg(char *tok)
{
        int savcle;             /* buffer to store original clexec */
        int status;

        savcle = clexec;        /* save execution mode */
        clexec = TRUE;          /* get the argument */
        status = nextarg("", tok, NSTRING, ctoec('\n'));
        clexec = savcle;        /* restore execution mode */
        return status;
}

/*
 * nextarg:
 *      get the next argument
 *
 * char *prompt;                prompt to use if we must be interactive
 * char *buffer;                buffer to put token into
 * int size;                    size of the buffer
 * int terminator;              terminating char to be used on interactive fetch
 */
int nextarg(char *prompt, char *buffer, int size, int terminator)
{
        char tbuf[NSTRING];     /* string buffer for some workings */
        /* if we are interactive, go get it! */
        if (clexec == FALSE)
                return getstring(prompt, buffer, size, terminator);
        /* grab token and advance past */
        execstr = token(execstr, buffer, size);

        /* evaluate it */
/* GGR - There is the possibility of an illegal overlap of args here.
 *       So it must be done via a temporary buffer.
 *      strcpy(buffer, getval(buffer));
 */
        strcpy(tbuf, getval(buffer));
        strcpy(buffer, tbuf);
        return TRUE;
}

/*
 * storemac:
 *      Set up a macro buffer and flag to store all
 *      executed command lines there
 *
 * int f;               default flag
 * int n;               macro number to use
 */
int storemac(int f, int n)
{
        struct buffer *bp;      /* pointer to macro buffer */
        char bufn[NBUFN];       /* name of buffer to use */

        /* must have a numeric argument to this function */
        if (f == FALSE) {
                mlwrite("No macro specified");
                return FALSE;
        }

        /* range check the macro number */
        if (n < 1 || n > 40) {
                mlwrite("Macro number out of range");
                return FALSE;
        }

        /* construct the macro buffer name */
        strcpy(bufn, "/Macro xx");
        bufn[7] = '0' + (n / 10);
        bufn[8] = '0' + (n % 10);

        /* set up the new macro buffer */
        if ((bp = bfind(bufn, TRUE, BFINVS)) == NULL) {
                mlwrite("Can not create macro");
                return FALSE;
        }

        /* and make sure it is empty */
        bclear(bp);

        /* and set the macro store pointers to it */
        mstore = TRUE;
        bstore = bp;
        return TRUE;
}

#if     PROC

/* GGR
 * Free up any current ptt_ent allocation
 */
void ptt_free(struct buffer *bp) {

    struct ptt_ent *fwdptr;
    struct ptt_ent *ptr = bp->ptt_headp;
    while(ptr) {
        fwdptr = ptr->nextp;
        free(ptr->from);
        free(ptr->to);
        free(ptr);
        ptr = fwdptr;
    }
    bp->ptt_headp = NULL;
    return;
}

/* GGR
 * Sets the pttable to use if PHON mode is on and sets the modeline
 * text to display when it is.
 */
static void use_pttable(struct buffer *bp) {
    ptt = bp;
    mode2name[2] = ptt->ptt_headp->display_code;    /* Text for modeline */
    curwp->w_flag |= WFMODE;
}


/* GGR
 * Compile the contents of a buffer into a ptt_remap structure
 */
static int ptt_compile(struct buffer *bp) {
    char ml_display_code[32];

/* Free up any previously-compiled table and get a default display code */

    ptt_free(bp);

    unicode_t uc1, uc2;
    int mlen = strlen(bp->b_bname);
    int offs = 1;                   /* Skip the leading / */
    offs += utf8_to_unicode(bp->b_bname, 1, mlen, &uc1);
    (void)utf8_to_unicode(bp->b_bname, offs, mlen, &uc2);
    offs = unicode_to_utf8(uc1, ml_display_code);
    offs += unicode_to_utf8(uc2, ml_display_code+offs);
    ml_display_code[offs] = '\0';

/* Read through the lines of the buffer */

    int caseset = 1;        /* Default is on */
    struct line *hlp = bp->b_linep;
    char lbuf[NLINE];       /* Could be dynamic... */
    char tok[NLINE];
    struct ptt_ent *lastp = NULL;
    for (struct line *lp = hlp->l_fp; lp != hlp; lp = lp->l_fp) {
        char to_string[NLINE] = "";
        int to_len = 0;
        memcpy(lbuf, lp->l_text, lp->l_used);
        char *rp = lbuf;
        lbuf[lp->l_used] = '\0';
        rp = token(rp, tok, NLINE);
        char from_string[NLINE];
        int bow;
        char *from_start;
        if (tok[0] == '^') {
            bow = 1;
            from_start = tok+1;
        }
        else {
            bow = 0;
            from_start = tok;
        }
        strcpy(from_string, from_start);
        if (!strcmp("caseset-on", from_string)) {
            caseset = CASESET_ON;
            continue;
        }
        if (!strcmp("caseset-capinit1", from_string)) {
            caseset = CASESET_CAPI_ONE;
            continue;
        }
        if (!strcmp("caseset-capinitall", from_string)) {
            caseset = CASESET_CAPI_ALL;
            continue;
        }
        if (!strcmp("caseset-lowinit1", from_string)) {
            caseset = CASESET_LOWI_ONE;
            continue;
        }
        if (!strcmp("caseset-lowinitall", from_string)) {
            caseset = CASESET_LOWI_ALL;
            continue;
        }
        if (!strcmp("caseset-off", from_string)) {
            caseset = CASESET_OFF;
            continue;
        }
        if (!strcmp("display-code", from_string)) {
            rp = token(rp, tok, NLINE);
            if (tok[0] != '\0')  {
                mlen = strlen(tok);
                offs = utf8_to_unicode(tok, 0, mlen, &uc1);
                (void)utf8_to_unicode(tok, offs, mlen, &uc2);
                offs = unicode_to_utf8(uc1, ml_display_code);
                offs += unicode_to_utf8(uc2, ml_display_code+offs);
                ml_display_code[offs] = '\0';
            }
            continue;
        }
        while(*rp != '\0') {
            rp = token(rp, tok, NLINE);
            if (tok[0] == '\0') break;
            if (!strncmp(tok, "0x", 2)) {
                long add = strtol(tok+2, NULL, 16);
                to_string[to_len++] = add;
            }
            else if (tok[0] == 'U' && tok[1] == '+') {
                int val = strtol(tok+2, NULL, 16);
                int incr = unicode_to_utf8(val, to_string+to_len);
                to_len += incr;
            }
            else {
                strcat(to_string, tok);
                to_len += strlen(tok);
            }
        }
        if (to_len == 0) continue;
        struct ptt_ent *new = malloc(sizeof(struct ptt_ent));
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
        new->from_len = strlen(from_string);
        if (caseset != CASESET_OFF) {
            new->from = tolower_utf8(from_string, new->from_len,
                 &(new->from_len), &(new->from_len_uc));
        }
        else {
            new->from = malloc(new->from_len+1);
            strcpy(new->from, from_string);
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
        new->to = malloc(to_len+1);
        strncpy(new->to, to_string, to_len);
        new->to[to_len] = '\0';
        new->to_len_uc = uclen_utf8(new->to);
        new->bow_only = bow;
        new->caseset = caseset;
    }
    if (lastp == NULL) return FALSE;
    use_pttable(bp);
    return TRUE;
}


/* GGR
 * Store a phonetic translation table.
 * This starts by storing a procedure, so we just use that code.
 */
static int ptt_storing = 0;
int storepttable(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status = storeproc(0, 0);
    if (status != TRUE) return status;

/* Mark that we need to compile the buffer */

    bstore->b_type = BTPHON;    /* Mark this as a translation buffer */
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
    char pttbuf[NBUFN];
    struct buffer *bp;

/* As soon as a table is defined ptt gets set, so if it isn't
 * we know that there are no translation tables.
 */

    if (!ptt) {
        mlforce("No phonetic translation tables are yet defined!");
        return FALSE;
    }

    status = mlreply("Translation table to use? ", pttbuf+1, NBUFN-2);
    if (status != TRUE) return status;

/* Find the ptt buffer */
    pttbuf[0] = '/';
    if ((bp = bfind(pttbuf, FALSE, BFINVS)) == NULL) {
        mlforce("Table %s was not found", pttbuf);
        return FALSE;
    }

/* Check that it is a translation buffer */

    if (bp->b_type != BTPHON) {
        mlforce("Buffer %s is not a translation buffer.", pttbuf);
        sleep(1);
        return TRUE;    /* Don't abort start-up file */
    }
    use_pttable(bp);    /* This does not actually activate it */
    return TRUE;
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
        bp = bp->b_bufp;                /* Start at next buffer */
        if (bp == NULL) bp = bheadp;
        while (bp != ptt && bp->b_type != BTPHON) {
            bp = bp->b_bufp;            /* Onto next buffer */
            if (bp == NULL) bp = bheadp;
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
 */
int ptt_handler(int c) {

    if (!ptt) return FALSE;
    if (!ptt->ptt_headp && !ptt_compile(ptt))
        return FALSE;

/* We are sent unicode characters
 * So insert the unicode character before testing...
 */

    int orig_doto = curwp->w_doto;
    if (linsert_uc(1, c) != TRUE) return FALSE;

    for (struct ptt_ent *ptr = ptt->ptt_headp; ptr; ptr = ptr->nextp) {
        unicode_t wc = c;       /* A working copy */
        if (ptr->caseset != CASESET_OFF) wc = utf8proc_tolower(wc);
        if (ptr->final_uc != wc) continue;

/* We need to know where <n> unicode chars back starts */
        int start_at = unicode_back_utf8(ptr->from_len_uc,
             curwp->w_dotp->l_text, curwp->w_doto);
        if (ptr->caseset != CASESET_OFF) {
/*
 * Need a unicode-case insensitive strncmp!!!
 * Also, since we can't guarantee that a case-changed string will be
 * the same length, we need to step back the right number of unicode
 * chars first.
 */
            if (start_at < 0) continue; /* Insufficient chars */
            if (nocasecmp_utf8(curwp->w_dotp->l_text,
                 start_at, curwp->w_dotp->l_used,
                 ptr->from, 0, ptr->from_len)) continue;
        }
        else {
/*
 * We need to check there are sufficient chars to check, then
 * just compare the bytes.
 */
            if (curwp->w_doto < ptr->from_len) continue;
            if (strncmp(curwp->w_dotp->l_text+start_at,
                 ptr->from, ptr->from_len)) continue;
        }
        if (ptr->bow_only && (curwp->w_doto > ptr->from_len)) { /* Not BOL */
/* Need to step back to the start of the preceding grapheme and get the
 * base Unicode char from there.
 */
            int offs = prev_utf8_offset(curwp->w_dotp->l_text,
                 start_at, TRUE);
            unicode_t prev_uc;
            (void)utf8_to_unicode(curwp->w_dotp->l_text,
                 offs, curwp->w_dotp->l_used, &prev_uc);

            const char *uc_class =
                 utf8proc_category_string((utf8proc_int32_t)prev_uc);
            if (uc_class[0] == 'L') continue;
        }

/* We have to replace the string with the translation.
 * If we are doing case-setting on the replacement then we
 * need to know the case of the first character.
 */
        curwp->w_doto = start_at;
        int edit_case = 0;
        int set_case;
        if (ptr->caseset != CASESET_OFF) {
            unicode_t fc;
            (void)utf8_to_unicode(curwp->w_dotp->l_text, start_at,
                 curwp->w_dotp->l_used, &fc);
            utf8proc_category_t  need_edit_type;
            if (ptr->caseset == CASESET_LOWI_ALL ||
                ptr->caseset == CASESET_LOWI_ONE) {
                need_edit_type = UTF8PROC_CATEGORY_LL;
                set_case = LOWERCASE;
            }
            else {
                need_edit_type = UTF8PROC_CATEGORY_LU;
                set_case = UPPERCASE;
            }
            if (utf8proc_category(fc) == need_edit_type)
                edit_case = 1;
        }
        ldelete(ptr->from_len, FALSE);
        linstr(ptr->to);

/* We might need to re-case the just-added string.
 * NOTE that we have added everything in the case that the user supplied,
 * and we only change things if the user "from" started with a capital.
 */

        if (edit_case && (ptr->caseset != CASESET_OFF)) {
            int count = ptr->to_len_uc;
            curwp->w_doto = start_at;
            ensure_case(set_case);
            forw_grapheme(1);
            if (ptr->caseset == CASESET_CAPI_ONE) { /* Just step over chars */
                while (--count) {       /* First already done */
                    if (forw_grapheme(1) <= 0)      /* !?! */
                        break;
                }
            }
            else if (ptr->caseset == CASESET_ON) {  /* Do the lot */
/* Like upperword(), but with known char count */
                while (--count) {       /* First already done */
                    if (forw_grapheme(1) <= 0)      /* !?! */
                        break;
                    ensure_case(set_case);
                }
            }
            else if ((ptr->caseset == CASESET_CAPI_ALL) ||
                     (ptr->caseset == CASESET_LOWI_ALL)) { /* Do per-word */
/* Like capword(), but with known char count */
                int was_inword = inword();
                while (--count) {       /* First already done */
                    if (forw_grapheme(1) <= 0)      /* !?! */
                        break;
                    int now_in_word = inword();
                    if (now_in_word && !was_inword) {
                        ensure_case(set_case);
                    }
                    was_inword = now_in_word;
                }
            }

        }
        return TRUE;
    }
/* We have to delete the added character before returning.
 * NOTE!!!  ldelchar() will actually delete a grapheme!
 * But we've just inserted the unichar we are deleting, so it can't
 * consist of multiple unicode chars - and we'll only delete the one.
 */
    curwp->w_doto = orig_doto;
    ldelchar(1, FALSE);
    return FALSE;
}

/*
 * storeproc:
 *      Set up a procedure buffer and flag to store all
 *      executed command lines there
 *
 * int f;               default flag
 * int n;               macro number to use
 */
int storeproc(int f, int n)
{
        struct buffer *bp;      /* pointer to macro buffer */
        int status;             /* return status */
        char bufn[NBUFN+1];     /* name of buffer to use */

        /* a numeric argument means its a numbered macro */
        if (f == TRUE)
                return storemac(f, n);

/* Append the procedure name to the buffer marker tag */
        bufn[0] = '/';
        if ((status =
             mlreply("Procedure name: ", &bufn[1], NBUFN)) != TRUE)
                return status;
        if (strlen(bufn) >= NBUFN) {
            mlforce("Procedure name too long (store): %s. Ignored.", bufn);
            sleep(1);
            return TRUE;    /* Don't abort start-up file */
        }

        /* set up the new macro buffer */
        if ((bp = bfind(bufn, TRUE, BFINVS)) == NULL) {
                mlwrite("Can not create macro");
                return FALSE;
        }

        /* and make sure it is empty */
        bclear(bp);

        /* and set the macro store pointers to it */
        mstore = TRUE;
        bp->b_type = BTPROC;    /* Mark the buffer type */
        bstore = bp;
        return TRUE;
}

/*
 * execproc:
 *      Execute a procedure
 *
 * int f, n;            default flag and numeric arg
 */
int execproc(int f, int n)
{
        UNUSED(f);
        struct buffer *bp;      /* ptr to buffer to execute */
        int status;             /* status return */
        char bufn[NBUFN+1];     /* name of buffer to execute */

/* Append the procedure name to the buffer marker tag */
        bufn[0] = '/';

        if (input_waiting != NULL) {
            strcpy(bufn+1, input_waiting);
            input_waiting = NULL;   /* We've used it */
        }
        else {
            if ((status =
                 mlreply("Execute procedure: ", &bufn[1], NBUFN)) != TRUE)
                    return status;
            if (strlen(bufn) >= NBUFN) {
                mlforce("Procedure name too long (exec): %s. Ignored.", bufn);
                sleep(1);
                return TRUE;    /* Don't abort start-up file */
            }
        }

        /* construct the buffer name */
        bufn[0] = '/';

        /* find the pointer to that buffer */
        if ((bp = bfind(bufn, FALSE, 0)) == NULL) {
                mlwrite("No such procedure");
                return FALSE;
        }

/* Check that it is a procedure buffer */

        if (bp->b_type != BTPROC) {
            mlforce("Buffer %s is not a procedure buffer.", bufn);
            sleep(1);
            return TRUE;    /* Don't abort start-up file */
        }

        /* and now execute it as asked */
        while (n-- > 0)
                if ((status = dobuf(bp)) != TRUE)
                        return status;
        return TRUE;
}
#endif

/*
 * execbuf:
 *      Execute the contents of a buffer of commands
 *
 * int f, n;            default flag and numeric arg
 */
int execbuf(int f, int n) {
    UNUSED(f);
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    char bufn[NSTRING];     /* name of buffer to execute */

/* Find out what buffer the user wants to execute */
    if ((status = mlreply("Execute buffer: ", bufn, NBUFN)) != TRUE)
        return status;

    if (kbdmode != STOP && (strcmp(bufn, kbdmacro_buffer) == 0)) {
        mlwrite("%%Cannot run keyboard macro when collecting it");
        return FALSE;
    }

/* Find the pointer to that buffer */
    if ((bp = bfind(bufn, FALSE, 0)) == NULL) {
        mlwrite("No such buffer");
        return FALSE;
    }

/* And now execute it as asked */
    while (n-- > 0)
        if ((status = dobuf(bp)) != TRUE)
            return status;
    return TRUE;
}

/*
 * dobuf:
 *      execute the contents of the buffer pointed to
 *      by the passed BP
 *
 *      Directives start with a "!" and include:
 *
 *      !endm           End a macro
 *      !if (cond)      conditional execution
 *      !else
 *      !endif
 *      !return         Return (terminating current macro)
 *      !goto <label>   Jump to a label in the current macro
 *      !force          Force macro to continue...even if command fails
 *      !while (cond)   Execute a loop if the condition is true
 *      !endwhile
 *
 *      Line Labels begin with a "*" as the first nonblank char, like:
 *
 *      *LBL01
 *
 * struct buffer *bp;           buffer to execute
 */
int dobuf(struct buffer *bp) {
    int status;             /* status return */
    struct line *lp;        /* pointer to line to execute */
    struct line *hlp;       /* pointer to line header */
    struct line *glp;       /* line to goto */
    struct line *mp;        /* Macro line storage temp */
    int dirnum;             /* directive index */
    int linlen;             /* length of line to execute */
    int i;                  /* index */
    int c;                  /* temp character */
    int force;              /* force TRUE result? */
    struct window *wp;              /* ptr to windows to scan */
    struct while_block *whlist;     /* ptr to !WHILE list */
    struct while_block *scanner;    /* ptr during scan */
    struct while_block *whtemp;     /* temporary ptr to a struct while_block */
    char *einit;            /* initial value of eline */
    char *eline;            /* text of line to execute */
    char tkn[NSTRING];      /* buffer to evaluate an expresion in */
    int return_stat = TRUE; /* What we expect to do */
    int orig_pause_key_index_update;    /* State on entry - to be restored */
#if     DEBUGM
    char *sp;               /* temp for building debug string */
    char *ep;               /* ptr to end of outline */
#endif

/* GGR - Only allow recursion up to a certain level... */

#define MAX_RECURSE 10

    if (bp->b_exec_level > MAX_RECURSE) {
        mlwrite("%%Maximum recursion level, %d, exceeded!", MAX_RECURSE);
        return FALSE;
    }
    bp->b_exec_level++;
    orig_pause_key_index_update = pause_key_index_update;
    pause_key_index_update = 1;

/* Clear IF level flags/while ptr */
    execlevel = 0;
    whlist = NULL;
    scanner = NULL;

/* Scan the buffer to execute, building WHILE header blocks */
    hlp = bp->b_linep;
    lp = hlp->l_fp;
    while (lp != hlp) {
/* Scan the current line */
        eline = lp->l_text;
        i = lp->l_used;

/* Trim leading whitespace */
        while (i-- > 0 && (*eline == ' ' || *eline == '\t')) ++eline;

/* If there's nothing here, don't bother */
        if (i <= 0) goto nxtscan;

/* If is a while directive, make a block... */
        if (eline[0] == '!' && eline[1] == 'w' && eline[2] == 'h') {
            whtemp = (struct while_block *)malloc(sizeof(struct while_block));
            if (whtemp == NULL) {
noram:          mlwrite("%%Out of memory during while scan");
failexit:       freewhile(scanner);
                goto failexit2;
            }
            whtemp->w_begin = lp;
            whtemp->w_type = BTWHILE;
            whtemp->w_next = scanner;
            scanner = whtemp;
        }

/* If is a BREAK directive, make a block... */
        if (eline[0] == '!' && eline[1] == 'b' && eline[2] == 'r') {
            if (scanner == NULL) {
                mlwrite("%%!BREAK outside of any !WHILE loop");
                goto failexit;
            }
            whtemp = (struct while_block *)malloc(sizeof(struct while_block));
            if (whtemp == NULL) goto noram;
            whtemp->w_begin = lp;
            whtemp->w_type = BTBREAK;
            whtemp->w_next = scanner;
            scanner = whtemp;
        }

/* If it is an endwhile directive, record the spot... */
        if (eline[0] == '!' && strncmp(&eline[1], "endw", 4) == 0) {
            if (scanner == NULL) {
                mlwrite("%%!ENDWHILE with no preceding !WHILE in '%s'",
                     bp->b_bname);
                goto failexit;
            }
/* move top records from the scanner list to the whlist until we have
 * moved all BREAK records and one WHILE record.
 */
            do {
                scanner->w_end = lp;
                whtemp = whlist;
                whlist = scanner;
                scanner = scanner->w_next;
                whlist->w_next = whtemp;
            } while (whlist->w_type == BTBREAK);
        }

        nxtscan:          /* on to the next line */
        lp = lp->l_fp;
    }

/* While and endwhile should match! */
    if (scanner != NULL) {
        mlwrite("%%!WHILE with no matching !ENDWHILE in '%s'", bp->b_bname);
        goto failexit;
    }

/* Let the first command inherit the flags from the last one.. */
    thisflag = lastflag;

/* Starting at the beginning of the buffer */
    hlp = bp->b_linep;
    lp = hlp->l_fp;
    while (lp != hlp) {
/* Allocate eline and copy macro line to it */
        linlen = lp->l_used;
        if ((einit = eline = malloc(linlen + 1)) == NULL) {
            mlwrite("%%Out of Memory during macro execution");
            goto failexit2;
        }
        memcpy(eline, lp->l_text, linlen);
        eline[linlen] = 0;      /* make sure it ends */

/* trim leading whitespace */
        while (*eline == ' ' || *eline == '\t') ++eline;

/* dump comments and blank lines */
        if (*eline == ';' || *eline == 0) goto onward;

#if     DEBUGM
/* If $debug == TRUE, every line to execute gets echoed and a key needs
 * to be pressed to continue.
 * ^G will abort the command.
 */

        if (macbug) {
            strcpy(outline, "<<<");

/* Debug macro name */
            strcat(outline, bp->b_bname);
            strcat(outline, ":");

/* Debug if levels */
            strcat(outline, itoa(execlevel));
            strcat(outline, ":");

/* and lastly the line. GGR - if line > 80 chars, chop it */
            if (strlen(eline) > 80) strncat(outline, eline, 80);
            else                    strcat(outline, eline);
            strcat(outline, ">>>");

/* Change all '%' to ':' so mlwrite won't expect arguments */
            sp = outline;
            while (*sp)
                if (*sp++ == '%') {
                    ep = --sp;          /* Advance to the end */
                    while (*ep++);
                    *(ep + 1) = 0;      /* Null terminate the string */
                    while (ep-- > sp)   /* copy backwards */
                        *(ep + 1) = *ep;
                    sp += 2;            /* and advance sp past the new % */
                }

/* Write out the debug line */
            mlforce(outline);
            update(TRUE);

/* And get the keystroke */
            if ((c = get1key()) == abortc) {
                mlforce(MLpre "Macro aborted" MLpost);
                goto failexit2;
            }

            if (c == metac) macbug = FALSE;
        }
#endif

/* Parse directives here.... */
        dirnum = -1;
        if (*eline == '!') {
/* Find out which directive this is */
            ++eline;
            for (dirnum = 0; dirnum < NUMDIRS; dirnum++)
                if (strncmp(eline, dname[dirnum],
                    strlen(dname[dirnum])) == 0)
                break;

/* and bitch if it's illegal */
            if (dirnum == NUMDIRS) {
                mlwrite("%%Unknown Directive");
                goto failexit2;
            }

/* service only the !ENDM macro here */
            if (dirnum == DENDM) {
                if (ptt_storing) {
                    ptt_compile(bstore);
                    ptt_storing = 0;
                }
                mstore = FALSE;
                bstore = NULL;
                goto onward;
            }

/* Restore the original eline.... */
            --eline;
        }

/* If macro store is on, just salt this away */
        if (mstore) {
/* Allocate the space for the line */
            linlen = strlen(eline);
            if ((mp = lalloc(linlen)) == NULL) {
                mlwrite ("Out of memory while storing macro");
                bp->b_exec_level--;
                pause_key_index_update = orig_pause_key_index_update;
                return FALSE;
            }

/* Copy the text into the new line */
            lfillchars(mp, linlen, eline);

/* Attach the line to the end of the buffer */
            bstore->b_linep->l_bp->l_fp = mp;
            mp->l_bp = bstore->b_linep->l_bp;
            bstore->b_linep->l_bp = mp;
            mp->l_fp = bstore->b_linep;
            goto onward;
        }


        force = FALSE;

/* Dump comments */
        if (*eline == '*') goto onward;

/* Now, execute directives */
        if (dirnum != -1) {
/* Skip past the directive */
            while (*eline && *eline != ' ' && *eline != '\t') ++eline;
            execstr = eline;

            switch (dirnum) {
            case DIF:       /* IF directive */
/* Grab the value of the logical exp */
                if (execlevel == 0) {
                    if (macarg(tkn) != TRUE) goto eexec;
                    if (stol(tkn) == FALSE) ++execlevel;
                }
                else ++execlevel;
                goto onward;

            case DWHILE:    /* WHILE directive */
/* Grab the value of the logical exp */
                if (execlevel == 0) {
                    if (macarg(tkn) != TRUE) goto eexec;
                    if (stol(tkn) == TRUE) goto onward;
                }
/* Drop down and act just like !BREAK */
                /* Falls through */
            case DBREAK:    /* BREAK directive */
                if (dirnum == DBREAK && execlevel) goto onward;

/* Jump down to the endwhile
 * Find the right while loop
 */
                whtemp = whlist;
                while (whtemp) {
                    if (whtemp->w_begin == lp) break;
                    whtemp = whtemp->w_next;
                }

                if (whtemp == NULL) {
                    mlwrite ("%%Internal While loop error");
                    goto failexit2;
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
/* Grab label to jump to */
                    eline = token(eline, golabel, NPAT);
                    linlen = strlen(golabel);
                    glp = hlp->l_fp;
                    while (glp != hlp) {
                        if (*glp->l_text == '*' &&
                            (strncmp(&glp->l_text[1], golabel, linlen) == 0)) {
                            lp = glp;
                            goto onward;
                        }
                        glp = glp->l_fp;
                    }
                    mlwrite("%%No such label");
                    goto failexit2;
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
                    whtemp = whlist;
                    while (whtemp) {
                        if (whtemp->w_type == BTWHILE &&
                            whtemp->w_end == lp) break;
                        whtemp = whtemp->w_next;
                    }

                    if (whtemp == NULL) {
                        mlwrite ("%%Internal While loop error");
                        goto failexit2;
                    }

/* reset the line pointer back.. */
                    lp = whtemp->w_begin->l_bp;
                    goto onward;
                }

            case DFORCE:    /* FORCE directive */
                force = TRUE;
                break;  /* GGR: Must drop down!! */

            case DFINISH:   /* FINISH directive */
                if (execlevel == 0) {
                    return_stat = FALSE;
                    goto eexec;
                }
                goto onward;
            }
        }

/* Execute the statement */
        status = docmd(eline);
        if (force) status = TRUE;       /* force the status */

/* Check for a command error */
        if (status != TRUE) {
/* Look if buffer is showing */
            wp = wheadp;
            while (wp != NULL) {
                if (wp->w_bufp == bp) { /* And point it */
                    wp->w_dotp = lp;
                    wp->w_doto = 0;
                    wp->w_flag |= WFHARD;
                }
                wp = wp->w_wndp;
            }
/* In any case set the buffer . */
            bp->b_dotp = lp;
            bp->b_doto = 0;
            free(einit);
            execlevel = 0;
            freewhile(whlist);
            bp->b_exec_level--;
            pause_key_index_update = orig_pause_key_index_update;
            return status;
        }

onward:                 /* On to the next line */
        free(einit);
        lp = lp->l_fp;
    }

eexec:                  /* Exit the current function */
    execlevel = 0;
    freewhile(whlist);
    bp->b_exec_level--;
    pause_key_index_update = orig_pause_key_index_update;
    return return_stat;

/* This sequence is used for several exits, so only write it once */
failexit2:
    freewhile(whlist);
    bp->b_exec_level--;
    pause_key_index_update = orig_pause_key_index_update;
    return FALSE;
}

/*
 * free a list of while block pointers
 *
 * struct while_block *wp;              head of structure to free
 */
void freewhile(struct while_block *wp)
{
        if (wp == NULL)
                return;
        if (wp->w_next)
                freewhile(wp->w_next);
        free(wp);
}

/*
 * execute a series of commands in a file
 * If given fname starts with "^", remove that character and don't
 * return any error status.
 * int f, n;            default flag and numeric arg to pass on to file
 */
static int include_level = 0;
#define MAX_INCLUDE_LEVEL 8
int execfile(int f, int n)
{
        UNUSED(f);
        int status;             /* return status of name query */
        char fname[NSTRING];    /* name of file to execute */
        char *fspec;            /* full file spec */
        int fail_ok = 0;
        int fns = 0;

        if ((status =
             mlreply("File to execute: ", fname, NSTRING - 1)) != TRUE)
                return status;

        if (include_level >= MAX_INCLUDE_LEVEL) {
                mlwrite("Include depth too great (%d)!",  include_level+1);
                return FALSE;
        }
        if (fname[0] == '^') {
                fail_ok = 1;
                fns = 1;
        }
/* Don't look in HOME!
 * This allows you to have uemacs.rc in HOME that includes a system one
 * by using "include-file uemacs.rc"
 */
        fspec = flook(fname+fns, FALSE, INTABLE);

        /* if it isn't around */
        if (fspec == NULL) {
                mlwrite("Include file %s not found!", fname+fns);
                return fail_ok? TRUE: FALSE;
        }

        /* otherwise, execute it */
        while (n-- > 0) {
                include_level++;
                status = dofile(fspec);
                include_level--;
                if (status != TRUE) return fail_ok? TRUE: status;
        }
        return TRUE;
}

/*
 * dofile:
 *      yank a file into a buffer and execute it
 *      if there are no errors, delete the buffer on exit
 *
 * char *fname;         file name to execute
 */
int dofile(char *fname)
{
        struct buffer *bp;      /* buffer to place file to exeute */
        struct buffer *cb;      /* temp to hold current buf while we read */
        int status;             /* results of various calls */
        char bufn[NBUFN];       /* name of buffer */

        makename(bufn, fname);  /* derive the name of the buffer */
        unqname(bufn);          /* make sure we don't stomp things */
        if ((bp = bfind(bufn, TRUE, 0)) == NULL)    /* get the needed buffer */
                return FALSE;

        bp->b_mode = MDVIEW;    /* mark the buffer as read only */
        cb = curbp;             /* save the old buffer */
        curbp = bp;             /* make this one current */
        if (silent)                 /* GGR */
                pathexpand = FALSE;
        /* and try to read in the file to execute */
        if ((status = readin(fname, FALSE)) != TRUE) {
                curbp = cb;     /* restore the current buffer */
                pathexpand = TRUE;  /* GGR */
                return status;
        }
        pathexpand = TRUE;          /* GGR */

        /* go execute it! */
        curbp = cb;             /* restore the current buffer */
        if ((status = dobuf(bp)) != TRUE)
                return status;

        /* if not displayed, remove the now unneeded buffer and exit */
        if (bp->b_nwnd == 0)
                zotbuf(bp);
        return TRUE;
}

/*
 * cbuf:
 *      Execute the contents of a numbered buffer
 *
 * int f, n;            default flag and numeric arg
 * int bufnum;          number of buffer to execute
 */
int cbuf(int f, int n, int bufnum) {
    UNUSED(f);
    struct buffer *bp;      /* ptr to buffer to execute */
    int status;             /* status return */
    static char bufname[] = "/Macro xx";

/* Make the buffer name */
    bufname[7] = '0' + (bufnum / 10);
    bufname[8] = '0' + (bufnum % 10);

/* Find the pointer to that buffer */
    if ((bp = bfind(bufname, FALSE, 0)) == NULL) {
        mlwrite("Macro not defined");
        return FALSE;
    }

/* and now execute it as asked */
    while (n-- > 0)
        if ((status = dobuf(bp)) != TRUE)
            return status;
    return TRUE;
}

int cbuf1(int f, int n) {
    return cbuf(f, n, 1);
}

int cbuf2(int f, int n) {
    return cbuf(f, n, 2);
}

int cbuf3(int f, int n) {
    return cbuf(f, n, 3);
}

int cbuf4(int f, int n) {
    return cbuf(f, n, 4);
}

int cbuf5(int f, int n) {
    return cbuf(f, n, 5);
}

int cbuf6(int f, int n) {
    return cbuf(f, n, 6);
}

int cbuf7(int f, int n) {
    return cbuf(f, n, 7);
}

int cbuf8(int f, int n) {
    return cbuf(f, n, 8);
}

int cbuf9(int f, int n) {
    return cbuf(f, n, 9);
}

int cbuf10(int f, int n) {
    return cbuf(f, n, 10);
}

int cbuf11(int f, int n) {
    return cbuf(f, n, 11);
}

int cbuf12(int f, int n) {
    return cbuf(f, n, 12);
}

int cbuf13(int f, int n) {
    return cbuf(f, n, 13);
}

int cbuf14(int f, int n) {
    return cbuf(f, n, 14);
}

int cbuf15(int f, int n) {
    return cbuf(f, n, 15);
}

int cbuf16(int f, int n) {
    return cbuf(f, n, 16);
}

int cbuf17(int f, int n) {
    return cbuf(f, n, 17);
}

int cbuf18(int f, int n) {
    return cbuf(f, n, 18);
}

int cbuf19(int f, int n) {
    return cbuf(f, n, 19);
}

int cbuf20(int f, int n) {
    return cbuf(f, n, 20);
}

int cbuf21(int f, int n) {
    return cbuf(f, n, 21);
}

int cbuf22(int f, int n) {
    return cbuf(f, n, 22);
}

int cbuf23(int f, int n) {
    return cbuf(f, n, 23);
}

int cbuf24(int f, int n) {
    return cbuf(f, n, 24);
}

int cbuf25(int f, int n) {
    return cbuf(f, n, 25);
}

int cbuf26(int f, int n) {
    return cbuf(f, n, 26);
}

int cbuf27(int f, int n) {
    return cbuf(f, n, 27);
}

int cbuf28(int f, int n) {
    return cbuf(f, n, 28);
}

int cbuf29(int f, int n) {
    return cbuf(f, n, 29);
}

int cbuf30(int f, int n) {
    return cbuf(f, n, 30);
}

int cbuf31(int f, int n) {
    return cbuf(f, n, 31);
}

int cbuf32(int f, int n) {
    return cbuf(f, n, 32);
}

int cbuf33(int f, int n) {
    return cbuf(f, n, 33);
}

int cbuf34(int f, int n) {
    return cbuf(f, n, 34);
}

int cbuf35(int f, int n) {
    return cbuf(f, n, 35);
}

int cbuf36(int f, int n) {
    return cbuf(f, n, 36);
}

int cbuf37(int f, int n) {
    return cbuf(f, n, 37);
}

int cbuf38(int f, int n) {
    return cbuf(f, n, 38);
}

int cbuf39(int f, int n) {
    return cbuf(f, n, 39);
}

int cbuf40(int f, int n) {
    return cbuf(f, n, 40);
}
