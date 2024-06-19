/*      isearch.c
 *
 * The functions in this file implement commands that perform incremental
 * searches in the forward and backward directions.  This "ISearch" command
 * is intended to emulate the same command from the original EMACS
 * implementation (ITS).  Contains references to routines internal to
 * SEARCH.C.
 *
 * REVISION HISTORY:
 *
 *      D. R. Banks 9-May-86
 *      - added ITS EMACSlike ISearch
 *
 *      John M. Gamble 5-Oct-86
 *      - Made iterative search use search.c's scanner() routine.
 *        This allowed the elimination of bakscan().
 *      - Put isearch constants into estruct.h
 *      - Eliminated the passing of 'status' to scanmore() and
 *        checknext(), since there were no circumstances where
 *        it ever equalled FALSE.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>

#define ISEARCH_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* Incremental search defines.
 */
#define CMDBUFLEN       256     /* Length of our command buffer */

#define CTLCHAR(x) (x & ~0x60)          /* UP or low case -> control */
#define IS_ABORT        (CTLCHAR('G'))  /* Abort the isearch */
#define IS_BACKSP       (CTLCHAR('H'))  /* Delete previous char */
#define IS_QUOTE        (CTLCHAR('Q'))  /* Quote next character */
#define IS_REVERSE      (CTLCHAR('R'))  /* Search backward */
#define IS_FORWARD      (CTLCHAR('S'))  /* Search forward */
#define IS_QUIT         (CTLCHAR('['))  /* Exit the search */
#define IS_RUBOUT       (0x7F)          /* Delete previous character */

/* A couple more "own" variables for the command string */

static int cmd_buff[CMDBUFLEN]; /* Save the command args here */
static int cmd_offset;                  /* Current offset into command buff */
static int cmd_reexecute = -1;          /* > 0 if re-executing command */

/* Routine to prompt for I-Search string.
 */
static int promptpattern(char *prompt) {
    db_def(tpat);

/* check to see whether we are executing a command line */

    if (clexec) return 0;

    db_set(tpat, prompt);    /* copy prompt to output string */
    db_append(tpat, MLbkt("<Meta>") " ");
    mlwrite_one(db_val(tpat));

/* This now needs the grapheme length of the byte array... */

    int glen = 0;
    int tplen = db_len(tpat);
    int tpi = 0;
    while (tpi < tplen) {
        struct grapheme dummy_gc;
/* Don't build any ex... */
        tpi = build_next_grapheme(db_val(tpat), tpi, tplen, &dummy_gc, 1);
        glen++;
    }
    db_free(tpat);
    return glen;
}

/* routine to echo i-search characters
 * GGR - This is *NOT* a mini-buffer, and tracks its own column.
 *
 * int c;               character to be echoed
 * int col;             column to be echoed in
 */
static int echo_str(char *str) {
    int cw = strlen(str);
    while(*str) TTputc(*str++);
    return cw;
}

static int echo_char(int c, int col) {
    movecursor(term.t_mbline, col); /* Position the cursor         */
    int cw;
    if ((c < ' ') || (c == 0x7F)) { /* Control character?          */
        switch (c) {                /* Yes, dispatch special cases */
        case '\n':                  /* Newline                     */
            cw = echo_str("<NL>");
            break;

        case '\r':                  /* Carriage return             */
            cw = echo_str("<CR>");
            break;

        case '\t':                  /* Tab                         */
            cw = echo_str("<TAB>");
            break;

        case 0x7F:                  /* Rubout:                     */
            cw = echo_str("^?");
            break;

        default: {                  /* Vanilla control char        */
            static char gen_ctl[3] = "^ ";
            gen_ctl[1] = c | 0x40;
            cw = echo_str(gen_ctl);
        }
        }
    }
    else {
        cw = utf8char_width(c);
        TTputc(c);                  /* Otherwise, output raw char */
    }
    TTflush();                      /* Flush the output           */
    return col + cw;                /* return the new column no   */
}

/* Routine to get the next character from the input stream.  If we're reading
 * from the real terminal, force a screen update before we get the char.
 * Otherwise, we must be re-executing the command string, so just return the
 * next character.
 */
static int get_char(void) {
    int c;                      /* A place to get a character   */

/* See if we're re-executing:
 * If so we want to play out all of the characters again, in order,
 * so that we replay the whole thing.
 */

    if (cmd_reexecute >= 0)     /* Is there an offset?          */
        if ((c = cmd_buff[cmd_reexecute++]) != 0)
            return c;           /* Yes, return any character    */

/* We're not re-executing (or aren't any more).  Try for a real char */

    cmd_reexecute = -1;     /* Say we're in real mode again     */
    update(FALSE);          /* Pretty up the screen             */
/* If we're getting too big ...  Complain loudly and bitterly */
    if (cmd_offset >= CMDBUFLEN - 1) {
        mlwrite_one("? command too long");
        return metac;                   /* And force a quit */
    }
    c = tgetc();            /* Get the next literal character      */
    cmd_buff[cmd_offset++] = c;     /* Save the char for next time */
    terminate_str(cmd_buff + cmd_offset);
    return c;               /* Return the character                */
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * Functions to check and set-up incremental debug mode.
 * This is *ONLY* intended to be used for running functional tests
 * that require "interactive" input!
 * That is incremental searching and query replace.
 * These MUST save the value of discmd when they start and restore that
 * when they exit.
 * I can see no other use for this beyond the autotests....
 */
static int (*prev_idbg_func) (void);    /* Get character routine */
static int call_time;                   /* 0 = delayed, 1 = instant */

/* The structures to prime wwith the required input and check.
 * These are freed when the seacrh finishes.
 */
struct pending {
    unicode_t *input;   /* Input as a unicode_t array */
    char *uproc;        /* Check function (user-proc) to call */
    int ilen;           /* Entries in input */
};
struct incr_input {
    int ci;             /* index of next char */
    int np;             /* Number of valid pending entries */
    int mp;             /* Number of allocated entries */
    struct pending pdg[0];
};

static struct incr_input *ii = NULL;

/* Push a string/function pair onto the array, allocating it as required.
 */
int simulate_incr(int f, int n) {
    UNUSED(f); UNUSED(n);

/* If we don't have an allocated ii, get one now */
    if (!ii) {
        ii = Xmalloc(sizeof(struct incr_input) + sizeof(struct pending));
        ii->ci = 0;
        ii->np = 0;
        ii->mp = 1;
    }
/* Will we need more space? */
    ii->np++;
    if (ii->np > ii->mp) {
        ii = Xrealloc(ii, sizeof(struct incr_input) + ii->np*sizeof(struct pending));
        ii->mp = ii->np;
    }

/* Get the first token - this contains the input characters (Unicode) */
    db_def(ntok);
    macarg(&ntok);          /* This handles functions on command line */

/* Allocate as many unicode_t entries as we have bytes */
    struct pending *iip = &(ii->pdg[ii->np-1]);
    int mlen = db_len(ntok);
    iip->input = Xmalloc(mlen*sizeof(unicode_t));
    int dbc = 0;
    int offs = 0;
    while (mlen > offs) {
        unicode_t tc;
        int used = utf8_to_unicode(db_val(ntok), offs, mlen, &tc);
        iip->input[dbc++] = tc;
        offs += used;
    }
    iip->ilen = dbc;

/* Is there another token - the proc to run? */
    execstr = token(execstr, &ntok);
    if (db_len(ntok) == 0) iip->uproc = NULL;    /* No */
    else iip->uproc = Xstrdup(db_val(ntok));     /* Yes, so remember it */

    db_free(ntok);
    return TRUE;
}

/* Function to run the command, which we may wish to do either
 * as we get the char to return (query-replace), or when we get the
 * next one (incremental-search).
 */
static void activate_cmd(void) {
    if (!ii->pdg[0].uproc) return;  /* Run first on list - if there */
    char *prev_execstr = execstr;   /* In case we are already running */
    execstr = ii->pdg[0].uproc;     /* Fudge name in as command line */
    int prev_inreex = inreex;
    inreex = FALSE;
    execproc(0, 0);                 /* It must be a user proc */
    execstr = prev_execstr;
    inreex = prev_inreex;
}

/* Get the "next character" from the debug buffer.
 * This is the next character from the first token on the line.
 * The rest of the line is user-proc command to be executed *after*
 * the last character in the string is enacted.
 */
static int idbg_nextchar(void) {

/* Do we need to move on to the next input buffer?. */

    if (ii->ci >= ii->pdg[0].ilen) {            /* Valid buffer chars */
        if (--ii->np == 0) return UEM_NOCHAR;   /* No more buffers! */
        if (call_time == 0) activate_cmd();     /* End-of-buf proc */
        Xfree(ii->pdg[0].input);
        Xfree(ii->pdg[0].uproc);
        for (int i = 1; i < ii->mp; i++) {
            ii->pdg[i-1] = ii->pdg[i];
        }
        ii->pdg[ii->mp-1].input  = NULL;
        ii->pdg[ii->mp-1].uproc = NULL;
        ii->ci = 0;
        if (call_time == 1) activate_cmd();     /* Start-of-buf proc */
    }
    return ii->pdg[0].input[ii->ci++];          /* Return the result */
}

/* Function to check whether to use this debug mechanism.
 * If it is set-up we use it.
 */
int incremental_debug_check(int type) {
    if (!ii) return FALSE;      /* Not set-up */

/* Now set-up the function that gets the "next character" from it. */

    prev_idbg_func = term.t_getchar;
    term.t_getchar = idbg_nextchar;
/* The call time type is:
 *
 *  0   Used by f/risearch(). Run the check proc *after* all of the
 *      input has been used (so it is actually run at the *start* of
 *      the next buffer read)
 *      This allows you to check the result of the input buffer.
 *
 *  1   Used by qreplace(). Run the proc *before* returning the
 *      first entry from the buffer.
 *      This allows you to test the result of the search before
 *      responding to what you want to do with the match.
 */
    call_time = type;

/* Whilst this is running we don't want any update() to run.
 * We've already saved the original discmd (in fisearch() or
 * replaces()) just in case.
 */
    discmd = FALSE;
    return TRUE;
}

/* Remove the incremtal input structure
 * This is to save the macro-caller having to do it, as it is
 * active if present.
 * Freeing NULL is not an error....
 */
void incremental_debug_cleanup(void) {

    if ((ii->np != 0) || (ii->ci < ii->pdg[0].ilen)) {
        mlforce_one("Unused incremental debug!");
        for (int i = 1; i < ii->mp; i++) {
            Xfree(ii->pdg[i].input);
            Xfree(ii->pdg[i].uproc);
        }
    }
    Xfree(ii->pdg[0].input);
    Xfree(ii->pdg[0].uproc);
    Xfree(ii);
    ii = NULL;
    term.t_getchar = prev_idbg_func;
    return;
}

/* Redisplay the given character at the given column, in reverse video.
 * The col gets set back to where it was, so no need to return anything.
 */
static void hilite(int c, int col) {
    int cw;
    if ((c < ' ') || (c == 0x7F)) { /* Control character? */
        switch (c) {                /* Dispatch special cases */
        case '\n':
        case '\r':  cw = 4; break;  /* <NL>, <CR> */
        case '\t':  cw = 5; break;  /* <TAB>      */
        default:    cw = 2; break;  /* ^. or ^?   */
        }
    }
    else {
        cw = utf8char_width(c);   /* Set it back the correct amount */
    }
    col -= cw;

/* Need force_movecursor as movecursor thinks we haven't moved */
    force_movecursor(term.t_mbline, col);
    TTrev(1);
    (void)echo_char(c, col);    /* Character in REV video   */
    TTrev(0);
    return;
}

/* Subroutine to do an incremental search.  In general, this works similarly
 * to the older micro-emacs search function, except that the search happens
 * as each character is typed, with the screen and cursor updated with each
 * new search character.
 *
 * While searching forward, each successive character will leave the cursor
 * at the end of the entire matched string.  Typing a Control-S  will cause
 * the next occurrence of the string to be searched for (where the next
 * occurrence does NOT overlap the current occurrence).  A Control-R will
 * change to a backwards search, META will terminate the search and Control-G
 * will abort the search.  Backspace or Delete  will back up to the previous
 * match of the string, or if the starting point is reached first, it will
 * delete the last character from the search string.
 *
 * While searching backward, each successive character will leave the cursor
 * at the beginning of the matched string.  Typing a Control-R will search
 * backward for the next occurrence of the string.  Control-S will revert
 * the search to the forward direction.  In general, the reverse
 * incremental search is just like the forward incremental search inverted.
 *
 * In all cases, if the search fails, the user will be feeped, and the search
 * will stall until the pattern string is edited back into something that
 * exists (or until the search is aborted).
 * Leave via a common exit so that group info can be invalidated.
 */
static int isearch(int f, int n) {
    UNUSED(f);
    int status;             /* Search status */
    int col;                /* prompt column */
    int cpos;       /* character number in search string  */
    int c;          /* current input character */

/* GGR - Allow for a trailing NUL */
    db_def(pat_save);       /* Saved copy of the old pattern str  */
    struct line *curline;   /* Current line on entry              */
    int curoff;             /* Current offset on entry            */
    int init_direction;     /* The initial search direction       */

/* Initialize starting conditions */

    cmd_reexecute = -1;         /* We're not re-executing (yet?)      */
    cmd_offset = 0;             /* Start at the beginning of the buff */
    terminate_str(cmd_buff);    /* Init the command buffer            */
    curline = curwp->w.dotp;    /* Save the current line pointer      */
    curoff = curwp->w.doto;     /* Save the current offset            */
    init_direction = n;         /* Save the initial search direction  */

    db_set(pat_save, db_val_nc(pat)); /* Save the old pattern string */
    db_set(pat, "");            /* Start with nothing */

/* Check for in "incremental-debug" mode? */

    int using_incremental_debug = incremental_debug_check(0);

/* This is a good place to start a re-execution: */

start_over:

/* Ask the user for the text of a pattern */
    col = promptpattern("ISearch:");    /* Prompt, remember the col */

    cpos = 0;               /* Start afresh               */
    status = TRUE;          /* Assume everything's cool   */

/* Get the first character in the pattern.  If we get an initial
 * Control-S or Control-R, re-use the old search string and find
 * the first occurrence
 */
    c = get_char();         /* Get the first character    */
    if ((c == IS_FORWARD) || (c == IS_REVERSE)) {
/* Reuse old search string?   */
/* Yup, find the grapheme length and re-echo the string. */
        cpos = 0;
        int plen = db_len(pat);
        int final_char = '!';
        while (db_charat(pat, cpos) != 0) {
            unicode_t uc;
            cpos += utf8_to_unicode(db_val(pat), cpos, plen, &uc);
            col = echo_char(uc, col);
            final_char = uc;
	}

        if (c == IS_REVERSE) {      /* forward search?        */
            n = -1;                 /* No, search in reverse  */
            if (curwp->w.dotp == curbp->b_linep ) {
                back_grapheme(1);   /* Be defensive about EOB */
            }
        }
        else
            n = 1;                  /* Yes, search forward    */
                                    /* Do the search */
        status = scanmore(&pat, n, FALSE, FALSE);
        if (!status) hilite(final_char, col);
        c = get_char();             /* Get another character */
    }

/* Top of the per character loop, although only IS_REVERSE and
 * IS_FORWARD actually do loop.
 */

    while (1) {         /* ISearch per character loop */
                        /* Check for special characters first: */
                        /* Most cases here change the search */
        switch (c) {            /* dispatch on the input char */
        case IS_QUIT:           /* Want to quit searching?    */
            status = TRUE;
            goto end_isearch;   /* Quit searching now         */
        case UEM_NOCHAR:        /* Run out of input?          */
        case IS_ABORT:          /* If abort search request    */
            status = FALSE;
            goto end_isearch;   /* Quit searching again       */

        case IS_REVERSE:    /* If backward search         */
        case IS_FORWARD:    /* If forward search          */
            if (c == IS_REVERSE)    /* If reverse search  */
                n = -1;             /* Set the reverse direction  */
            else                    /* Otherwise,         */
                n = 1;              /*  go forward        */
/* This calls asks for the *next* match, not a continuation of the
 * current one.
 */
            status = scanmore(&pat, n, TRUE, FALSE);    /* Restart */
            if (!status) hilite('!', col+1);        /* No further match */
            c = get_char(); /* Get next char */
            continue;       /* Continue the search */

        case IS_QUOTE:      /* Quote next character       */
            c = get_char(); /* Get the next char - might be a control */
            break;

/* The cmd_buff collects *all* chars, including the IS_FORWARD and
 * IS_REVERSE chars, so it remembers the entire history of how you
 * got to here.
 * Backspace after IS_FORWARD/IS_REVERSE undoes the last find command,
 * but leaves the search string (which is displayed) the same.
 */
        case IS_BACKSP:     /* If a backspace:      */
        case IS_RUBOUT:     /*  or if a Rubout:     */
            if (cmd_offset <= 1) {  /* Anything to delete?  */
                status = TRUE;      /* No, just exit        */
                goto end_isearch;
            }
            --cmd_offset;           /* Back up over Rubout  */
            terminate_str(cmd_buff + --cmd_offset); /* Yes, delete last char */
            curwp->w.dotp = curline;        /* Reset the line pointer */
            curwp->w.doto = curoff; /*  and the offset       */
            n = init_direction;     /* Reset search direction */
            db_set(pat, db_val(pat_save)); /* Restore old search str */
            cmd_reexecute = 0;      /* Start the whole mess over  */
            goto start_over;        /* Let it take care of itself */

/* Presumably a quasi-normal character comes here.
 * This can include control-chars not explicitly handled.
 */
        default:                /* All other chars        */
            break;
        }               /* Switch */

/* I guess we got something to search for, so search for it */

/* Handle unicode...more than 1 byte */
        char ucb[6];
        int nb;
        if (c > 0x7f) {
            nb = unicode_to_utf8(c, ucb);
        }
        else {
            nb = 1;
            ucb[0] = c;
        }
        db_appendn(pat, ucb, nb);
        col = echo_char(c, col);    /* Echo the character         */
        if (!status) {              /* If we lost last time       */
            TTputc(BELL);           /* Feep again                 */
            TTflush();              /* see that the feep feeps    */
        }
        else                        /* Otherwise, we must have won */
                                    /* find next match */
             status = scanmore(&pat, n, FALSE, TRUE);

        if (!status) hilite(c, col);
        c = get_char();             /* Get the next char        */
/* Exit if we run out of input.... */
        if (c == UEM_NOCHAR) break;
    }
end_isearch:
    (void)scanmore(NULL, 0, 0, 0);  /* Invalidate group matches */
    if (using_incremental_debug) incremental_debug_cleanup();
    db_free(pat_save);
    return status;
}

/* Incremental search entry - forward direction
 */
int fisearch(int f, int n) {
    struct line *curline;           /* Current line on entry    */
    int curoff;                     /* Current offset on entry  */

/* Remember the initial . on entry: */

    int saved_discmd = discmd;      /* Save this in ase we change it. */
    curline = curwp->w.dotp;        /* Save the current line pointer */
    curoff = curwp->w.doto;         /* Save the current offset       */

/* Do the search */

    if (!(isearch(f, n))) {         /* Call ISearch forwards  */
                                    /* If error in search:    */
        curwp->w.dotp = curline;    /* Reset line pointer and */
        curwp->w.doto = curoff;     /* offset to orig value   */
        curwp->w_flag |= WFMOVE;    /* Say we've moved        */
        update(FALSE);              /* And force an update    */
        mlwrite_one(MLbkt("search failed"));   /* Say we died */
    } else
        mlerase();      /* If happy, just erase the cmd line  */
    srch_patlen = db_len(pat);   /* Save default search pattern length */
    discmd = saved_discmd;          /* Back to original... */
    return TRUE;
}
/* Reverse direction uses same code, just reverses direction */
int risearch(int f, int n) {        /* Same as fisearch in reverse */
    return fisearch(f, -n);
}
