/*      random.c
 *
 *      This file contains the command processing functions for a number of
 *      random commands. There is no functional grouping here, for sure.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"
#include "charset.h"

#include "utf8proc.h"

int tabsize; /* Tab size (0: use real tabs) */

/*
 * Set fill column to n.
 */
int setfillcol(int f, int n) {
/* GGR - default to 72 on no arg or an invalid one */
    if (f && (n > 0)) fillcol = n;
    else              fillcol = 72;

    mlwrite(MLpre "Fill column is %d" MLpost, fillcol);
    return TRUE;
}

/*
 * Display the current position of the cursor, in origin 1 X-Y coordinates,
 * the character that is under the cursor (in hex), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 * Normally this is bound to "C-X =".
 */
int showcpos(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *lp;        /* current line */
    long numchars;          /* # of chars in file */
    int numlines;           /* # of lines in file */
    long predchars;         /* # chars preceding point */
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
        if (lp == curwp->w_dotp) {
            predlines = numlines;
            predchars = numchars + curwp->w_doto;
            if ((curwp->w_doto) == llength(lp)) curchar = '\n';
            else {
                struct grapheme glyi;   /* Full data */
                bytes_used = lgetgrapheme(&glyi, FALSE);
                curchar = glyi.uc;
                if (glyi.cdm != 0) uc_used = 2;
                if (glyi.ex != NULL) {
                    for (unicode_t *xc = glyi.ex; *xc != UEM_NOCHAR; xc++)
                        uc_used++;
                    free(glyi.ex);
                }
            }
        }
/* On to the next line */
        ++numlines;
        numchars += llength(lp) + 1;
        lp = lforw(lp);
    }

/* If at end of file, record it */
    if (curwp->w_dotp == curbp->b_linep) {
        predlines = numlines;
        predchars = numchars;
        curchar = UEM_NOCHAR;   /* NoChar */
    }

/* Get real column and end-of-line column. */
    col = getccol(FALSE);
    savepos = curwp->w_doto;
    curwp->w_doto = llength(curwp->w_dotp);
    ecol = getccol(FALSE);
    curwp->w_doto = savepos;

/* If we are in DOS mode we need to add an extra char for the \r
 * on each preceding line.
 */
    if (curbp->b_mode & MDDOSLE) {
        predchars += predlines;
        numchars += numlines;
    }

    ratio = 0;              /* Ratio before dot. */
    if (numchars != 0) ratio = (100L * predchars)/numchars;

/* summarize and report the info */
    char descr[40];
    if (curchar == UEM_NOCHAR) strcpy(descr, "at end of buffer");
    else {
        char temp[8];
        if (curchar > 127) {        /* utf8!? */
            int blen = unicode_to_utf8(display_for(curchar), temp);
            temp[blen] = '\0';
        }
        else if (curchar <= ' ') {  /* non-printing - lookup descr */
                strcpy(temp, charset[curchar]);
            }
            else {                      /* printable ASCII */
                temp[0] = curchar;
                temp[1] = '\0';
            }
        snprintf(descr, 40 ,"char = U+%04x (%s)", curchar, temp);
    }
    char gl_descr[20] = "";
    if (bytes_used > 1)
          snprintf(gl_descr, 20, " %db/%duc", bytes_used, uc_used);
    mlwrite( "Line %d/%d Col %d/%d Byte %D/%D (%d%%) %s%s",
          predlines+1, numlines+1, col, ecol,
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
        if (lp == curwp->w_dotp) break;
        ++numlines;
        lp = lforw(lp);
    }

/* And return the resulting count */
    return numlines + 1;
}

/*
 * Return current column.  Stop at first non-blank given TRUE argument.
 * Column counting needs to take into account zero-widths, and also the
 * 2/3-column displays for control characters done by vtputc().
 */
int getccol(int bflg) {
    int i, col;
    struct line *dlp = curwp->w_dotp;
    int byte_offset = curwp->w_doto;
    int len = llength(dlp);

    col = i = 0;
    while (i < byte_offset) {
        unicode_t c;
        i += utf8_to_unicode(dlp->l_text, i, len, &c);
/* Check non-space first, if we are looking for it */
        if (c != ' ' && c != '\t' && bflg) break;
        update_screenpos_for_char(col, c);
    }
    return col;
}

/*
 * Set current column.
 * Column counting needs to take into account zero-widths, and also the
 * 2/3-column displays for control characters done by vtputc().
 *
 * int pos;             position to set cursor
 */
int setccol(int pos) {
    int i;          /* index into current line */
    int col;        /* current cursor column   */
    struct line *dlp = curwp->w_dotp;

    col = i = 0;
    int len = llength(dlp);

/* Scan the line until we are at or past the target column */
    while (i < len) {
        if (col >= pos) break;  /* Upon reaching the target, drop out */
        unicode_t c;
        i += utf8_to_unicode(dlp->l_text, i, len, &c);
        if (zerowidth_type(c)) continue;
        update_screenpos_for_char(col, c);
    }
    curwp->w_doto = i;              /* Set us at the new position... */
    return col >= pos;              /* ..and tell whether we made it */
}

/*
 * Twiddle the two characters on either side of dot. If dot is at the end of
 * the line twiddle the two characters before it. Return with an error if dot
 * is at the beginning of line; it seems to be a bit pointless to make this
 * work. This fixes up a very common typo with a single stroke. Normally bound
 * to "C-T". This always works within a line, so "WFEDIT" is good enough.
 */
int twiddle(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *dotp;
    int doto;
    char rch_buf[64], lch_buf[64];
    int rch_st, rch_nb, lch_st, lch_nb;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */

    dotp = curwp->w_dotp;
    doto = curwp->w_doto;

    int reset_col = 0;
    int maxlen = llength(dotp);
    char *l_buf = dotp->l_text;

/* GGR
 * twiddle here (and, e.g.,  bash) seems to act on the chars before and
 * after point -  except when you are at the end of a line....
 * This inconsistency seems odd. Prime emacs always acted on the two chars
 * preceding point, which was great for fixing typos as you made them.
 * If the ggr-style is set then twiddle will act on the two preceding chars.
 *
 * So where the right-hand character is depends on the mode and if you're
 * not in GGR style then it depends on whether you are at the end-of-line
 * too (doto == maxlen, so you then use ggr-style!!).
 * But once we know where the right-hand character is the left-hand one
 * is always the one preceding it.
 */
    if (using_ggr_style || doto == maxlen) {
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
 * So we take a copy of each and put them back in the reverse order.
 * If we are twiddling *around* point we might now be in the "middle" of
 * a character, so we have to reset to the original colum.
 * This is the start of the now R/h character (i.e what was L/h).
 */
    memcpy(rch_buf, l_buf + rch_st, rch_nb);
    memcpy(lch_buf, l_buf + lch_st, lch_nb);
    memcpy(l_buf+lch_st, rch_buf, rch_nb);
    memcpy(l_buf+lch_st+rch_nb, lch_buf, lch_nb);
    if (reset_col) curwp->w_doto = lch_st + rch_nb;
    lchange(WFEDIT);
    return TRUE;
}

/*
 * Quote the next character, and insert it into the buffer. All the characters
 * are taken literally, with the exception of the newline, which always has
 * its line splitting meaning. The character is always read, even if it is
 * inserted 0 times, for regularity. Bound to "C-Q"
 */
int quote(int f, int n) {
    UNUSED(f);
    int s;
    int c;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    c = tgetc();
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
    int nextstop;

    if (n < 0) return (FALSE);
    if (n == 0 || n > 1) {
        tabsize = n;
        return(TRUE);
    }

    if (! tabsize) return(linsert_byte(1, '\t'));

    nextstop = curwp->w_doto += tabsize - (getccol(FALSE) % tabsize);
    if (nextstop <= llength(curwp->w_dotp)) {
        curwp->w_doto = nextstop;
        return(TRUE);
    }

/* Otherwise tabstop past end, so move to end then insert */

    curwp->w_doto = llength(curwp->w_dotp);
    return(linsert_byte(tabsize - (getccol(FALSE) % tabsize), ' '));
}

/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
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
    return linsert_byte(tabsize - (getccol(FALSE) % tabsize), ' ');
}

/*
 * change tabs to spaces
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
        curwp->w_doto = 0;      /* start at the beginning */

/* Detab the entire current line */
        while (curwp->w_doto < llength(curwp->w_dotp)) {
/* If we have a tab */
            if (lgetc(curwp->w_dotp, curwp->w_doto) == '\t') {
                ldelchar(1, FALSE);
                insspace(TRUE, (tabmask + 1) - (curwp->w_doto & tabmask));
            }
            forw_grapheme(1);
        }
/* Advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    curwp->w_doto = 0;      /* to the begining of the line */
    thisflag &= ~CFCPCN;    /* flag that this resets the goal column */
    lchange(WFEDIT);        /* yes, we have made at least an edit */
    return TRUE;
}

/*
 * change spaces to tabs where posible
 *
 * int f, n;            default flag and numeric repeat count
 */
int entab(int f, int n) {
    int inc;        /* increment to next line [sgn(n)] */
    int fspace;     /* pointer to first space if in a run */
    int ccol;       /* current cursor column */
    char cchar;     /* current character */

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
          return rdonly();          /* we are in read only mode    */

    if (f == FALSE) n = 1;

/* Loop thru entabbing n lines */
    inc = ((n > 0) ? 1 : -1);
    while (n) {
        curwp->w_doto = 0;      /* start at the beginning */

/* Entab the entire current line */
        fspace = -1;
        ccol = 0;
        while (curwp->w_doto < llength(curwp->w_dotp)) {
/* See if it is time to compress */
            if ((fspace >= 0) && (nextab(fspace) <= ccol)) {
                if (ccol - fspace < 2) fspace = -1;
                else {
/* There is a bug here dealing with mixed space/tabed lines.......
 * it will get fixed.  (when?)
 */
                    back_grapheme(ccol - fspace);
                    ldelete((long) (ccol - fspace), FALSE);
                    linsert_byte(1, '\t');
                    fspace = -1;
                }
            }

/* Get the current character */
            cchar = lgetc(curwp->w_dotp, curwp->w_doto);

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
    curwp->w_doto = 0;      /* to the begining of the line */
    thisflag &= ~CFCPCN;    /* flag that this resets the goal column */
    lchange(WFEDIT);        /* yes, we have made at least an edit */
    return TRUE;
}

/*
 * trim trailing whitespace from the point to eol
 *
 * int f, n;            default flag and numeric repeat count
 */
int trim(int f, int n) {
    struct line *lp;        /* current line pointer */
    int offset;             /* original line offset position */
    int length;             /* current length */
    int inc;                /* increment to next line [sgn(n)] */

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */

    if (f == FALSE) n = 1;

/* Loop thru trimming n lines */
    inc = ((n > 0) ? 1 : -1);
    while (n) {
        lp = curwp->w_dotp;     /* find current line text */
        offset = curwp->w_doto; /* save original offset */
        length = lp->l_used;    /* find current length */

/* Trim the current line */
        while (length > offset) {
            if (lgetc(lp, length - 1) != ' ' && lgetc(lp, length - 1) != '\t')
                  break;
            length--;
        }
        lp->l_used = length;

/* Advance/or back to the next line */
        forwline(TRUE, inc);
        n -= inc;
    }
    lchange(WFEDIT);
    thisflag &= ~CFCPCN;    /* flag that this resets the goal column */
    return TRUE;
}

/*
 * Open up some blank space. The basic plan is to insert a bunch of newlines,
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

/*
 * Insert a newline. Bound to "C-M". If we are in CMODE, do automatic
 * indentation as specified.
 */
int insert_newline(int f, int n) {
    UNUSED(f);
    int s;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return FALSE;

/* If we are in C mode and this is a default <NL> */
    if (n == 1 && (curbp->b_mode & MDCMOD) && curwp->w_dotp != curbp->b_linep)
          return cinsert();

/*
 * If a newline was typed, fill column is defined, the argument is non-
 * negative, wrap mode is enabled, and we are now past fill column,
 * and we are not read-only, perform word wrap.
 */
    if ((curwp->w_bufp->b_mode & MDWRAP) && fillcol > 0 &&
        getccol(FALSE) > fillcol &&
        (curwp->w_bufp->b_mode & MDVIEW) == FALSE)
          execute(META | SPEC | 'W', FALSE, 1);

/* insert some lines */
    while (n--) {
        if ((s = lnewline()) != TRUE) return s;
#if SCROLLCODE
        curwp->w_flag |= WFINS;
#endif
    }
    return TRUE;
}

/* Insert a newline and indentation for C */
int cinsert(void) {
    char *cptr;     /* string pointer into text to copy */
    int tptr;       /* index to scan into line */
    int bracef;     /* was there a brace at the end of line? */
    int i;
    char ichar[NSTRING];    /* buffer to hold indent of last line */

/* GGR fix - nothing fancy if we're at left hand edge */
    if (curwp->w_doto == 0) return(lnewline());

/* Grab a pointer to text to copy indentation from */
    cptr = &curwp->w_dotp->l_text[0];

/* Check for a brace - check for last non-blank character! */
    tptr = curwp->w_doto;
    bracef = 0;
    i = tptr;
    while (--i >= 0) {
        if (cptr[i] == ' ' || cptr[i] == '\t') continue;
        if (cptr[i] == '{') {
            bracef = 1;
            break;
        }
    }

/* Save the indent of the previous line */
    i = 0;
    while ((i < tptr) && (cptr[i] == ' ' || cptr[i] == '\t')
          && (i < NSTRING - 1)) {
        ichar[i] = cptr[i];
        ++i;
    }
    ichar[i] = 0;           /* terminate it */

/* Put in the newline */
    if (lnewline() == FALSE) return FALSE;

/* And the saved indentation */
    linstr(ichar);

/* And one more tab for a brace */
    if (bracef) insert_tab(FALSE, 1);

#if SCROLLCODE
    curwp->w_flag |= WFINS;
#endif
    return TRUE;
}

/*
 * insert a brace into the text here...we are in CMODE
 *
 * int n;       repeat count
 * int c;       brace to insert (always } for now)
 */
int insbrace(int n, int c) {
    int ch;                 /* last character before input */
    int oc;                 /* caractere oppose a c */
    int i, count;
    int target;             /* column brace should go after */
    struct line *oldlp;
    int oldoff;

/* If we aren't at the beginning of the line... */
    if (curwp->w_doto != 0)

/* Scan to see if all space before this is white space */
    for (i = curwp->w_doto - 1; i >= 0; --i) {
        ch = lgetc(curwp->w_dotp, i);
        if (ch != ' ' && ch != '\t') return linsert_uc(n, c);
    }

/* Chercher le caractere oppose correspondant */
    switch (c) {
    case '}':
        oc = '{';
        break;
    case ']':
        oc = '[';
        break;
    case ')':
        oc = '(';
        break;
    default:
        return FALSE;
    }

    oldlp = curwp->w_dotp;
    oldoff = curwp->w_doto;

    count = 1;
    back_grapheme(1);

    while (count > 0) {
        if (curwp->w_doto == llength(curwp->w_dotp))
            ch = '\n';
        else
            ch = lgetc(curwp->w_dotp, curwp->w_doto);

        if (ch == c) ++count;
        if (ch == oc) --count;

        back_grapheme(1);
        if (boundry(curwp->w_dotp, curwp->w_doto, REVERSE)) break;
    }

    if (count != 0) {       /* no match */
        curwp->w_dotp = oldlp;
        curwp->w_doto = oldoff;
        return linsert_uc(n, c);
    }

    curwp->w_doto = 0;      /* Debut de ligne */
/* Aller au debut de la ligne apres la tabulation */
    while ((ch = lgetc(curwp->w_dotp, curwp->w_doto)) == ' ' || ch == '\t')
        forw_grapheme(1);

/* Delete back first */
    target = getccol(FALSE);    /* c'est l'indent que l'on doit avoir */
    curwp->w_dotp = oldlp;
    curwp->w_doto = oldoff;

    while (target != getccol(FALSE)) {
        if (target < getccol(FALSE)) /* on doit detruire des caracteres */
            while (getccol(FALSE) > target) backdel(FALSE, 1);
        else {          /* on doit en inserer */
            while (target - getccol(FALSE) >= 8) linsert_byte(1, '\t');
            linsert_byte(target - getccol(FALSE), ' ');
        }
    }
/* And insert the required brace(s) */
    return linsert_uc(n, c);
}

/* insert a # into the text here...we are in CMODE */
int inspound(void) {
    int ch; /* last character before input */
    int i;

/* If we are at the beginning of the line, no go */
    if (curwp->w_doto == 0) return linsert_byte(1, '#');

/* Scan to see if all space before this is white space */
    for (i = curwp->w_doto - 1; i >= 0; --i) {
        ch = lgetc(curwp->w_dotp, i);
        if (ch != ' ' && ch != '\t') return linsert_byte(1, '#');
    }

/* Delete back first */
    while (getccol(FALSE) >= 1) backdel(FALSE, 1);

/* And insert the required pound */
    return linsert_byte(1, '#');
}

/*
 * Delete blank lines around dot. What this command does depends if dot is
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
    long nld;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    lp1 = curwp->w_dotp;
    while (llength(lp1) == 0 && (lp2 = lback(lp1)) != curbp->b_linep)
        lp1 = lp2;
    lp2 = lp1;
    nld = 0;
    while ((lp2 = lforw(lp2)) != curbp->b_linep && llength(lp2) == 0)
        ++nld;
    if (nld == 0) return TRUE;
    curwp->w_dotp = lforw(lp1);
    curwp->w_doto = 0;
    return ldelete(nld, FALSE);
}

/*
 * Insert a newline, then enough tabs and spaces to duplicate the indentation
 * of the previous line. Assumes tabs are every eight characters. Quite simple.
 * Figure out the indentation of the current line. Insert a newline by calling
 * the standard routine. Insert the indentation by inserting the right number
 * of tabs and spaces. Return TRUE if all ok. Return FALSE if one of the
 * subcomands failed. Normally bound to "C-J".
 */
int indent(int f, int n) {
    UNUSED(f);
    int nicol;
    int c;
    int i;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return FALSE;
    while (n--) {
        nicol = 0;
        for (i = 0; i < llength(curwp->w_dotp); ++i) {
            c = lgetc(curwp->w_dotp, i);
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

/*
 * Delete forward. This is real easy, because the basic delete routine does
 * all of the work. Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument. Normally bound to "C-D".
 */
int forwdel(int f, int n) {
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if (n < 0) return backdel(f, -n);
    if (f != FALSE) {               /* Really a kill.       */
        if ((lastflag & CFKILL) == 0) kdelete();
        thisflag |= CFKILL;
    }
    return ldelchar((long) n, f);
}

/*
 * Delete backwards. This is quite easy too, because it's all done with other
 * functions. Just move the cursor back, and delete forwards. Like delete
 * forward, this actually does a kill if presented with an argument. Bound to
 * both "RUBOUT" and "C-H".
 */
int backdel(int f, int n) {
    int s;

    if (curbp->b_mode & MDVIEW)     /* Don't allow this command if */
        return rdonly();            /* We are in read only mode    */
    if (n < 0) return forwdel(f, -n);
    s = (back_grapheme(n) > 0);
    if (s == TRUE) {                /* Don't kdelete if we can't move! */
        if (f != FALSE) {           /* Really a kill.                  */
            if ((lastflag & CFKILL) == 0) kdelete();
            thisflag |= CFKILL;
        }
        s = ldelchar(n, f);
    }
    return s;
}

/*
 * Kill text. If called without an argument, it kills from dot to the end of
 * the line, unless it is at the end of the line, when it kills the newline.
 * If called with an argument of 0, it kills from the start of the line to dot.
 * If called with a positive argument, it kills from dot forward over that
 * number of newlines. If called with a negative argument it kills backwards
 * that number of newlines. Normally bound to "C-K".
 */
int killtext(int f, int n) {
    struct line *nextp;
    long chunk;

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if */
          return rdonly();          /* we are in read only mode    */
    if ((lastflag & CFKILL) == 0)   /* Clear kill buffer if */
          kdelete();                /* last wasn't a kill.  */
    thisflag |= CFKILL;
    if (f == FALSE) {
        chunk = llength(curwp->w_dotp) - curwp->w_doto;
        if (chunk == 0) chunk = 1;
    }
    else if (n == 0) {
        chunk = curwp->w_doto;
        curwp->w_doto = 0;
    } else if (n > 0) {
        chunk = llength(curwp->w_dotp) - curwp->w_doto + 1;
        nextp = lforw(curwp->w_dotp);
        while (--n) {
            if (nextp == curbp->b_linep) return FALSE;
            chunk += llength(nextp) + 1;
            nextp = lforw(nextp);
        }
    } else {
        mlwrite("neg kill");
        return FALSE;
    }
    return ldelete(chunk, TRUE);
}

/*
 * change the editor mode status
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
    char prompt[50];        /* string to prompt user with */
    char cbuf[NPAT];        /* buffer to recieve mode name into */

/* Build the proper prompt string */
    if (global) strcpy(prompt, "Global mode to ");
    else        strcpy(prompt, "Mode to ");

    if (kind == TRUE) strcat(prompt, "add: ");
    else              strcat(prompt, "delete: ");

/* Prompt the user and get an answer */

    status = mlreply(prompt, cbuf, NPAT - 1);
    if (status != TRUE) return status;

/* Check for 1st char being uppercase */
#if COLOR
    uflag = (cbuf[0] >= 'A' && cbuf[0] <= 'Z');
#endif

/* Test it first against the colors we know */
#if IBMPC
    for (i = 0; i <= NCOLORS; i++) {
#else
    for (i = 0; i < NCOLORS; i++) {
#endif
    if (strcasecmp(cbuf, cname[i]) == 0) {
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
        return TRUE;
        }
    }

/* Test it against the modes we know */

    for (i = 0; i < NUMMODES; i++) {
        if (strcasecmp(cbuf, mode2name[i]) == 0) {
/* Finding a match, we process it */
            if (kind == TRUE) {
                if (!ptt && !strcasecmp(mode2name[i], "PHON")) {
                    mlforce("No phonetic translation tables are yet defined!");
                    return FALSE;
                }
                if (global) gmode |= (1 << i);
                else        curbp->b_mode |= (1 << i);
            }
            else {
                if (global) gmode &= ~(1 << i);
                else        curbp->b_mode &= ~(1 << i);
            }
/* Display new mode line */
            if (global == 0) upmode();
            mlerase();      /* erase the junk */
            return TRUE;
        }
    }

    mlwrite("No such mode!");
    return FALSE;
}

/*
 * prompt and set an editor mode
 *
 * int f, n;            default and argument
 */
int setemode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(TRUE, FALSE);
}

/*
 * prompt and delete an editor mode
 *
 * int f, n;            default and argument
 */
int delmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(FALSE, FALSE);
}

/*
 * prompt and set a global editor mode
 *
 * int f, n;            default and argument
 */
int setgmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(TRUE, TRUE);
}

/*
 * prompt and delete a global editor mode
 *
 * int f, n;            default and argument
 */
int delgmode(int f, int n) {
    UNUSED(f); UNUSED(n);
    return adjustmode(FALSE, TRUE);
}

/*
 * This function simply clears the message line,
 * mainly for macro usage
 *
 * int f, n;            arguments ignored
 */
int clrmes(int f, int n) {
    UNUSED(f); UNUSED(n);
    mlforce("");
    return TRUE;
}

/*
 * This function writes a string on the message line
 * mainly for macro usage
 *
 * int f, n;            arguments ignored
 */
int writemsg(int f, int n) {
    UNUSED(f); UNUSED(n);
    char *sp;               /* pointer into buf to expand %s */
    char *np;               /* ptr into nbuf */
    int status;
    char buf[NPAT];         /* buffer to receive message into */
    char nbuf[NPAT * 2];    /* buffer to expand string into */

    if ((status = mlreply("Message to write: ", buf, NPAT - 1)) != TRUE)
        return status;

/* Expand all '%' to "%%" so mlwrite won't expect arguments */
    sp = buf;
    np = nbuf;
    while (*sp) {
        *np++ = *sp;
        if (*sp++ == '%') *np++ = '%';
    }
    *np = '\0';

/* Write the message out */
    mlforce(nbuf);
    return TRUE;
}

/*
 * the cursor is moved to a matching fence
 *
 * int f, n;            not used
 */
int getfence(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct line *oldlp;     /* original line pointer */
    int oldoff;             /* and offset */
    int sdir;               /* direction of search (1/-1) */
    int count;              /* current fence level count */
    char ch;                /* fence type to match against */
    char ofence;            /* open fence */
    char c;                 /* current character in scan */

/* Save the original cursor position */
    oldlp = curwp->w_dotp;
    oldoff = curwp->w_doto;

/* Get the current character */
    if (oldoff == llength(oldlp)) ch = '\n';
    else                          ch = lgetc(oldlp, oldoff);

/* Setup proper matching fence */
    switch (ch) {
    case '(':
        ofence = ')';
        sdir = FORWARD;
        break;
    case '{':
        ofence = '}';
        sdir = FORWARD;
        break;
    case '[':
        ofence = ']';
        sdir = FORWARD;
        break;
    case ')':
        ofence = '(';
        sdir = REVERSE;
        break;
    case '}':
        ofence = '{';
        sdir = REVERSE;
        break;
    case ']':
        ofence = '[';
        sdir = REVERSE;
        break;
    default:
        TTbeep();
        return FALSE;
    }

/* Set up for scan */
    count = 1;
    if (sdir == REVERSE) back_grapheme(1);
    else                 forw_grapheme(1);

/* Scan until we find it, or reach the end of file */
    while (count > 0) {
        if (curwp->w_doto == llength(curwp->w_dotp)) c = '\n';
        else          c = lgetc(curwp->w_dotp, curwp->w_doto);
        if (c == ch) ++count;
        if (c == ofence) --count;
        if (sdir == FORWARD) forw_grapheme(1);
        else                 back_grapheme(1);
        if (boundry(curwp->w_dotp, curwp->w_doto, sdir)) break;
    }

/* If count is zero, we have a match, move the sucker */
    if (count == 0) {
        if (sdir == FORWARD) back_grapheme(1);
        else                 forw_grapheme(1);
        curwp->w_flag |= WFMOVE;
        return TRUE;
    }

/* Restore the current position */
    curwp->w_dotp = oldlp;
    curwp->w_doto = oldoff;
    TTbeep();
    return FALSE;
}

/*
 * Close fences are matched against their partners, and if
 * on screen the cursor briefly lights there
 *
 * char ch;                     fence type to match against
 */
int fmatch(int ch) {
    struct line *oldlp;     /* original line pointer */
    int oldoff;             /* and offset */
    struct line *toplp;     /* top line in current window */
    int count;              /* current fence level count */
    char opench;            /* open fence */
    char c;                 /* current character in scan */
    int i;

/* First get the display update out there */
    update(FALSE);

/* Save the original cursor position */
    oldlp = curwp->w_dotp;
    oldoff = curwp->w_doto;

/* Setup proper open fence for passed close fence */
    if (ch == ')')      opench = '(';
    else if (ch == '}') opench = '{';
    else                opench = '[';

/* Find the top line and set up for scan */
    toplp = curwp->w_linep->l_bp;
    count = 1;
    back_grapheme(2);

/* Scan back until we find it, or reach past the top of the window */
    while (count > 0 && curwp->w_dotp != toplp) {
        if (curwp->w_doto == llength(curwp->w_dotp)) c = '\n';
        else          c = lgetc(curwp->w_dotp, curwp->w_doto);
        if (c == ch) ++count;
        if (c == opench) --count;
        back_grapheme(1);
        if (curwp->w_dotp == curwp->w_bufp->b_linep->l_fp && curwp->w_doto == 0)
            break;
    }

/* If count is zero, we have a match, display the sucker
 * There is a real machine dependant timing problem here we have
 * yet to solve......... (???)
 */
    if (count == 0) {
        forw_grapheme(1);
        for (i = 0; i < term.t_pause; i++) update(FALSE);
    }

/* Restore the current position */
    curwp->w_dotp = oldlp;
    curwp->w_doto = oldoff;
    return TRUE;
}

/* A string getter for istring and rawstring, as they only differ in the
 * routine they use to prompt for and get the input.
 */
enum istr_type { RAW_STR, COOKED_STR };
typedef int (*mlfn_t)(char *, char *, int);

int string_getter(int f, int n, enum istr_type call_type) {
    int status;                     /* status return code */
    char tstring[NLINE + 1];        /* string to add */
    mlfn_t ml_func;
    char *prompt;

/* ask for string to insert, using the requested funtion */

    if (call_type == RAW_STR) {
        ml_func = mlreplyall;
        prompt = "String: ";
    }
    else {
        ml_func = mlreply;
        prompt = "String/unicode chars: ";
    }

    status = ml_func(prompt, tstring, NLINE);
    if (status != TRUE)
        return status;

    char *rp = tstring;
    char nstring[NLINE] = "";
    int nlen = 0;
    char tok[NLINE];
    while(*rp != '\0') {
        rp = token(rp, tok, NLINE);
        if (tok[0] == '\0') break;
        if ((call_type == COOKED_STR) && !strncmp(tok, "0x", 2)) {
            long add = strtol(tok+2, NULL, 16);
            nstring[nlen++] = add;
        }
        else if ((call_type == COOKED_STR) && tok[0] == 'U' && tok[1] == '+') {
            int val = strtol(tok+2, NULL, 16);
            int incr = unicode_to_utf8(val, nstring+nlen);
            nlen += incr;
        }
        else {
            strcat(nstring, tok);
            nlen += strlen(tok);
        }
        nstring[nlen] = '\0';
    }
    if (f == FALSE) n = 1;
    if (n < 0) n = -n;

/* insert it */

    while (n-- && (status = linstr(nstring)));
    return status;
}

/* Ask for and insert a string into the current buffer at current point
 * GGR
 * Treats response as a set of tokens, allowing unicode (U+xxxx) and utf8
 * (0x..) characters to be entered.
 * You can enter ASCII words (i.e. space-separated ones) within "..".
 *
 * int f, n;            ignored arguments
 */
int istring(int f, int n) {
    return string_getter(f, n, COOKED_STR);
}

/*
 * Ask for a raw string and insert into current buffer at current point
 * Intended for use by macros.
 * It's istring, but uses mlreplyall() to get the text.
 */
int rawstring(int f, int n) {
    return string_getter(f, n, RAW_STR);
}

/*
 * ask for and overwite a string into the current
 * buffer at the current point
 *
 * int f, n;            ignored arguments
 */
int ovstring(int f, int n) {
    int status;     /* status return code */
    char tstring[NPAT + 1]; /* string to add */

/* Ask for string to insert */
    status = mlreplyt("String to overwrite: ", tstring, NPAT, metac);
    if (status != TRUE) return status;

    if (f == FALSE) n = 1;
    if (n < 0) n = -n;

/* Insert it */
    while (n-- && (status = lover(tstring)));
    return status;
}

/* GGR - extras follow */

/* Delete all but one white around cursor */
int leaveone(int f, int n) {
    if (whitedelete(f, n))
         return(linsert_byte(1, ' '));
    return(FALSE);
}

int whitedelete(int f, int n) {
    UNUSED(f); UNUSED(n);
    int c;
    int status;

    status = FALSE;
/* Use forwdel and backdel so *they* take care of VIEW etc */
    while (curwp->w_doto != 0 &&
         ((c = lgetc(curwp->w_dotp, (curwp->w_doto)-1)) == ' '
         || c == '\t')) {
         if (!backdel(0, 1)) return FALSE;
         status = TRUE;
    }
    while (curwp->w_doto != llength(curwp->w_dotp) &&
         ((c = lgetc(curwp->w_dotp, curwp->w_doto)) == ' '
         || c == '\t')) {
         if (!forwdel(0, 1)) return FALSE;
         status = TRUE;
    }
    return status;
}

int quotedcount(int f, int n) {
    UNUSED(f); UNUSED(n);
    int savedpos;
    int count;
    int doubles;

    if (curbp->b_mode&MDVIEW)       /* don't allow this command if  */
         return(rdonly());          /* we are in read only mode     */
    doubles = 0;
    savedpos = curwp->w_doto;
    while (curwp->w_doto > 0) {
        if (lgetc(curwp->w_dotp, --(curwp->w_doto)) == '\'') {
/* Is it a doubled real quote? */
            if ((curwp->w_doto != 0) &&
                (lgetc(curwp->w_dotp, (curwp->w_doto - 1)) == '\'')) {
                --curwp->w_doto;
                ++doubles;
            }
            else {
                count = savedpos - curwp->w_doto - 1 - doubles;
                curwp->w_doto = savedpos;
                linstr("',");
                linstr(itoa(count));
                return(TRUE);
            }
        }
    }
    curwp->w_doto = savedpos;
    mlwrite("Bad quoting!");
    TTbeep();
    return(FALSE);
}

/*
 * Set GGR mode global var if given non-default argument (n > 1).
 * Otherwise, switch it off,
 */
int ggr_style(int f, int n) {
    UNUSED(f);
    using_ggr_style = (n > 1);
    return TRUE;
}
