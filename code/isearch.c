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
    char tpat[NPAT + 20];

/* check to see whether we are executing a command line */

    if (clexec) return 0;

    strcpy(tpat, prompt);       /* copy prompt to output string */
    strcat(tpat, MLbkt("<Meta>"));
    strcat(tpat, " ");

    mlwrite_one(tpat);

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
    cmd_buff[cmd_offset] = '\0';    /* And terminate the buffer    */
    return c;               /* Return the character                */
}

/* Functions to check and set-up incremental debug mode.
 * This is *only* intended to be used for running functional tests
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
 * This is the next character from the first token on the line.
 * The rest of the line is user-proc command to be executed *after*
 * the last character in the string is enacted.
 */
static char dbg_chars[128];
static int dbg_chars_i = -1;

static int idbg_nextchar(void) {

/* Do we have any pending chars to return? */

    if (dbg_chars_i < 0) return UEM_NOCHAR;     /* Why are we here? */

    if (dbg_chars_i > 0) {                      /* Valid buffer chars */
        if (dbg_chars[dbg_chars_i] != '\0') {   /* Not run out of them */
            return dbg_chars[dbg_chars_i++];
        }
    }

    if (idbg_line == idbg_head) return UEM_NOCHAR;  /* No more buffer */

/* We want to get the first token.
 * However, token expects NUL-terminated strings.
 * So, ensure that this line buffer info has a NUL at the end.
 * The allocation and manipulation code in line.c ensures there is always
 * at least one "extra", "free" character at the end of l_text.
 */
    idbg_line->l_text[idbg_line->l_used] = '\0';

/* Do we have a delayed command to run? */

    if (call_time == 0) activate_cmd();

/* NOTE! that the use of token() overwrites the buffer data with NULs
 * but that is OK, as we destroy the buffer at the end and we never
 * re-visit any of the text.
 */
    char ntok[128];
    char *dlp = idbg_line->l_text;
    dlp = token(idbg_line->l_text, ntok, 128);

    int nch = strlen(ntok);
    int dbc = 0;
    int offs = 0;
    while (nch > offs) {
        int tc;
        int used = utf8_to_unicode(ntok, offs, nch, (unicode_t*)&tc);
        dbg_chars[dbc++] = tc;
        offs += used;
     }
    dbg_chars[dbc] = '\0';

/* Any rest of line is the command to run, but skip spaces before
 * checking that...
 */
    while(*dlp == ' ') {dlp++; idbg_line->l_used--;}
    strcpy(next_cmd, dlp);

    if (call_time == 1) activate_cmd();

/* Get to next line, if there is one - skip zero-length ones... */

    do {
        idbg_line = lforw(idbg_line);
        if (idbg_line == idbg_head) break;  /* No more buffer */
    } while (llength(idbg_line) == 0);

/* Note that dbg_chars_i refers to teh line we have just parsed, *NOT*
 * what idbg_line now points at!.
 */
    dbg_chars_i = 1;    /* Must be one, as we skip zero-length lines */
    return dbg_chars[0];
}

int incremental_debug_check(int type) {
    if (!(idbg_buf = bfind(IDB_BFR, FALSE, 0))) return FALSE;
    idbg_buf->b_flag &= ~BFCHG;         /* No prompt when deleting */

/* Ensure we start at the beginning of the buffer.
 * NOTE that we never actually switch to the buffer - we just get
 * the lines from it.
 */
    idbg_head = idbg_buf->b_linep;
    idbg_line = lforw(idbg_head);

/* Now set-up the function that gets the "next character" from it. */

    prev_idbg_func = term.t_getchar;
    term.t_getchar = idbg_nextchar;
    next_cmd[0] = '\0';
    dbg_chars_i = 0;    /* Where to start */
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
    dbg_chars_i = -1;
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
/* Yup, find the grapheme length and re-echo the string. */
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
            status = scanmore(pat, n, TRUE, FALSE); /* Start again   */
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
            cmd_buff[--cmd_offset] = '\0';  /* Yes, delete last char */
            curwp->w.dotp = curline;        /* Reset the line pointer */
            curwp->w.doto = curoff; /*  and the offset       */
            n = init_direction;     /* Reset search direction */
            strcpy(pat, pat_save);  /* Restore old search str */
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
        char *ucbp = ucb;
        int togo = nb;
        while (togo--) {
            pat[cpos++] = *(ucbp++);/* put the char in the buffer */
            if (cpos >= NPAT) {     /* too many chars in string?  */
                                    /* Yup.  Complain about it    */
                if (clexec == FALSE) mlwrite_one("? Search string too long");
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
/* Exit if we run out of input.... */
        if (c == UEM_NOCHAR) break;
    }
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
