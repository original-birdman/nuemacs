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

#define IS_ABORT        0x07    /* Abort the isearch */
#define IS_BACKSP       0x08    /* Delete previous char */
#define IS_TAB          0x09    /* Tab character (allowed search char) */
#define IS_NEWLINE      0x0D    /* New line from keyboard (Carriage return) */
#define IS_QUOTE        0x11    /* Quote next character */
#define IS_REVERSE      0x12    /* Search backward */
#define IS_FORWARD      0x13    /* Search forward */
#define IS_VMSQUOTE     0x16    /* VMS quote character */
#define IS_VMSFORW      0x18    /* Search forward for VMS */
#define IS_QUIT         0x1B    /* Exit the search */
#define IS_RUBOUT       0x7F    /* Delete previous character */

static int echo_char(int c, int col);

/* A couple of "own" variables for re-eat */

static int (*saved_get_char) (void);    /* Get character routine */
static int eaten_char = -1;             /* Re-eaten char */

/* A couple more "own" variables for the command string */

static int cmd_buff[CMDBUFLEN]; /* Save the command args here */
static int cmd_offset;                  /* Current offset into command buff */
static int cmd_reexecute = -1;          /* > 0 if re-executing command */


/* Subroutine to do incremental reverse search.  It actually uses the
 * same code as the normal incremental search, as both can go both ways.
 */
int risearch(int f, int n) {
    struct line *curline;       /* Current line on entry        */
    int curoff;                 /* Current offset on entry      */

/* Remember the initial . on entry: */

    curline = curwp->w.dotp;    /* Save the current line pointer  */
    curoff = curwp->w.doto;     /* Save the current offset        */

/* Make sure the search doesn't match where we already are:   */

    back_grapheme(1);           /* Back up a character            */

    if (!(isearch(f, -n))) {        /* Call ISearch backwards */
                                    /* If error in search:    */
        curwp->w.dotp = curline;    /* Reset line pointer and */
        curwp->w.doto = curoff;     /* offset to orig value   */
        curwp->w_flag |= WFMOVE;    /* Say we've moved        */
        update(FALSE);              /* And force an update    */
        mlwrite_one(MLbkt("search failed")); /* Say we died */
    } else
        mlerase();      /* If happy, just erase the cmd line  */
    srch_patlen = strlen(pat);
    return TRUE;
}

/* Again, but for the forward direction
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

/* The following is a worker subroutine used by the reverse search.  It
 * compares the pattern string with the characters at "." for equality. If
 * any characters mismatch, it will return FALSE.
 *
 * This isn't used for forward searches, because forward searches leave "."
 * at the end of the search string (instead of in front), so all that needs to
 * be done is match the last char input.
 *
 * char *patrn;                 String to match to buffer
 */
static int match_pat(char *patrn) {
/* See if the pattern string matches string at "."   */
    int buffchar;           /* character at current position      */
    struct line *curline;   /* current line during scan           */
    int curoff;             /* position within current line       */

/* Setup the local scan pointer to current "." */

    curline = curwp->w.dotp;    /* Get the current line structure  */
    curoff = curwp->w.doto;     /* Get the offset within that line */

/* Top of per character compare loop: */

/* Loop for all characters in patrn   */
    for (unsigned int i = 0; i < strlen(patrn); i++) {
        if (curoff == llength(curline)) {   /* If at end of line */
            curline = lforw(curline);       /* Skip to the next line */
            curoff = 0;         /* Start at the beginning of the line */
            if (curline == curbp->b_linep)
                return FALSE;   /* Abort if at end of buffer */
            buffchar = '\n';    /* And say the next char is NL */
        } else
            buffchar = lgetc(curline, curoff++);    /* Get next char */
        if (!eq(buffchar, patrn[i]))    /* Is it what we're looking for? */
            return FALSE;               /* Nope, just punt it then       */
    }
    return TRUE;            /* Everything matched? Let's celebrate */
}

/* Trivial routine to insure that the next character in the search string is
 * still true to whatever we're pointing to in the buffer.  This routine will
 * not attempt to move the "point" if the match fails, although it will
 * implicitly move the "point" if we're forward searching, and find a match,
 * since that's the way forward isearch works.
 *
 * If the compare fails, we return FALSE and assume the caller will call
 * scanmore or something.
 *
 * int chr;             Next unicode char to look for
 * char *patrn;         The entire search string (incl chr)
 * int dir;             Search direction
 */
static int checknext(int chr, char *patrn, int dir) {
/* Check next character in search string */
    struct line *curline;   /* current line during scan           */
    int curoff;             /* position within current line       */
    unicode_t buffchar;     /* character at current position      */
    int status;             /* how well things go                 */
    int nb;

/* Setup the local scan pointer to current "." */

    curline = curwp->w.dotp;    /* Get the current line structure     */
    curoff = curwp->w.doto;     /* Get the offset within that line    */

    if (dir > 0) {              /* If searching forward               */
        if (curoff == llength(curline)) {   /* If at end of line  */
            curline = lforw(curline);       /* Skip to the next line */
            if (curline == curbp->b_linep)
                return FALSE;           /* Abort if end of buffer */
            curoff = 0; /* Start at the beginning of the line  */
            buffchar = '\n';            /* And say the next char is NL */
            nb = 1;
        }
        else {                  /* Allow for Unicode */
            nb = utf8_to_unicode(curline->l_text, curoff, curline->l_used,
                 &buffchar);
            curoff += nb;
        }
/* Is it what we're looking for?
 * If yes, set the buffer's point to the matched character and say
 * that we've moved
 */
        if (nb == 1)
            status = eq(buffchar, chr);
        else
            status = unicode_eq(buffchar, chr);
        if (status != 0) {
            curwp->w.dotp = curline;
            curwp->w.doto = curoff;
            curwp->w_flag |= WFMOVE;
        }
        return status;              /* And return the status       */
    }
    else                            /* Else, if reverse search:    */
        return match_pat(patrn);    /* See if we're in right place */
}

/* Routine to prompt for I-Search string.
 */
static int promptpattern(char *prompt) {
    char tpat[NPAT + 20];

    strcpy(tpat, prompt);       /* copy prompt to output string */
    strcat(tpat, " " MLpre);    /* build new prompt string */
    expandp(pat, &tpat[strlen(tpat)], NPAT / 2);    /* add old pattern */
    strcat(tpat, MLpost "<Meta>: ");

/* check to see if we are executing a command line */
    if (!clexec) mlwrite_one(tpat);

/* This now needs the grapheme length of the byte array... */

    int glen = 0;
    int tplen = strlen(tpat);
    int tpi = 0;
    while (tpi < tplen) {
        struct grapheme dummy_gc;
        tpi = build_next_grapheme(tpat, tpi, tplen, &dummy_gc);
        if (dummy_gc.ex) Xfree(dummy_gc.ex);
        glen++;
    }
    return glen;
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

/* Subroutine to do an incremental search.  In general, this works similarly
 * to the older micro-emacs search function, except that the search happens
 * as each character is typed, with the screen and cursor updated with each
 * new search character.
 *
 * While searching forward, each successive character will leave the cursor
 * at the end of the entire matched string.  Typing a Control-S or Control-X
 * will cause the next occurrence of the string to be searched for (where the
 * next occurrence does NOT overlap the current occurrence).  A Control-R will
 * change to a backwards search, META will terminate the search and Control-G
 * will abort the search.  Rubout will back up to the previous match of the
 * string, or if the starting point is reached first, it will delete the
 * last character from the search string.
 *
 * While searching backward, each successive character will leave the cursor
 * at the beginning of the matched string.  Typing a Control-R will search
 * backward for the next occurrence of the string.  Control-S or Control-X
 * will revert the search to the forward direction.  In general, the reverse
 * incremental search is just like the forward incremental search inverted.
 *
 * In all cases, if the search fails, the user will be feeped, and the search
 * will stall until the pattern string is edited back into something that
 * exists (or until the search is aborted).
 */

int isearch(int f, int n) {
    UNUSED(f);
    int status;             /* Search status */
    int col;                /* prompt column */
    int cpos;       /* character number in search string  */
    int c;          /* current input character */
    int expc;       /* function expanded input char       */
/* GGR - Allow for a trailing NUL */
    char pat_save[NPAT+1];  /* Saved copy of the old pattern str  */
    struct line *curline;   /* Current line on entry              */
    int curoff;             /* Current offset on entry            */
    int init_direction;     /* The initial search direction       */

/* Initialize starting conditions */

    cmd_reexecute = -1;     /* We're not re-executing (yet?)      */
    cmd_offset = 0;         /* Start at the beginning of the buff */
    cmd_buff[0] = '\0';     /* Init the command buffer            */
    strncpy(pat_save, pat, NPAT);   /* Save the old pattern string   */
    curline = curwp->w.dotp;        /* Save the current line pointer */
    curoff = curwp->w.doto; /* Save the current offset            */
    init_direction = n;     /* Save the initial search direction  */

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
    c = ectoc(expc = get_char());   /* Get the first character    */
    if ((c == IS_FORWARD) || (c == IS_REVERSE)) {
/* Reuse old search string?   */
/* Yup, find the grapheme length and re-echo the string    */
        cpos = 0;
        int plen = strlen(pat);
        while (pat[cpos] != 0) {
            unicode_t uc;
            cpos += utf8_to_unicode(pat, cpos, plen, &uc);
            col = echo_char(uc, col);
	}

        if (c == IS_REVERSE) {      /* forward search?        */
            n = -1;             /* No, search in reverse  */
            back_grapheme(1);   /* Be defensive about EOB */
        }
        else
            n = 1;              /* Yes, search forward    */
        status = scanmore(pat, n);      /* Do the search         */
        c = ectoc(expc = get_char());   /* Get another character */
    }

/* Top of the per character loop */

    for (;;) {          /* ISearch per character loop */
                        /* Check for special characters first: */
                        /* Most cases here change the search */

        if (expc == metac)  /* Want to quit searching?    */
            return TRUE;    /* Quit searching now         */

        switch (c) {        /* dispatch on the input char */
        case IS_ABORT:      /* If abort search request    */
            return FALSE;   /* Quit searching again       */

        case IS_REVERSE:    /* If backward search         */
        case IS_FORWARD:    /* If forward search          */
            if (c == IS_REVERSE)    /* If reverse search  */
                n = -1;             /* Set the reverse direction  */
            else                    /* Otherwise,         */
                n = 1;             /*  go forward         */
            status = scanmore(pat, n);      /* Start again   */
            c = ectoc(expc = get_char());   /* Get next char */
            continue;       /* Go continue with the search */

        case IS_NEWLINE:    /* Carriage return            */
            c = '\n';       /* Make it a new line         */
            break;          /* Make sure we use it        */

        case IS_QUOTE:      /* Quote character            */
            c = ectoc(expc = get_char());   /* Get the next char */

        case IS_TAB:        /* Generically allowed        */
        case '\n':          /*  controlled characters     */
            break;          /* Make sure we use it        */

        case IS_BACKSP:     /* If a backspace:      */
        case IS_RUBOUT:     /*  or if a Rubout:     */
            if (cmd_offset <= 1)    /* Anything to delete?  */
                return TRUE;        /* No, just exit        */
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
                return TRUE;    /* And return the last status */
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
                return TRUE;        /* Return an error            */
            }
        }
        pat[cpos] = 0;              /* null terminate the buffer  */
        col = echo_char(c, col);    /* Echo the character         */
        if (!status) {              /* If we lost last time       */
            TTputc(BELL);           /* Feep again                 */
            TTflush();              /* see that the feep feeps    */
        }
        else                        /* Otherwise, we must have won */
            if (!(status = checknext(c, pat, n)))   /* See if match  */
             status = scanmore(pat, n); /* or find the next match */
        c = ectoc(expc = get_char());   /* Get the next char        */
    }                               /* for {;;} */
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
