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

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/*
 * Incremental search defines.
 */
#define CMDBUFLEN       256     /* Length of our command buffer */

#define IS_ABORT        (CONTROL|'G')   /* Abort the isearch */
#define IS_BACKSP       (CONTROL|'H')   /* Delete previous char */
#define IS_TAB          (CONTROL|'I')   /* Tab character (allowed search char) */
#define IS_NEWLINE      (CONTROL|'M')   /* New line from keyboard (Carriage return) */
#define IS_QUOTE        (CONTROL|'Q')   /* Quote next character */
#define IS_REVERSE      (CONTROL|'R')   /* Search backward */
#define IS_FORWARD      (CONTROL|'S')   /* Search forward */
#define IS_QUIT         (CONTROL|'[')   /* Exit the search */
#define IS_RUBOUT       (0x7F)          /* Delete previous character */

/* A couple of "own" variables for re-eat */

static int (*saved_get_char) (void);    /* Get character routine */
static int eaten_char = -1;             /* Re-eaten char */

/* A couple more "own" variables for the command string */

static int cmd_buff[CMDBUFLEN]; /* Save the command args here */
static int cmd_offset;                  /* Current offset into command buff */
static int cmd_reexecute = -1;          /* > 0 if re-executing command */

/* Routine to prompt for I-Search string.
 */
static int promptpattern(char *prompt) {
    char tpat[NPAT + 20];

    strcpy(tpat, prompt);       /* copy prompt to output string */
    strcat(tpat, MLbkt("<Meta>"));
    strcat(tpat, " ");

/* check to see if we are executing a command line */
    if (!clexec) mlwrite_one(tpat);

/* This now needs the grapheme length of the byte array... */

    int glen = 0;
    int tplen = strlen(tpat);
    int tpi = 0;
    while (tpi < tplen) {
        struct grapheme dummy_gc;
/* Don't build any ex... */
        tpi = build_next_grapheme(tpat, tpi, tplen, &dummy_gc, 1);
        glen++;
    }
    return glen;
}

/* routine to echo i-search characters
 * GGR - This is *NOT* a mini-buffer, and tracks its own column.
 *
 * int c;               character to be echoed
 * int col;             column to be echoed in
 */
static int echo_char(int c, int col) {
    movecursor(term.t_nrow, col);   /* Position the cursor         */
    if ((c < ' ') || (c == 0x7F)) { /* Control character?          */
        switch (c) {                /* Yes, dispatch special cases */
        case '\n':                  /* Newline                     */
            TTputc('<');
            TTputc('N');
            TTputc('L');
            TTputc('>');
            col += 3;
            break;

        case '\t':                  /* Tab                        */
            TTputc('<');
            TTputc('T');
            TTputc('A');
            TTputc('B');
            TTputc('>');
            col += 4;
            break;

        case 0x7F:                  /* Rubout:                    */
            TTputc('^');            /* Output a funny looking     */
            TTputc('?');            /*  indication of Rubout      */
            col++;                  /* Count the extra char       */
            break;

        default:                    /* Vanilla control char       */
            TTputc('^');            /* Yes, output prefix         */
            TTputc(c + 0x40);       /* Make it "^X"               */
            col++;                  /* Count this char            */
        }
    } else
        TTputc(c);                  /* Otherwise, output raw char */
    TTflush();                      /* Flush the output           */
    int cw = utf8char_width(c);
    return col + cw;                /* return the new column no   */
}

/* Routine to get the next character from the input stream.  If we're reading
 * from the real terminal, force a screen update before we get the char.
 * Otherwise, we must be re-executing the command string, so just return the
 * next character.
 */
static int get_char(void) {
    int c;                      /* A place to get a character   */

/* See if we're re-executing: */

    if (cmd_reexecute >= 0)     /* Is there an offset?          */
        if ((c = cmd_buff[cmd_reexecute++]) != 0)
            return c;           /* Yes, return any character    */

/* We're not re-executing (or aren't any more).  Try for a real char */

    cmd_reexecute = -1;     /* Say we're in real mode again     */
    update(FALSE);          /* Pretty up the screen             */
    if (cmd_offset >= CMDBUFLEN - 1) {  /* If we're getting too big ... */
        mlwrite_one("? command too long");  /* Complain loudly and bitterly */
        return metac;                   /* And force a quit */
    }
    c = get1key();          /* Get the next character              */
    cmd_buff[cmd_offset++] = c;     /* Save the char for next time */
    cmd_buff[cmd_offset] = '\0';    /* And terminate the buffer    */
    return c;               /* Return the character                */
}

/* Hacky routine to re-eat a character.  This will save the character to be
 * re-eaten by redirecting the input call to a routine here.  Hack, etc.
 *
 * Come here on the next term.t_getchar call.
 */
static int uneat(void) {
    int c;

    term.t_getchar = saved_get_char;    /* restore the routine address */
    c = eaten_char;                     /* Get the re-eaten char       */
    eaten_char = -1;                    /* Clear the old char          */
    return c;                           /* and return the last char    */
}

void reeat(int c) {
    if (eaten_char != -1)               /* If we've already been here    */
        return /*(NULL) */;             /* Don't do it again             */
    eaten_char = c;                     /* Else, save the char for later */
    saved_get_char = term.t_getchar;    /* Save the char get routine     */
    term.t_getchar = uneat;             /* Replace it with ours          */
}

/* Functions to check and set-up incremental debug mode.
 * This is only intended to be used for runnign functional tests
 * I can see no other use for it....
 */
#define IDB_BFR "//incremental-debug"
static struct buffer *idbg_buf;
static struct line *idbg_line, *idbg_head;
static int (*prev_idbg_func) (void);    /* Get character routine */
static char next_cmd[NLINE];
static int call_time;                   /* 0 = delayed, 1 = instant */

/* Function to run the command, which we may wish to do either
 * as we get the char to return (query-replace), or when we get the
 * next one (incremental-search).
 */
static void activate_cmd(void) {
    if (next_cmd[0] == '\0') return;
    char *prev_execstr = execstr;   /* In case we are already running */
    execstr = next_cmd;
    int prev_inreex = inreex;
    inreex = FALSE;
    execproc(0, 0);                 /* It must be a user proc */
    execstr = prev_execstr;
    inreex = prev_inreex;
}

/* Get the "next character" from the debug buffer.
 * This is the first characer on the line.
 * The line from col3 onwards is a command to be excuted *before* the
 * next character is returned.
 */
static int idbg_nextchar(void) {
    if (call_time == 0) activate_cmd();

/* Now process the next buffer line....
 * NOTE: that we need to get the next Unicode character NOT a grapheme!
 */
    int retchar;
    if (idbg_line == idbg_head) return '\0';
    int used = utf8_to_unicode(idbg_line->l_text, 0, llength(idbg_line),
         (unicode_t*)&retchar);
    int cmdlen = llength(idbg_line); /* lines not NUL-terminated */
    cmdlen -= used + 1;
    if (cmdlen > 0) {
        strncpy(next_cmd, idbg_line->l_text + used +1, cmdlen);
        next_cmd[cmdlen] = '\0';
    }
    else {
        next_cmd[0] = '\0';
    }
    if (call_time == 1) activate_cmd();
    idbg_line = lforw(idbg_line);
    return retchar;
}

int incremental_debug_check(int type) {
    if (!(idbg_buf = bfind(IDB_BFR, FALSE, 0))) return FALSE;
    idbg_buf->b_flag &= ~BFCHG;         /* No prompt when deleting */

/* Ensure we start at the beginning of the buffer.
 * NOTE that we never actually swicvth to the buffer - we just get
 * the lines from it.
 */

    idbg_head = idbg_buf->b_linep;
    idbg_line = lforw(idbg_head);

/* Now set-up the function that gets the "next character" from it. */

    prev_idbg_func = term.t_getchar;
    term.t_getchar = idbg_nextchar;
    next_cmd[0] = '\0';
    call_time = type;

    return TRUE;
}

/* NOTE that cleanup DELETES THE BUFFER!!!!
 * This is to save the macro-caller having to do it, as it is
 * active if present.
 * Also, you can't remove "//..." buffers using delete-buffer.
 */

void incremental_debug_cleanup(void) {

    if (!idbg_buf) return;
    term.t_getchar = prev_idbg_func;
    zotbuf(idbg_buf);
    return;
}

/* Redisplay the given character at the given column, in reverse video.
 * The col gets set back to where it was, so no need to return anything.
 */
static void hilite(int c, int col) {
    col -= utf8char_width(c);   /* Set it back the correct amount */
/* Need force_movecursor as movecursor thinks we haven't moved */
    force_movecursor(term.t_nrow, col);
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
 * will abort the search.  Rubout will back up to the previous match of the
 * string, or if the starting point is reached first, it will delete the
 * last character from the search string.
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
    char pat_save[NPAT+1];  /* Saved copy of the old pattern str  */
    struct line *curline;   /* Current line on entry              */
    int curoff;             /* Current offset on entry            */
    int init_direction;     /* The initial search direction       */

/* Initialize starting conditions */

    cmd_reexecute = -1;     /* We're not re-executing (yet?)      */
    cmd_offset = 0;         /* Start at the beginning of the buff */
    cmd_buff[0] = '\0';     /* Init the command buffer            */
    strcpy(pat_save, pat);  /* Save the old pattern string        */
    curline = curwp->w.dotp; /* Save the current line pointer     */
    curoff = curwp->w.doto; /* Save the current offset            */
    init_direction = n;     /* Save the initial search direction  */

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
/* Yup, find the grapheme length and re-echo the string    */
        cpos = 0;
        int plen = strlen(pat);
        int final_char = '!';
        while (pat[cpos] != 0) {
            unicode_t uc;
            cpos += utf8_to_unicode(pat, cpos, plen, &uc);
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
        status = scanmore(pat, n, FALSE, FALSE);    /* Do the search */
        if (!status) hilite(final_char, col);
        c = get_char();             /* Get another character */
    }

/* Top of the per character loop */

    for (;;) {          /* ISearch per character loop */
                        /* Check for special characters first: */
                        /* Most cases here change the search */

        if (c == IS_QUIT) {     /* Want to quit searching?    */
            status = TRUE;
            goto end_isearch;   /* Quit searching now         */
        }
        switch (c) {            /* dispatch on the input char */
        case IS_ABORT:          /* If abort search request    */
            status = FALSE;
            goto end_isearch;   /* Quit searching again       */

        case IS_REVERSE:    /* If backward search         */
        case IS_FORWARD:    /* If forward search          */
            if (c == IS_REVERSE)    /* If reverse search  */
                n = -1;             /* Set the reverse direction  */
            else                    /* Otherwise,         */
                n = 1;              /*  go forward         */
/* This calls asks for the *next* match, not a continuation of the
 * current one.
 */
            status = scanmore(pat, n, TRUE, FALSE); /* Start again   */
            if (!status) hilite('!', col+1);  /* No further match */
            c = get_char(); /* Get next char */
            continue;       /* Go continue with the search */

        case IS_NEWLINE:    /* Carriage return            */
            c = '\n';       /* Make it a new line         */
            break;          /* Make sure we use it        */

        case IS_QUOTE:      /* Quote character            */
            c = get_char(); /* Get the next char */

        case IS_TAB:        /* Generically allowed        */
        case '\n':          /*  controlled characters     */
            break;          /* Make sure we use it        */

        case IS_BACKSP:     /* If a backspace:      */
        case IS_RUBOUT:     /*  or if a Rubout:     */
            if (cmd_offset <= 1) {  /* Anything to delete?  */
                status = TRUE;      /* No, just exit        */
                goto end_isearch;
            }
            --cmd_offset;           /* Back up over Rubout  */
            cmd_buff[--cmd_offset] = '\0';  /* Yes, delete last char */
            curwp->w.dotp = curline;        /* Reset the line pointer */
            curwp->w.doto = curoff; /*  and the offset       */
            n = init_direction;     /* Reset search direction */
            strcpy(pat, pat_save);  /* Restore old search str */
            cmd_reexecute = 0;      /* Start the whole mess over  */
            goto start_over;        /* Let it take care of itself */

/* Presumably a quasi-normal character comes here */

        default:                /* All other chars        */
            if (c < ' ') {      /* Is it printable? Nope. */
                reeat(c);       /* Re-eat the char        */
                status = TRUE;  /* Return the previous status */
                goto end_isearch;
            }
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
        char *ucbp = ucb;
        int togo = nb;
        while (togo--) {
            pat[cpos++] = *(ucbp++);/* put the char in the buffer */
            if (cpos >= NPAT) {     /* too many chars in string?  */
                                    /* Yup.  Complain about it    */
                mlwrite_one("? Search string too long");
                status = TRUE;       /* Return an error            */
                goto end_isearch;
            }
        }
        pat[cpos] = 0;              /* null terminate the buffer  */
        col = echo_char(c, col);    /* Echo the character         */
        if (!status) {              /* If we lost last time       */
            TTputc(BELL);           /* Feep again                 */
            TTflush();              /* see that the feep feeps    */
        }
        else                        /* Otherwise, we must have won */
             status = scanmore(pat, n, FALSE, TRUE);    /* find next match */
        if (!status) hilite(c, col);
        c = get_char();             /* Get the next char        */
    }                               /* for {;;} */
end_isearch:
    (void)scanmore(NULL, 0, 0, 0);  /* Invalidate group matches */
    if (using_incremental_debug) incremental_debug_cleanup();
    return status;
}

/* Incremental search entry - forward direction
 */
int fisearch(int f, int n) {
    struct line *curline;           /* Current line on entry    */
    int curoff;                     /* Current offset on entry  */

/* Remember the initial . on entry: */

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
    srch_patlen = strlen(pat);
    return TRUE;
}
/* Reverse direction uses same code, just reverses direction */
int risearch(int f, int n) {        /* Same as fisearch in reverse */
    return fisearch(f, -n);
}
