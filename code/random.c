/*      random.c
 *
 *      This file contains the command processing functions for a number of
 *      random commands. There is no functional grouping here, for sure.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "charset.h"
#include "utf8proc.h"

static int tabsize; /* Tab size (0: use real tabs) */

/* Set fill column to n.
 */
int setfillcol(int f, int n) {
/* GGR - default to 72 on no arg or an invalid one */
    if (f && (n > 0)) fillcol = n;
    else              fillcol = 72;

    mlwrite(MLbkt("Fill column is %d"), fillcol);
    return TRUE;
}

/* Display the current position of the cursor, in origin 1 X-Y coordinates,
 * the character that is under the cursor (in hex), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 * Normally this is bound to "C-X =".
 */
int showcpos(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *lp;        /* current line */
    ue64I_t numchars;       /* # of chars in file */
    ue64I_t predchars;      /* # chars preceding point */
    int numlines;           /* # of lines in file */
    int predlines;          /* # lines preceding point */
    unicode_t curchar;      /* char under cursor (GGR unicode) */
    int ratio;
    int col;
    int savepos;            /* temp save for current offset */
    int ecol;               /* column pos/end of current line */
    int bytes_used = 1;     /* ...by the current grapheme */
    int uc_used = 1;        /* unicode characters in grapheme */

/* Starting at the beginning of the buffer */
    lp = lforw(curbp->b_linep);

/* Start counting chars and lines */
    numchars = 0;
    numlines = 0;
    predchars = 0;
    predlines = 0;
    curchar = 0;
    while (lp != curbp->b_linep) {
/* If we are on the current line, record it */
        if (lp == curwp->w.dotp) {
            predlines = numlines;
            predchars = numchars + curwp->w.doto;
            if (((size_t)curwp->w.doto) == lused(lp)) curchar = '\n';
            else {
                struct grapheme glyi;   /* Full data */
                bytes_used = lgetgrapheme(&glyi, FALSE);
                curchar = glyi.uc;
                if (glyi.cdm != 0) uc_used = 2;
                if (glyi.ex != NULL) {
                    for (unicode_t *xc = glyi.ex; *xc != UEM_NOCHAR; xc++)
                        uc_used++;
                    Xfree(glyi.ex);
                }
            }
        }
/* On to the next line */
        ++numlines;
        numchars += lused(lp) + 1;
        lp = lforw(lp);
    }

/* If at end of file, record it */
    if (curwp->w.dotp == curbp->b_linep) {
        predlines = numlines;
        predchars = numchars;
        curchar = UEM_NOCHAR;   /* NoChar */
    }

/* Get real column and end-of-line column. */
    col = getccol() + 1;        /* Internal 0-based, external 1-based */
    savepos = curwp->w.doto;
    curwp->w.doto = lused(curwp->w.dotp);
    ecol = getccol() + 1;       /* Internal 0-based, external 1-based */
    curwp->w.doto = savepos;

/* If we are in DOS mode we need to add an extra char for the \r
 * on each preceding line.
 */
    if (curbp->b_mode & MDDOSLE) {
        predchars += predlines;
        numchars += numlines;
    }

    ratio = 0;              /* Ratio before dot. */
    if (numchars != 0) ratio = (100L * predchars)/numchars;

/* summarize and report the info.
 * NOTE that this reports the current base character *only*, but the byte
 * count is for the full grapheme.
 */
    char descr[40];
    if (curchar == UEM_NOCHAR) strcpy(descr, "at end of buffer");
    else {
        char temp[8];
        if (curchar > 127) {        /* utf8!? */
            int blen = unicode_to_utf8(display_for(curchar), temp);
            terminate_str(temp + blen);
        }
        else if (curchar <= ' ') {  /* non-printing - lookup descr */
                strcpy(temp, charset[curchar]);
            }
            else {                      /* printable ASCII */
                temp[0] = curchar;
                terminate_str(temp + 1);
            }
        snprintf(descr, 40 ,"char = U+%04x (%s)", curchar, temp);
    }
#define DLEN 32
    char gl_descr[DLEN] = "";   /* Longer than actually needed... */
    if (bytes_used > 1)
          snprintf(gl_descr, DLEN, " %db/%duc", bytes_used, uc_used);
    mlwrite( "Line %d/%d Col %d/%d(%d) Byte %D/%D (%d%%) %s%s",
          predlines+1, numlines+1, col, ecol, savepos,
          predchars, numchars, ratio, descr, gl_descr);
    return TRUE;
}

/* Get the current line number */
int getcline(void) {
    struct line *lp;        /* current line */
    int numlines;           /* # of lines before point */

/* Starting at the beginning of the buffer */
    lp = lforw(curbp->b_linep);

/* Start counting lines */
    numlines = 0;
    while (lp != curbp->b_linep) {
/* If we are on the current line, record it */
        if (lp == curwp->w.dotp) break;
        ++numlines;
        lp = lforw(lp);
    }

/* And return the resulting count */
    return numlines + 1;
}

/* Return current column.
 * Column counting needs to take into account zero-widths, and also the
 * 2/3-column displays for control characters done by vtputc() so uses
 * utf8_to_unicode()+update_screenpos_for_char rather than next_utf8_offset().
 */
int getccol(void) {
    int i, col;
    struct line *dlp = curwp->w.dotp;
    int byte_offset = curwp->w.doto;
    int len = lused(dlp);

    col = i = 0;
    while (i < byte_offset) {
        unicode_t c;
        i += utf8_to_unicode(ltext(dlp), i, len, &c);
        update_screenpos_for_char(col, c);
    }
    return col;
}

/* Set current column.
 * Column counting needs to take into account zero-widths, and also the
 * 2/3-column displays for control characters done by vtputc().
 *
 * int pos;             position to set cursor
 */
int setccol(int pos) {
    int i;          /* index into current line */
    int col;        /* current cursor column   */
    struct line *dlp = curwp->w.dotp;

    col = i = 0;
    int len = lused(dlp);

/* Scan the line until we are at or past the target column */
    while (i < len) {
        if (col >= pos) break;  /* Upon reaching the target, drop out */
        unicode_t c;
        i += utf8_to_unicode(ltext(dlp), i, len, &c);
        update_screenpos_for_char(col, c);
    }
    curwp->w.doto = i;          /* Set us at the new position... */
    return col >= pos;          /* ..and tell whether we made it */
}

/* Twiddle the two characters on either side of dot. If dot is at the end of
 * the line twiddle the two characters before it. Return with an error if dot
 * is at the beginning of line; it seems to be a bit pointless to make this
 * work. This fixes up a very common typo with a single stroke. Normally bound
 * to "C-T". This always works within a line, so "WFEDIT" is good enough.
 */
int twiddle(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *dotp;
    int doto;
    int rch_st, rch_nb, lch_st, lch_nb;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */

    dotp = curwp->w.dotp;
    int maxlen = lused(dotp);
    const char *l_buf = ltext(dotp);

    while (n-- > 0) {
        int reset_col = 0;
        doto = curwp->w.doto;
/* GGR
 * twiddle here (and, e.g.,  bash) seems to act on the chars before and
 * after point -  except when you are at the end of a line....
 * This inconsistency seems odd. Prime emacs always acted on the two chars
 * preceding point, which was great for fixing typos as you made them.
 * If the ggr_opts GGR_TWIDDLE is set then twiddle will act on the
 * two preceding chars.
 *
 * So where the right-hand character is depends on the mode and if you're
 * not in GGR style then it depends on whether you are at the end-of-line
 * too (doto == maxlen, so you then use ggr-style!!).
 * But once we know where the right-hand character is the left-hand one
 * is always the one preceding it.
 */
        if ((ggr_opts&GGR_TWIDDLE) || doto == maxlen) {
            rch_st = prev_utf8_offset(l_buf, doto, TRUE);
            if (rch_st < 0) return (FALSE);
            rch_nb = doto - rch_st;
        }
        else {  /* Need to get back to where we are in this mode...*/
            reset_col = 1;
            rch_st = doto;
            rch_nb = next_utf8_offset(l_buf, rch_st, maxlen, TRUE) - rch_st;
        }
        lch_st = prev_utf8_offset(l_buf, rch_st, TRUE);
        if (lch_st < 0) return (FALSE);
        lch_nb = rch_st - lch_st;

/* We know where the two characters start, and how many bytes each has.
 * So we copy them into a buffer in the reverse order and then 
 * overwrite the original string with this new one.
 * If we are twiddling *around* point (i.e. not GGR_TWIDDLE mode and not
 * at eol) we might now be in the "middle" of a character, so we move to
 * the end of the pair (meaning we advance as we twiddle).
 */
        db_setn(glb_db, l_buf+rch_st, rch_nb);
        db_appendn(glb_db, l_buf+lch_st, lch_nb);
        db_overwriten_at(ldb(dotp), db_val(glb_db), db_len(glb_db), lch_st);

        if (reset_col) curwp->w.doto = lch_st + lch_nb + rch_nb;
    }
    lchange(WFEDIT);
    return TRUE;
}

/* Quote the next character, and insert it into the buffer. All the characters
 * are taken literally, with the exception of the newline, which always has
 * its line splitting meaning. The character is always read, even if it is
 * inserted 0 times, for regularity. Bound to "C-Q"
 */
static int last_qchar = 0;
int quote(int f, int n) {
    UNUSED(f);
    int s;
    int c;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (inreex && RXARG(quote))     /* Use previously-obtained char? */
        c = last_qchar;
    else {
        c = tgetc();
        last_qchar = c;
    }
    if (n < 0) return FALSE;
    if (n == 0) return TRUE;
    if (c == '\n') {
        do {
            s = lnewline();
        } while (s == TRUE && --n);
        return s;
    }
    if (!inmb && kbdmode == RECORD) addchar_kbdmacro(c);
    return linsert_uc(n, c);
}

/* GGR version of tab */
int typetab(int f, int n) {
    UNUSED(f);

    if (n < 0) return (FALSE);
    if (n == 0 || n > 1) {
        tabsize = n;
        return(TRUE);
    }

    if (! tabsize) return(linsert_byte(1, '\t'));

/* We have to work with columns, not byte-counts */

    int ccol = getccol();
    int newcol = ccol + tabsize - (getccol() % tabsize);

/* Try to get to the target column and check for success */

    if (setccol(newcol)) return TRUE;

/* We fail if the line is too short, so then we pad with spaces */

    ccol = getccol();
    return linsert_byte((newcol - ccol), ' ');
}

/* Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces. This has to be
 * done in this slightly funny way because the tab (in ASCII) has been turned
 * into "C-I" (in 10 bit code) already. Bound to "C-I".
 */
int insert_tab(int f, int n) {
    UNUSED(f);
    if (n < 0) return FALSE;
    if (n == 0 || n > 1) {
        tabsize = n;
        return TRUE;
    }
    if (!tabsize) return linsert_byte(1, '\t');
    return linsert_byte(tabsize - (getccol() % tabsize), ' ');
}

/* change tabs to spaces
 *
 * int f, n;            default flag and numeric repeat count
 */
int detab(int f, int n) {
    int inc;        /* increment to next line [sgn(n)] */

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */

    if (f == FALSE) n = 1;

/* Loop thru detabbing n lines */
    inc = ((n > 0) ? 1 : -1);
    while (n) {
        curwp->w.doto = 0;      /* start at the beginning */

/* Detab the entire current line */
        while ((size_t)curwp->w.doto < lused(curwp->w.dotp)) {
/* If we have a tab */
            if (lgetc(curwp->w.dotp, curwp->w.doto) == '\t') {
                ldelgrapheme(1, FALSE);
                insspace(TRUE, (tabmask + 1) - (curwp->w.doto & tabmask));
            }
            forw_grapheme(1);
        }
/* Advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    curwp->w.doto = 0;      /* to the begining of the line */
    lchange(WFEDIT);        /* yes, we have made at least an edit */
    return TRUE;
}

/* change spaces to tabs where possible
 *
 * int f, n;            default flag and numeric repeat count
 */
int entab(int f, int n) {
    int inc;        /* increment to next line [sgn(n)] */
    int fspace;     /* pointer to first space if in a run */
    int ccol;       /* current cursor column */
    char cchar;     /* current character */

    if (curbp->b_mode & MDVIEW) /* Don't allow this command if */
          return rdonly();      /* we are in read only mode    */

    if (f == FALSE) n = 1;

/* Loop thru entabbing n lines */
    inc = ((n > 0) ? 1 : -1);
    while (n) {
        curwp->w.doto = 0;      /* Start at the beginning */

/* Entab the entire current line */
        fspace = -1;
        ccol = 0;
        while ((size_t)curwp->w.doto < lused(curwp->w.dotp)) {
/* See if it is time to compress */
            if ((fspace >= 0) && (nextab(fspace) <= ccol)) {
                if (ccol - fspace < 2) fspace = -1;
                else {
/* There is a bug here dealing with mixed space/tabbed lines.......
 * it will get fixed.  (when?)
 */
                    back_grapheme(ccol - fspace);
                    ldelete((ue64I_t) (ccol - fspace), FALSE);
                    linsert_byte(1, '\t');
                    fspace = -1;
                }
            }

/* Get the current character */
            cchar = lgetc(curwp->w.dotp, curwp->w.doto);

            switch (cchar) {
            case '\t':      /* a tab...count em up */
                ccol = nextab(ccol);
                break;

            case ' ':       /* a space...compress? */
                if (fspace == -1) fspace = ccol;
                ccol++;
                break;

            default:        /* any other char...just count */
                ccol++;
                fspace = -1;
                break;
            }
            forw_grapheme(1);
        }

/* Advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    curwp->w.doto = 0;      /* to the begining of the line */
    lchange(WFEDIT);        /* yes, we have made at least an edit */
    return TRUE;
}

/* trim trailing whitespace from the point to eol
 *
 * int f, n;            default flag and numeric repeat count
 */
int trim(int f, int n) {
    struct line *lp;        /* current line pointer */
    int offset;             /* original line offset position */
    int length;             /* current length */
    int inc;                /* increment to next line [sgn(n)] */

    if (curbp->b_mode & MDVIEW) /* don't allow this command if */
          return rdonly();      /* we are in read only mode    */

    if (f == FALSE) n = 1;

/* Loop thru trimming n lines */
    inc = ((n > 0) ? 1 : -1);
    while (n) {
        lp = curwp->w.dotp;     /* find current line text */
        offset = curwp->w.doto; /* save original offset */
        length = lused(lp);     /* find current length */

/* Trim the current line */
        while (length > offset) {
            switch(lgetc(lp, length - 1)) {
                case ' ':
                case '\t':
                    length--;
                    continue;   /* Keep going... */
                default:
                    break;
            }
            break;
        }
        db_truncate(ldb(lp), length);

/* Advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    lchange(WFEDIT);
    return TRUE;
}

/* Open up some blank space. The basic plan is to insert a bunch of newlines,
 * and then back up over them. Everything is done by the subcommand
 * processors. They even handle the looping. Normally this is bound to "C-O".
 */
int openline(int f, int n) {
    UNUSED(f);
    int i;
    int s;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return FALSE;
    if (n == 0) return TRUE;
    i = n;                          /* Insert newlines.     */
    do {
        s = lnewline();
    } while (s == TRUE && --i);
    if (s == TRUE)                      /* Then back up overtop */
          s = (back_grapheme(n) > 0);   /* of them all.         */
    return s;
}

/* Insert a newline and indentation for C */
static int cinsert(void) {
    const char *cptr;   /* string pointer into text to copy */
    int tptr;           /* index to scan into line */
    int bracef;         /* was there a brace at the end of line? */
    int i;

/* GGR fix - nothing fancy if we're at left hand edge */
    if (curwp->w.doto == 0) return(lnewline());

/* Grab a pointer to text to copy indentation from */
    cptr = ltext(curwp->w.dotp);

/* Check for a brace - check for last non-blank character! */
    tptr = curwp->w.doto;
    bracef = 0;
    i = tptr;
/* Using C-mode for Perl is also nice. But we need to allow for
 * things such as "@{$data{$dev}[$item]} = @attr;"
 * So count all { and } and set bracef if there are more of the former.
 */
    while (--i >= 0) {
        if (cptr[i] == ' ' || cptr[i] == '\t') continue;
        if      (cptr[i] == '{') bracef++;
        else if (cptr[i] == '}') bracef--;
    }
    bracef = (bracef > 0);

/* Save the indent of the previous line */
    i = 0;
    db_strdef(ichar);  /* buffer to hold indent of last line */
    while ((i < tptr) && (cptr[i] == ' ' || cptr[i] == '\t')) {
        db_addch(ichar, cptr[i]);
        ++i;
    }

/* Put in the newline */
    if (lnewline() == FALSE) return FALSE;

/* And the saved indentation */
    linstr(db_val(ichar));
    db_free(ichar);

/* And one more tab for a brace */
    if (bracef) insert_tab(FALSE, 1);

    curwp->w_flag |= WFINS;
    return TRUE;
}

/* Insert a newline. Bound to "C-M". If we are in CMODE, do automatic
 * indentation as specified.
 */
int insert_newline(int f, int n) {
    UNUSED(f);
    int s;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return FALSE;

/* If we are in C mode and this is a default <NL> */
    if (n == 1 && (curbp->b_mode & MDCMOD) && curwp->w.dotp != curbp->b_linep)
          return cinsert();

/* If a newline was typed, fill column is defined, the argument is non-
 * negative, wrap mode is enabled, and we are now past fill column,
 * and we are not read-only, perform word wrap.
 */
    if ((curwp->w_bufp->b_mode & MDWRAP) && fillcol > 0 &&
        getccol() > fillcol &&
        (curwp->w_bufp->b_mode & MDVIEW) == FALSE) {
/* Don't start the handler when it is already running as that might
 * just get into a loop...
 */
        if (!meta_spec_active.W) {
            meta_spec_active.W = 1;
            execute(META|SPEC|'W', FALSE, (ggr_opts&GGR_FULLWRAP)? 2: 1);
            meta_spec_active.W = 0;
/* If the result of the wrap is that we are at the start of a line then
 * we don't want to add a space.
 * This has to match the handling in execute() in main.c.
 */
            if (curwp->w.doto == 0) return TRUE;
        }
    }

/* insert some lines */
    while (n--) {
        if ((s = lnewline()) != TRUE) return s;
        curwp->w_flag |= WFINS;
    }
    return TRUE;
}

/* insert a # into the text here...we are in CMODE */
int inspound(void) {
    char ch;    /* last character before input */
    int i;

/* If we are at the beginning of the line, no go */
    if (curwp->w.doto == 0) return linsert_byte(1, '#');

/* Scan to see if all space before this is white space */
    for (i = curwp->w.doto - 1; i >= 0; --i) {
        ch = lgetc(curwp->w.dotp, i);
        if (ch != ' ' && ch != '\t') return linsert_byte(1, '#');
    }

/* Delete back first */
    while (getccol() >= 1) backdel(FALSE, 1);

/* And insert the required pound */
    return linsert_byte(1, '#');
}

/* Delete blank lines around dot. What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a blank line, this command
 * deletes all the blank lines above and below the current line. If it is
 * sitting on a non blank line then it deletes all of the blank lines after
 * the line. Normally this command is bound to "C-X C-O". Any argument is
 * ignored.
 */
int deblank(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *lp1;
    struct line *lp2;
    ue64I_t nld;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    lp1 = curwp->w.dotp;
    while (lused(lp1) == 0 && (lp2 = lback(lp1)) != curbp->b_linep)
        lp1 = lp2;
    lp2 = lp1;
    nld = 0;
    while ((lp2 = lforw(lp2)) != curbp->b_linep && lused(lp2) == 0)
        ++nld;
    if (nld == 0) return TRUE;
    curwp->w.dotp = lforw(lp1);
    curwp->w.doto = 0;
    return ldelete(nld, FALSE);
}

/* Insert a newline, then enough tabs and spaces to duplicate the indentation
 * of the previous line. Assumes tabs are every eight characters. Quite simple.
 * Figure out the indentation of the current line. Insert a newline by calling
 * the standard routine. Insert the indentation by inserting the right number
 * of tabs and spaces. Return TRUE if all OK. Return FALSE if one of the
 * subcommands failed. Normally bound to "C-J".
 */
int indent(int f, int n) {
    UNUSED(f);
    int nicol;
    char c;
    size_t i;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return FALSE;
    while (n--) {
        nicol = 0;
        for (i = 0; i < lused(curwp->w.dotp); ++i) {
            c = lgetc(curwp->w.dotp, i);
            if (c != ' ' && c != '\t') break;
            if (c == '\t') nicol |= tabmask;
            ++nicol;
        }
        if (lnewline() == FALSE ||
             ((i = nicol / 8) != 0 && linsert_byte(i, '\t') == FALSE) ||
             ((i = nicol % 8) != 0 && linsert_byte(i, ' ') == FALSE))
            return FALSE;
    }
    return TRUE;
}

/* Character deleting backend for forwdel and backdel.
 * This is really easy, because the basic delete routine does all of the work.
 * For a negative argument it moves back that number of graphemes first.
 * For a positive one it moves forwards that number of graphemes, then
 * returns to the start position.
 * In both cases we are left at the "start" of the text we wish to delete
 * and the grapheme move returns the number of bytes moved (-ve if the
 * complete move request failed - e.g. at start/end of buffer) so that
 * number of bytes is then removed.
 * If any non-default argument is present, it kills rather than deletes,
 * to prevent loss of text if typed with a big argument.
 * Normally bound to ctlD (forwdel) and ctlH [BackSpace (backdel).
 * NOTE!!!
 * We can't just call ldelgrapheme() as that only deletes forwards and
 * also wouldn't update the com_flag.
 */
static int chardel(int f, int n) {
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */

/* Work out how many bytes to delete for n graphemes */
    if (n < 0) {    /* Go back requested amount */
        n = abs(back_grapheme(-n)); /* How many *do* we go back? */
    }
    else {          /* Go forwards requested amount then back to start */
        struct line *saved_dotp = curwp->w.dotp;    /* Save line */
        int saved_doto = curwp->w.doto;             /* Save offset */
        n = abs(forw_grapheme(n));  /* How many *d0* we go forwards? */
        curwp->w.dotp = saved_dotp;                 /* Restore line */
        curwp->w.doto = saved_doto;                 /* Restore offset */
    }

    if (f != FALSE) {               /* Really a kill.       */
        if ((com_flag & CFKILL) == 0) {
            kdelete();
            com_flag |= CFKILL;
        }
    }
/* Since we have a default (not user-given) count ensure KILL flag is unset */
    else com_flag &= ~CFKILL;
    return ldelete((ue64I_t) n, f);
}

/* Delete forward. This is real easy, because the basic delete routine does
 * all of the work. Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument. Normally bound to "C-D".
 */
int forwdel(int f, int n) {
    return chardel(f, n);
}
/* Delete backwards. This just sends the negated count to chardel.
 * which know to move backward before doing a forward delete.
 */
int backdel(int f, int n) {
    return chardel(f, -n);
}

/* Kill text. If called without an argument, it kills from dot to the end of
 * the line, unless it is at the end of the line, when it kills the newline.
 * If called with an argument of 0, it kills from the start of the line to dot.
 * If called with a positive argument, it kills from dot forward over that
 * number of newlines. If called with a negative argument it kills backwards
 * that number of newlines. Normally bound to "C-K".
 */
int killtext(int f, int n) {
    struct line *nextp;
    ue64I_t chunk;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if ((com_flag & CFKILL) == 0) { /* Clear kill buffer if */
          kdelete();                /* last wasn't a kill.  */
          com_flag |= CFKILL;
    }
    if (f == FALSE) {
        chunk = lused(curwp->w.dotp) - curwp->w.doto;
        if (chunk == 0) chunk = 1;
    }
    else if (n == 0) {
        chunk = curwp->w.doto;
        curwp->w.doto = 0;
    } else if (n > 0) {
        chunk = lused(curwp->w.dotp) - curwp->w.doto + 1;
        nextp = lforw(curwp->w.dotp);
        while (--n) {
            if (nextp == curbp->b_linep) return FALSE;
            chunk += lused(nextp) + 1;
            nextp = lforw(nextp);
        }
    } else {
        n = -n;
        chunk = curwp->w.doto;
        nextp = curwp->w.dotp;
        while (n--) {
            nextp = lback(nextp);
            if (nextp == curbp->b_linep) return FALSE;
            chunk += lused(nextp) + 1;
        }
        curwp->w.dotp = nextp;
        curwp->w.doto = 0;
    }
    return ldelete(chunk, TRUE);
}

/* change the editor mode status
 *
 * int kind;            true = set,          false = delete
 * int global;          true = global flag,  false = current buffer flag
 */
static int adjustmode(int kind, int global) {
    int i;          /* loop index */
    int status;     /* error return on input */
#if COLOR
    int uflag;      /* was modename uppercase?      */
#endif
    char prompt[50];    /* string to prompt user with */
    db_strdef(cbuf);    /* buffer to recieve mode name into */

/* Build the proper prompt string */
    sprintf(prompt, "%sode to %s: ", (global)? "Global m": "M",
         (kind == TRUE)? "add": "delete");

/* Prompt the user and get an answer */

    status = mlreply(prompt, &cbuf, CMPLT_NONE);
    if (status != TRUE) goto exit;

/* Check for 1st char being uppercase */
#if COLOR
    uflag = (db_charat(cbuf, 0) >= 'A' && db_charat(cbuf, 0) <= 'Z');
#endif

/* Test it first against the colors we know */
    for (i = 0; i < NCOLORS; i++) {
        if (db_casecmp(cbuf, cname[i]) == 0) {
/* Finding the match, we set the color */
#if COLOR
            if (uflag) {
                if (global) gfcolor = i;
                curwp->w_fcolor = i;
            }
            else {
                if (global) gbcolor = i;
                curwp->w_bcolor = i;
            }
            curwp->w_flag |= WFCOLR;
#endif
            mlerase();
            status = TRUE;
            goto exit;
        }
    }

/* Test it against the modes we know */

    for (i = 0; i < NUMMODES; i++) {
        if (db_casecmp(cbuf, mode2name[i]) == 0) {
/* Finding a match, we process it */
            if (kind == TRUE) {
                if (!ptt && (modecode[i] == 'P')) {
                    mlforce("No phonetic translation tables are yet defined!");
                    status = FALSE;
                    goto exit;
                }
                if (global) gmode |= (1 << i);
                else        curbp->b_mode |= (1 << i);
            }
            else {
                if (global) gmode &= ~(1 << i);
                else        curbp->b_mode &= ~(1 << i);
            }
/* Display new mode line */
            if (global == 0) upmode(NULL);
            mlerase();      /* erase the junk */
            status = TRUE;
            goto exit;
        }
    }
    status = FALSE;
    mlwrite_one("No such mode!");

exit:
    db_free(cbuf);
    return status;
}

/* prompt and set an editor mode
 *
 * int f, n;            default and argument
 */
int setemode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(TRUE, FALSE);
}

/* prompt and delete an editor mode
 *
 * int f, n;            default and argument
 */
int delmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(FALSE, FALSE);
}

/* prompt and set a global editor mode
 *
 * int f, n;            default and argument
 */
int setgmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(TRUE, TRUE);
}

/* prompt and delete a global editor mode
 *
 * int f, n;            default and argument
 */
int delgmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(FALSE, TRUE);
}

/* This function simply clears the message line,
 * mainly for macro usage
 *
 * int f, n;            arguments ignored
 */
int clrmes(int f, int n) {
    UNUSED(f); UNUSED(n);
    mlforce("");
    return TRUE;
}

/* This function writes a string on the message line
 * mainly for macro usage
 *
 * NOTE!!!! The string is checked (as one item) for variable expansion etc.
 * (within the mlreply call, via nextarg() and getval()) so if the string
 * starts with !, @, #, $, %, & or * you need to prepend a "~".
 * This is true even for things like - write-message "%lopts"
 *
 * IFF n comes in as 2, just print to stderr - for macro debugging ONLY.
 *
 */
int writemsg(int f, int n) {
    UNUSED(f);
    int status;
    db_strdef(buf);     /* buffer to receive message into */

    if ((status =
     mlreply("Message to write: ", &buf, CMPLT_NONE)) != TRUE)
        goto exit;

/* Write the message out */
    if (n == 2) fprintf(stderr, "%s\n", db_val(buf));
    else {
        mlforce_one(db_val(buf));
        if (kbdmode == PLAY) mline_persist = TRUE;
    }
exit:
    db_free(buf);
    return status;
}

/* A string getter for istring and rawstring, as they only differ in the
 * routine they use to prompt for and get the input.
 */
enum istr_type { RAW_STR, COOKED_STR };

static int string_getter(int f, int n, enum istr_type call_type) {
    int status;             /* status return code */
    char *prompt;
    const char *istrp;      /* Final string to insert */

    db_strdef(tstring);     /* string to add */
    db_strdef(tok);

/* Ask for string to insert, using the requested function.
 * If we are reading a macro just use the rest of the line (execstr).
 */
    if (clexec == FALSE) {
        if (call_type == RAW_STR) prompt = "String: ";
        else                      prompt = "Tokens/unicode chars: ";
        status = mlreply(prompt, &tstring, CMPLT_NONE);
        if (status != TRUE) goto exit;
        execstr = db_val(tstring);
    }

/* For COOKED_STR we have to process the rest of the line token-by-token.
 * NOTE: that this means any spaces in the string WILL BE SKIPPED!
 * e.g.
 *  insert-tokens 0x12 " " check1 " plus some more " &div 4 2
 * results in:
 *  ^R check1 plus some more 2
 */
    if (call_type == COOKED_STR) {
        const char *vp;
        db_strdef(nstring);
        while(*execstr != '\0') {
            execstr = token(execstr, &tok);
            if (db_len(tok) == 0) break;
            vp = getval(db_val(tok));   /* Must evaluate tokens */
            if (!strncmp(vp, "0x", 2)) {
                long add = strtol(vp+2, NULL, 16);
/* This is only for a single byte */
                if ((add < 0) || (add > 0xFF)) {
                    mlwrite("Oxnn syntax is only for a single byte (%s)",
                         db_val(tok));
                    continue;
                }
                db_addch(nstring, add);
            }
            else if (*vp == 'U' && *(vp+1) == '+') {
                int val = strtol(vp+2, NULL, 16);
                char utf[8];
                int incr = unicode_to_utf8(val, utf);
                db_appendn(nstring, utf, incr);
            }
            else {
                db_append(nstring, vp);
            }
        }
        istrp = db_val(nstring);
    }
    else {
/* For the RAW_STR we just take the next token and evaluate it.
 * e.g.
 *  insert-string     A   B   C D
 * results in:
 *  A
 * while:
 *  insert-string "A B C D"
 * results in:
 *  A B C D
 */
        execstr = token(execstr, &tok);
        istrp = getval(db_val(tok));
    }

    if (f == FALSE) n = 1;
    if (n < 0) n = -n;

/* insert it */

    while (n-- && (status = linstr(istrp)));

exit:
    db_free(tstring);
    db_free(tok);
    return status;
}

/* Insert text into the current buffer at current point
 * GGR
 * Treats response as a set of (evaluated) tokens, allowing unicode (U+xxxx)
 * and utf8 (0x..) characters to be inserted.
 * You can enter ASCII words (i.e. space-separated ones) within "..".
 *
 * int f, n;            ignored arguments
 */
int itokens(int f, int n) {
    return string_getter(f, n, COOKED_STR);
}

/* Insert a string into current buffer at current point
 * In macros it inserts the evaulated first token (so a quoted string
 * may be used).
 * In interactive mode it will insert the entire response (but ^X-<CR> will
 * replicated macro-mode).
 */
int istring(int f, int n) {
    return string_getter(f, n, RAW_STR);
}

/* ask for and overwrite a string into the current
 * buffer at the current point
 *
 * int f, n;            ignored arguments
 */
int ovstring(int f, int n) {
    int status;     /* status return code */
    db_strdef(tstring); /* string to add */

/* Ask for string to insert */
    status = mlreply("String to overwrite: ", &tstring, CMPLT_NONE);
    if (status != TRUE) goto exit;

    if (f == FALSE) n = 1;
    if (n < 0) n = -n;

/* Insert it */
    while (n-- && (status = lover(db_val(tstring))));

exit:
    db_free(tstring);
    return status;
}

/* GGR - extras follow */

/* Delete all but one white around cursor.
 * With a -ve arg alwats return TRUE.
 * If the abs value of the arg is 2 (so +2 or -2) always leave a space
 * even if there was not one originally (so "forceone").
 */
int leaveone(int f, int n) {
    UNUSED(f);
    int status = whitedelete(0, (n<0)? -1: 1);
    if (status || (n == 2) || (n == -2)) status = linsert_byte(1, ' ');
    return (n < 0)? TRUE: status;
}

int whitedelete(int f, int n) {
    UNUSED(f);

    const char *lp, *rp, *stp, *etp;
    stp = ltext(curwp->w.dotp);         /* Start of line text */
    etp = stp + lused(curwp->w.dotp);   /* End of line text */
    lp = rp = stp + curwp->w.doto;      /* Working pointers */

/* Find the left-most space */
    while (lp > stp) {
        switch(*(lp-1)) {
        case ' ':
        case '\t':
            lp--;
            continue;
        default:
            break;
        }
        break;          /* End loop if no match */
    }

/* Find the first non-space on the right */
    while (rp < etp) {
        switch(*rp) {
        case ' ':
        case '\t':
            rp++;
            continue;
        default:
            break;
        }
        break;          /* End loop if no match */
    }

/* Delete from lp up to (but not including) rp.
 * When there is nothing to remove return TRUE if we have been given an
 * arg of -1, otherwise return FALSE.
 */
    int to_delete = rp - lp;
    if (to_delete <= 0) return (n == -1);
    curwp->w.doto = lp - stp;       /* Move dot to left-most space */
    ldelete(to_delete, FALSE);      /* Delete them in one go */
    return TRUE;
}

/* Insert a ' and string length count at the end of a 'xxxx string.
 * For Fortran code....C doesn't really use the idiom.
 */
int quotedcount(int f, int n) {
    UNUSED(f); UNUSED(n);
    int savedpos;
    int count;
    int doubles;

    if (curbp->b_mode&MDVIEW)   /* don't allow this command if  */
         return(rdonly());      /* we are in read only mode     */
    doubles = 0;
    savedpos = curwp->w.doto;
    while (curwp->w.doto > 0) {
        if (lgetc(curwp->w.dotp, --(curwp->w.doto)) == '\'') {
/* Is it a doubled real quote? */
            if ((curwp->w.doto != 0) &&
                (lgetc(curwp->w.dotp, (curwp->w.doto - 1)) == '\'')) {
                --curwp->w.doto;
                ++doubles;
            }
            else {
                count = savedpos - curwp->w.doto - 1 - doubles;
                curwp->w.doto = savedpos;
                linstr("',");
                linstr(ue_itoa(count));
                return(TRUE);
            }
        }
    }
    curwp->w.doto = savedpos;
    mlwrite_one("Bad quoting!");
    TTbeep();
    return(FALSE);
}

/* Set GGR option flags all on if given non-default argument (n > 1).
 * Otherwise, switch all off.
 * Historic single flag setting for what is now a bit-map.
 */
int ggr_style(int f, int n) {
    UNUSED(f);
    if (n > 1)  ggr_opts = 0xffffffff;  /* All on */
    else        ggr_opts = 0;           /* All off */
    return TRUE;
}

/* Allow us to set whether to re-use a previously prompted-for
 * buffer/function/search name/string on a reexecute.
 */
int re_args_exec(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;

    db_strdef(buf);
    db_strdef(tok);

    status = mlreply("exec set: ", &buf, CMPLT_NONE);
    if (status != TRUE) goto exit;  /* Only act on +ve response */

    const char *rp = db_val(buf);
    int mode = RX_ON;
    int orig_rxargs = rxargs;
    while(*rp != '\0') {
        rp = token(rp, &tok);
        if (db_len(tok) == 0) break;
        if (!strcasecmp(db_val(tok), "none")) {
            rxargs = 0;
            continue;
        }
        if (!strcasecmp(db_val(tok), "all")) {
            rxargs = ~0;
            continue;
        }
        if (!strcmp(db_val(tok), "+")) {
            mode = RX_ON;
            continue;
        }
        if (!strcmp(db_val(tok), "-")) {
            mode = RX_OFF;
            continue;
        }
        int found = 0;
        for (struct rx_mask *rxmp = rx_mask; ; rxmp++) {
            if (!rxmp->entry) break;
            if (!db_cmp(tok, rxmp->entry)) {
                if (mode == RX_ON)  rxargs |= rxmp->mask;
                else                rxargs &= ~rxmp->mask;
                found = 1;
                break;
            }
        }
        if (found) continue;
/* If we get here we've found something we couldn't handle, which is
 * an error. So complain and re-instate the original setting.
 */
        mlwrite("Unexpected arg: %s", db_val(tok));
        rxargs = orig_rxargs;
        status = FALSE;
        goto exit;
    }

exit:
    db_free(buf);
    db_free(tok);
    return TRUE;
}

/* Open the directory where the file in the current buffer lives.
 * Bound to ^XU in the standard uemacs.rc.
 */
int open_parent(int f, int n) {
    UNUSED(f); UNUSED(n);

    if (*(curbp->b_fname) == '\0') {
        mlforce("This buffer has no filename");
        return FALSE;
    }
    char *bpath = Xstrdup(curbp->b_fname);
    char *parent_path = dirname(bpath);
    int status = TRUE;
    if (!showdir_handled(parent_path)) {
        mlforce("Failed to open parent");
        status = FALSE;
    }
    Xfree(bpath);
    return status;
}

/* Simulate a user typing by calling execute on each char in a string in
 * turn.
 * ONLY MEANT FOR TEST SCRIPTS!!!!
 */
int simulate(int f, int n) {
    UNUSED(f); UNUSED(n);

    db_strdef(input);
/* Grab the next token and advance past */
    nextarg("", &input, CMPLT_NONE);

    int status = FALSE;     /* In case n <= 0 */
    while (n-- > 0) {
        size_t offs = 0;
        unicode_t c;
        int mask = 0;
        while (offs < db_len(input)) {
            offs += utf8_to_unicode(db_val(input), offs,
                 db_len(input), &c);
            switch(c) {
/* Handle Esc(Meta), CtlX and control chars */
            case 0x1b:      /* Escape */
                mask |= META;
                continue;
            case 0x18:      /* CtlX */
                mask |= CTLX;
                continue;
            default:
                if (c <= 0x1F) c = CONTROL | (c + '@');
                else        /* If any mask is set, force Upper case */
                    if (mask) c = (c & ~DIFCASE);
            }
            status = execute(c|mask, 0, 1);
            mask = 0;
            if (!status) break;
        }
    }
    db_free(input);
    return status;
}
