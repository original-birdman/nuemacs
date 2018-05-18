/*      word.c
 *
 *      The routines in this file implement commands that work word or a
 *      paragraph at a time.  There are all sorts of word mode commands.  If I
 *      do any sentence mode commands, they are likely to be put in this file.
 *
 *      Modified by Petri Kutvonen
 */

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

#include "utf8proc.h"

/* This is set by inword() when a call tests a grapheme with a
 * zero-width work-break attached.
 */
static int zw_break = 0;

/* Word wrap on n-spaces. Back-over whatever precedes the point on the current
 * line and stop on the first word-break or the beginning of the line. If we
 * reach the beginning of the line, jump back to the end of the word and start
 * a new line.  Otherwise, break the line at the word-break, eat it, and jump
 * back to the end of the word.
 * Returns TRUE on success, FALSE on errors.
 *
 * @f: default flag.
 * @n: numeric argument.
 */
int wrapword(int f, int n)
{
    UNUSED(f); UNUSED(n);
    int cnt;        /* size of word wrapped to next line */
    int c;          /* charector temporary */

/* Backup from the <NL> 1 char */
    if (back_grapheme(1) <= 0) return FALSE;

/* Back up until we aren't in a word, make sure there's a break in the line */
    cnt = 0;
    while (((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ')
         && (c != '\t')) {
        cnt++;
        if (back_grapheme(1) <= 0) return FALSE;
/* If we make it to the beginning, start a new line */
        if (curwp->w_doto == 0) {
            gotoeol(FALSE, 0);
            return lnewline();
        }
    }

/* Delete the forward white space */
    if (!forwdel(0, 1)) return FALSE;

/* Put in a end of line */
    if (!lnewline()) return FALSE;

/* And past the first word */
    while (cnt-- > 0) {
        if (forw_grapheme(1) <= 0) return FALSE;
    }

/* Make sure the display is not horizontally scrolled */
    if (curwp->w_fcol != 0) {
        curwp->w_fcol = 0;
        curwp->w_flag |= WFHARD | WFMOVE | WFMODE;
    }
    return TRUE;
}

/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "back_grapheme" and "forw_grapheme" routines.
 * Error if you try to move beyond the buffers.
 */
int backword(int f, int n)
{
        if (n < 0)
                return forwword(f, -n);
        if (back_grapheme(1) <= 0)
                return FALSE;
        while (n--) {
                while (inword() == FALSE) {
                        if (back_grapheme(1) <= 0)
                                return FALSE;
                }
                while ((inword() != FALSE) || zw_break) {
                        if (back_grapheme(1) <= 0)
                                return FALSE;
                }
        }
        return (forw_grapheme(1) > 0);      /* Success count => T/F */
}

/*
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forw_grapheme".
 * Error if you try and move beyond the buffer's end.
 */
int forwword(int f, int n)
{
    if (n < 0)
            return backword(f, -n);
    while (n--) {
/* GGR - reverse loop order according to ggr-style state
 * Determines whether you end up at the end of the current word (ggr-style)
 * or the start of next.
 */
        int state1 = using_ggr_style? FALSE: TRUE;
        int prev_zw_break = 0;
        while ((inword() == state1) || (!state1 && prev_zw_break)) {
            if (forw_grapheme(1) <= 0)
                return FALSE;
            prev_zw_break = zw_break;
        }
        prev_zw_break = zw_break;
        while ((inword() == !state1) || (!state1 && zw_break)) {
            if (forw_grapheme(1) <= 0)
                return FALSE;
            prev_zw_break = zw_break;
        }
    }
    return TRUE;
}

/* GGR
 * Force the case of the current character (or the main character of
 * a multi-char grapheme) to be a particular case.
 * For use by upper/lower/cap-word() and equiv.
 * We start by defining the calling parameters.
 */
void ensure_case(int want_case) {
    utf8proc_category_t case_tofix;
    utf8proc_int32_t (*case_hndlr) (utf8proc_int32_t);

    if (want_case == LOWERCASE) {
        case_tofix = UTF8PROC_CATEGORY_LU;
        case_hndlr = utf8proc_tolower;
    }
    else if (want_case == UPPERCASE) {
        case_tofix = UTF8PROC_CATEGORY_LL;
        case_hndlr = utf8proc_toupper;
    }
    else return;

    int saved_doto = curwp->w_doto;     /* Save position */
    struct grapheme gc;
    (void)lgetgrapheme(&gc, FALSE);     /* Doesn't move doto */
/* We only look at the base character for casing.
 * If it's not what we want to change, leave now...
 */
    if (utf8proc_category((utf8proc_int32_t)gc.uc) != case_tofix) return;
    char utf8_repl[8];
    int orig_utf8_len = unicode_to_utf8(gc.uc, utf8_repl);
    gc.uc = case_hndlr((utf8proc_int32_t)gc.uc);
    ldelete(orig_utf8_len, FALSE);
    linsert_uc(1, gc.uc);                  /* Inserts unicode */
    curwp->w_doto = saved_doto;         /* Restore positon */
    lchange(WFHARD);
    return;
}

/*
 * Move the cursor forward by the specified number of words. As you move,
 * convert any characters to upper case. Error if you try and move beyond the
 * end of the buffer. Bound to "M-U".
 */
int upperword(int f, int n)
{
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if (n < 0)
        return FALSE;
    while (n--) {
        while (inword() == FALSE) {
            if (forw_grapheme(1) <= 0)
                return FALSE;
        }
        int prev_zw_break = zw_break;
        while ((inword() != FALSE) || prev_zw_break) {
            ensure_case(UPPERCASE);
            if (forw_grapheme(1) <= 0)
                return FALSE;
            prev_zw_break = zw_break;
        }
    }
    return TRUE;
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert characters to lower case. Error if you try and move over the end of
 * the buffer. Bound to "M-L".
 */
int lowerword(int f, int n)
{
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if (n < 0)
        return FALSE;
    while (n--) {
        while (inword() == FALSE) {
            if (forw_grapheme(1) <= 0)
                return FALSE;
        }
        int prev_zw_break = zw_break;
        while ((inword() != FALSE)  || prev_zw_break) {
            ensure_case(LOWERCASE);
            if (forw_grapheme(1) <= 0)
                return FALSE;
            prev_zw_break = zw_break;
        }
    }
    return TRUE;
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case. Error if you try and move past the end of the
 * buffer. Bound to "M-C".
 */
int capword(int f, int n)
{
    UNUSED(f);
    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */
    if (n < 0)
        return FALSE;
    while (n--) {
        while (inword() == FALSE) {
            if (forw_grapheme(1) <= 0)
                return FALSE;
        }
        int prev_zw_break = zw_break;
        if (inword() != FALSE) {
            ensure_case(UPPERCASE);
            if (forw_grapheme(1) <= 0)
                return FALSE;
            while ((inword() != FALSE) || prev_zw_break) {
                ensure_case(LOWERCASE);
                if (forw_grapheme(1) <= 0)
                    return FALSE;
            }
        }
    }
    return TRUE;
}

/*
 * Kill forward by "n" words. Remember the location of dot. Move forward by
 * the right number of words. Put dot back where it was and issue the kill
 * command for the right number of characters. With a zero argument, just
 * kill one word and no whitespace. Bound to "M-D".
 * GGR - forw_grapheme()/back_grapheme() now move by graphemes, so we need
 *       to track the byte count (which they now return).
 */
int delfword(int f, int n)
{
        struct line *dotp;      /* original cursor line */
        int doto;               /*      and row */
        long size;              /* # of chars to delete */

        /* don't allow this command if we are in read only mode */
        if (curbp->b_mode & MDVIEW)
                return rdonly();

        /* GGR - allow a -ve arg - use the backward function */
        if (n < 0)
                return(delbword(f, -n));

        /* Clear the kill buffer if last command wasn't a kill */
        if ((lastflag & CFKILL) == 0)
                kdelete();
        thisflag |= CFKILL;     /* this command is a kill */

        /* save the current cursor position */
        dotp = curwp->w_dotp;
        doto = curwp->w_doto;

        /* figure out how many characters to give the axe */
        size = 0;
        int moved = 0;

        /* get us into a word.... */
        while (inword() == FALSE) {
                moved = forw_grapheme(1);
                if (moved <= 0) return FALSE;
                size += moved;
        }

        if (n == 0) {
                /* skip one word, no whitespace! */
                int prev_zw_break = 0;
                while ((inword() == TRUE) || prev_zw_break) {
                        moved = forw_grapheme(1);
                        if (moved <= 0) return FALSE;
                        size += moved;
                        prev_zw_break = zw_break;
                }
        } else {
                /* skip n words.... */
                while (n--) {

                        /* if we are at EOL; skip to the beginning of the next */
                        while (curwp->w_doto == llength(curwp->w_dotp)) {
                                moved = forw_grapheme(1);
                                if (moved <= 0) return FALSE;
                                ++size;     /* Will move one to next line */
                        }

                        /* move forward till we are at the end of the word */
                        int prev_zw_break = 0;
                        while ((inword() == TRUE) || prev_zw_break) {
                                moved = forw_grapheme(1);
                                if (moved <= 0) return FALSE;
                                size += moved;
                                prev_zw_break = zw_break;
                        }

                        /* if there are more words, skip the interword stuff */
                        if (n != 0)
                                while (inword() == FALSE) {
                                        moved = forw_grapheme(1);
                                        if (moved <= 0) return FALSE;
                                        size += moved;
                                }
                }
        }
        /* restore the original position and delete the words */
        curwp->w_dotp = dotp;
        curwp->w_doto = doto;
        return ldelete(size, TRUE);
}

/*
 * Kill backwards by "n" words. Move backwards by the desired number of words,
 * counting the characters. When dot is finally moved to its resting place,
 * fire off the kill command. Bound to "M-Rubout" and to "M-Backspace".
 * GGR - forw_grapheme()/back_grapheme() now move by graphemes, so we need
 *       to track the byte count (which they now return).
 */
int delbword(int f, int n)
{
        long size;

/* GGR - variables for kill-ring fix-up */
        int    i, status, ku;
        char   *sp;             /* pointer into string to insert */
        struct kill *kp;        /* pointer into kill buffer */
        struct kill *op;

        /* don't allow this command if we are in read only mode */
        if (curbp->b_mode & MDVIEW)
                return rdonly();

        /* GGR - allow a -ve arg - use the forward function */
        if (n <= 0)
                return(delfword(f, -n));

        /* Clear the kill buffer if last command wasn't a kill */
        if ((lastflag & CFKILL) == 0)
                kdelete();
        thisflag |= CFKILL;     /* this command is a kill */

        int moved;
        moved = back_grapheme(1);
        if (moved <= 0) return FALSE;
        size = moved;
        while (n--) {
                while (inword() == FALSE) {
                        moved = back_grapheme(1);
                        if (moved <= 0) return FALSE;
                        size += moved;
                }
                while ((inword() != FALSE) || zw_break) {
                        moved = back_grapheme(1);
                        if (moved <= 0) goto bckdel;
                        size += moved;
                }
        }
        moved = forw_grapheme(1);
        if (moved <= 0) return FALSE;
        size -= moved;

/* GGR
 * We have to cater for the function being called multiple times in
 * succession. This should be the equivalent of Esc<n>delbword, i.e. the
 * text can be yanked back and still all be in the same order as it was
 * originally.
 * This means that we have to fiddle with the kill ring buffers.
 */
bckdel:
    if ((kp = kbufh[0]) != NULL) {
        kbufh[0] = kbufp = NULL;
        ku = kused[0];
        kused[0] = KBLOCK;
    }

    status = ldelete(size, TRUE);

    /* fudge back the rest of the kill ring */
    while (kp != NULL) {
        if (kp->d_next == NULL)
                i = ku;
        else
                i = KBLOCK;
        sp = kp->d_chunk;
        while (i--) {
            kinsert(*sp++);
        }
        op = kp;
        kp = kp->d_next;
        free(op);           /* kinsert() mallocs, so we free */
    }

    return(status);
}

/*
 * Return TRUE if the character at dot is a character that is considered to be
 * part of a word. The word character list is hard coded. Should be setable.
 * GGR - the grapheme-based version.
 */
int inword(void)
{
    struct grapheme gc;

    if (curwp->w_doto == llength(curwp->w_dotp))
        return FALSE;
    (void)lgetgrapheme(&gc, FALSE);

    zw_break = 0;
    if (gc.cdm == 0x200B) {
        zw_break = 1;
    }
    else if (gc.ex) {
        for (unicode_t *exc = gc.ex; *exc != END_UCLIST; exc++) {
            if (*exc == 0x200B) {
                zw_break = 1;
                break;
            }
        }
    }

/* We only look at the base character to determine whether this is a
 * word character.
 */
    const char *uc_class =
         utf8proc_category_string((utf8proc_int32_t)gc.uc);

    if (uc_class[0] == 'L') return TRUE;    /* Letter */
    if (uc_class[0] == 'N') return TRUE;    /* Number */
    return FALSE;
}

#if     WORDPRO
/* a GGR one.. */
int fillwhole(int f, int n)
{
    int savline, thisline;
    int status = FALSE;

        gotobob(TRUE, 1);
        savline = 0;
        mlwrite(MLpre "Filling all paragraphs in buffer.." MLpost);
        while ((thisline = getcline()) > savline) {
                status = fillpara(f, n);
                savline = thisline;
        }
        mlwrite("");
        return(status);
}

/*
 * delete n paragraphs starting with the current one
 *
 * int f        default flag
 * int n        # of paras to delete
 */
int killpara(int f, int n)
{
        UNUSED(f);
        int status;     /* returned status of functions */

        while (n--) {           /* for each paragraph to delete */

                /* mark out the end and beginning of the para to delete */
                gotoeop(FALSE, 1);

                /* set the mark here */
                curwp->w_markp = curwp->w_dotp;
                curwp->w_marko = curwp->w_doto;

                /* go to the beginning of the paragraph */
                gotobop(FALSE, 1);
                curwp->w_doto = 0;  /* force us to the beginning of line */

                /* and delete it */
                if ((status = killregion(FALSE, 1)) != TRUE)
                        return status;

                /* and clean up the 2 extra lines */
                ldelete(2L, TRUE);
        }
        return TRUE;
}


/*
 *      wordcount:      count the # of words in the marked region,
 *                      along with average word sizes, # of chars, etc,
 *                      and report on them.
 *
 * int f, n;            ignored numeric arguments
 */
int wordcount(int f, int n)
{
        UNUSED(f); UNUSED(n);
        struct line *lp;        /* current line to scan */
        int offset;             /* current char to scan */
        int orig_offset;        /* offset in line at start */
        long size;              /* size of region left to count */
        int ch;                 /* current character to scan */
        int wordflag;           /* are we in a word now? */
        int lastword;           /* were we just in a word? */
        long nwords;            /* total # of words */
        long nchars;            /* total number of chars */
        int nlines;             /* total number of lines in region */
        int avgch;              /* average number of chars/word */
        int status;             /* status return code */
        struct region region;   /* region to look at */

        /* make sure we have a region to count */
        if ((status = getregion(&region)) != TRUE)
                return status;
        lp = region.r_linep;
        offset = region.r_offset;
        orig_offset = offset;
        size = region.r_size;
        /* count up things */
        lastword = FALSE;
        nchars = 0L;
        nwords = 0L;
        nlines = 0;
        while (size--) {

                /* get the current character */
                if (offset == llength(lp)) {    /* end of line */
                        ch = '\n';
                        lp = lforw(lp);
                        offset = 0;
                        ++nlines;
                } else {
                        ch = lgetc(lp, offset);
                        ++offset;
                }

                /* and tabulate it */
                wordflag = (
                    (isletter(ch)) ||
                    (ch >= '0' && ch <= '9'));
                if (wordflag == TRUE && lastword == FALSE)
                        ++nwords;
                lastword = wordflag;
                ++nchars;
        }
/* GGR - Increment line count if offset is now more than at the start
 * So line1 col3 -> line2 col55 is 2 lines,. not 1
 */
        if (offset > orig_offset) ++nlines;
        /* and report on the info */
        if (nwords > 0L)
                avgch = (int) ((100L * nchars) / nwords);
        else
                avgch = 0;

/* GGR - we don't need nlines+1 here now */
        mlwrite("Words %D Chars %D Lines %d Avg chars/word %f",
                nwords, nchars, nlines, avgch);
        return TRUE;
}
#endif

/*
 * Set the GGR-added end-of-sentence list for use by fillpara and
 * justpara.
 * Allows utf8 punctuation, but does NOT cater for any zero-width
 * character usage (so no combining diacritical marks here...).
 * Mainly for macro usage
 *
 * int f, n;            arguments ignored
 */
static int n_eos = 0;
static char eos_str[NLINE] = "";    /* String given by user */
int eos_chars(int f, int n)
{
        UNUSED(f); UNUSED(n);
        int status;
        char prompt[NLINE+60];
        char buf[NLINE];

        if (n_eos == 0) {
            strcpy(eos_str, "none");    /* Clearer for user? */
        }
        sprintf(prompt,
            "End of sentence characters " MLpre "currently %s" MLpost ":",
        eos_str);

        status = mlreply(prompt, buf, NLINE - 1);
        if (status == FALSE) {      /* Empty response - remove item */
                if (eos_list) free (eos_list);
                n_eos = 0;
        }
        else if (status == TRUE) {  /* Some response - set item */
/* We'll get the buffer length in characters, then allocate that number of
 * unicode chars. It might be more than we need (if there is a utf8
 * multi-byte character in there) but it's not going to be that big.
 * Actually we'll allocate one extra and put an illegal value at the end.
 */
                int len = strlen(buf);
                eos_list = realloc(eos_list, sizeof(unicode_t)*(len + 1));
                int i = 0;
                n_eos = 0;
                while (i < len) {
                        unicode_t c;
                        i += utf8_to_unicode(buf, i, len, &c);
                        eos_list[n_eos++] = c;
                }
                eos_list[n_eos] = END_UCLIST;
                strcpy(eos_str, buf);
        }
/* Do nothing on anything else - allows you to abort once you've started. */
        return status;              /* Whatever mlreply returned */
}

/* Space factor needed for a unicode array copied from a char array */
#define MFACTOR sizeof(unicode_t)/sizeof(char)

/*
 * Generic filling routine to be called by various front-ends.
 * Fills the text between point and end of paragraph.
 * given parameters.
 *  indent      l/h indent to use (NOT on first line!!)
 *  width       column at which to wrap
 *  f_ctl       various other conditions, determined by caller.
 */
struct filler_control {
    int justify;
};
int filler(int indent, int width, struct filler_control *f_ctl) {

    unicode_t *wbuf;        /* buffer for current word     */
    int wbuf_ents;          /* Number of elements needed.. */
    int *space_ind;         /* Where the spaces are */
    int nspaces;            /* How many spaces there are */

    int wordlen;            /* graphemes in current word    */
    int wi;                 /* unicde index in current word */
    int clength;            /* position on line during fill */
    int i;                  /* index during word copy       */
    int pending_space;      /* Number of pending spaces     */
    int newlength;          /* tentative new line length    */
    int eosflag;            /* was the last char a period?  */

    int gap;                /* number of spaces to pad with */
    int rtol;               /* Pass direction */
    struct region f_region; /* Formatting region */

    if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
            return rdonly();        /* we are in read only mode     */

    if (width == 0) {             /* no fill column set */
        mlwrite("No fill column set");
        return FALSE;
    }
    rtol = 1;           /* Direction of first padding pass */

/* Since mark might be within the current paragraph its status after
 * this is undefined, so we cn play with it.
 */
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;
    if (!gotoeop(FALSE, 1)) return FALSE;
    if (!getregion(&f_region)) return FALSE;

/* The region must not be empty. This also caters for an empty buffer. */

    if (f_region.r_size <= 0) return FALSE;

/* Go to the start of the region */

    curwp->w_dotp = f_region.r_linep;
    curwp->w_doto = f_region.r_offset;

/* Initialize various info */

    clength = getccol(FALSE);   /* Less than r_offset if multibyte char */
    eosflag = 0;
    pending_space = 0;

/*
 * wbuf needs to be sufficiently long to contain all of the longest
 * line (plus 1, for luck...) in case there is no word-break in it.
 * So we allocate it dynamically.
 */
    wbuf_ents = llength(curwp->w_dotp) + 1;
    wbuf = malloc(MFACTOR*wbuf_ents);
    wordlen = wi = 0;

/* If we've been asked to right-justify we need some space */
    space_ind = f_ctl->justify? malloc(sizeof(int)*(wbuf_ents+1)/2): NULL;
    nspaces = 0;

    long togo = f_region.r_size + 1;    /* Need final end-of-line */
    while (togo > 0) {

/* Get the next character.
 * lgetgrapheme will return a NL at the end of line
 * If we get to end end-of-line without having got any word chars since the
 * previous end-of-line then we are at the end of a paragraph
 */
        struct grapheme gi;
        int bytes = lgetgrapheme(&gi, 0);
        if (bytes <= 0) {           /* We are in trouble... */
            mlforce("ERROR: cannot continue filling.");
            return FALSE;
        }
        if (gi.uc == '\n') {
            gi.uc = ' ';

/* Reallocate buffer if more space needed.
 * Remember we are using unicode_t, while llength is char
 * Make sure we check the *next* line length
 */
            if (llength(lforw(curwp->w_dotp)) >= wbuf_ents) {
                wbuf_ents = llength(lforw(curwp->w_dotp)) + 1;
                wbuf = realloc(wbuf, MFACTOR*wbuf_ents);
                if (space_ind) {    /* We're padding */
                     space_ind =
                          realloc(space_ind, sizeof(int)*(wbuf_ents+1)/2);
                }
            }
        }

/* Delete what we have read and note we've handled bytes*/

        ldelete(bytes, FALSE);
        togo -= bytes;

/* If not a separator, just add it in */

        int at_whitespace = (gi.cdm == 0 ) && (gi.uc == ' ' || gi.uc == '\t');
        if (!at_whitespace) {                       /* A word "char" */

/* Handle any defined punctuation characters */
            eosflag = 0;
            if (eos_list) {     /* Some eos defined */
                for (unicode_t *eosch = eos_list;
                     *eosch != END_UCLIST; eosch++) {
                    if (gi.cdm == 0 && gi.uc == *eosch) {
                        eosflag = 1;
                        break;
                    }
                }
            }
            wbuf[wi++] = gi.uc; /* Dynamically sized, so these    */
            if (gi.cdm != 0) {  /* assignments can always be done */
                wbuf[wi++] = gi.cdm;
                if (gi.ex) {
                    int xc = 0;
                    while (gi.ex[xc] != END_UCLIST)
                        wbuf[wi++] = gi.ex[xc];
                }
            }
            wordlen += utf8proc_charwidth(gi.uc);
/* Free-up any allocated grapheme space...and on to next char */
            if (gi.ex != NULL) free(gi.ex);
            continue;
        }

/* We are at a word-break (whitespace). Do we have a word waiting?
 * Calculate tentative new length with word added. If it is > width
 * then push a newline before flushing the waiting word.
 * We also flush if there are no bytes left or if we are forcing a
 * newline between paragraphs.
 */
        if (!wordlen) continue;     /* Multiple spaces */

        newlength = clength + pending_space + wordlen;
        if (newlength <= width) {
/* We'll just add the word to the current line */
            if (pending_space) {
                if (space_ind) {
/* We need to save the doto location, not clength, to allow for unicode */
                     space_ind[nspaces] = curwp->w_doto;
                     nspaces++;
                }
            }
        } else {        /* Need to add a newline before adding word */
            if (nspaces && togo) {
/* Justify the right edges too..
 * Now written in a manner to handle unicode chars....
 * We can only do it if we've actually found some spaces and only want to
 * do it if we aren't forcing a newline (end of paragraph) and have
 * more characters to process (not at end of region).
 *
 * Remember where we are on the line.
 */
                int end_doto = curwp->w_doto;
                gap = width - clength;  /* How much we need to add (in columns) */
                while (gap > 0) {
                    if (rtol) for (int ns = nspaces-1; ns >= 0; ns--) {
                        curwp->w_doto = space_ind[ns];
                        linsert_byte(1, ' ');
                        end_doto++;     /* A space is one byte... */
                        rtol = 0;       /* So next pass goes other way */
                        if (--gap <= 0) break;
                        for (int ni = ns; ni < nspaces; ni++) {
                            space_ind[ni]++;
                        }
                    }
                    if (gap <= 0) break;
                    for (int ns = 0; ns < nspaces; ns++) {
                        curwp->w_doto = space_ind[ns];
                        linsert_byte(1, ' ');
                        end_doto++;     /* A space is one byte... */
                        rtol = 1;       /* So next pass goes other way */
                        if (--gap <= 0) break;
                        for (int ni = ns; ni < nspaces; ni++) {
                            space_ind[ni]++;
                        }
                    }
                }
                nspaces = 0;
/* Fix up our location in the line to be where we were, plus added spaces */
                curwp->w_doto = end_doto;
            }
            lnewline();
            pending_space = 0;
            for (int i = 0; i < indent; i++) linsert_byte(1, ' ');
            clength = indent;
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
        }
/* Possibly add space(s) */
        if (pending_space) linsert_byte(pending_space, ' ');

/* ...and add in the word in either case */
        for (i = 0; i < wi; i++) {
            linsert_byte(1, wbuf[i]);
        }
        clength += wordlen + pending_space;
        wordlen = wi = 0;
        pending_space = 1 + eosflag;
    }

/* And add a last newline for the end of our new paragraph, unless at
 * end of last paragraph, in which case just got to last line */
    if (lforw(curwp->w_dotp) != curbp->b_linep) lnewline();
    else gotoeob(FALSE, 1);
    free(wbuf);     /* Mustn't forget these... */
    if (space_ind) free(space_ind);

/* Make sure the display is not horizontally scrolled */
    if (curwp->w_fcol != 0) {
        curwp->w_fcol = 0;
        curwp->w_flag |= WFHARD | WFMOVE | WFMODE;
    }

    return TRUE;
}

/*
 * Fill the current paragraph according to the current
 * fill column.
 * Leave any leading whitespace in the paragraph in place.
 *
 * f and n - deFault flag and Numeric argument
 */

int fillpara(int f, int n) {
    UNUSED(f);
    struct filler_control fp_ctl;
    fp_ctl.justify = 0;
    if (n < 0) {
        n = -n;
        fp_ctl.justify = 1;
    }
    if (n == 0) n = 1;
    int status = FALSE;
    while (n--) {
        forwword(FALSE, 1);
        if (!gotobop(FALSE, 1)) return FALSE;
        status = filler(0, fillcol, &fp_ctl);
        if (status != TRUE) break;
    }
    return status;
}

/* Fill the current paragraph using the current column as
 * the indent.
 * Remove any exisiting leading whitespace in the paragraph.
 *
 * int f, n;            deFault flag and Numeric argument
 */
int justpara(int f, int n)
{
    UNUSED(f);
    struct filler_control fp_ctl;
    fp_ctl.justify = 0;
    if (n < 0) {
        n = -n;
        fp_ctl.justify = 1;
    }
    if (n == 0) n = 1;

/* Have to cater for tabs and multibyte chars...can't use w_doto */
    int leftmarg = getccol(FALSE);
    if (leftmarg + 10 > fillcol) {
        mlwrite("Column too narrow");
        return FALSE;
    }
    int status = FALSE;
    while (n--) {
/* We need to get rid of any leading whitespace, then pad first line
 * here, at bop */
        gotoeol(FALSE, 1);          /* In case we are already at bop */
        gotobop(FALSE, 1);
        (void)whitedelete(1, 1);    /* Don't care whether there was any */
        curwp->w_doto = 0;          /* Should be 0 anyway... */
        for (int i = 0; i < leftmarg; i++) linsert_byte(1, ' ');
        status = filler(leftmarg, fillcol, &fp_ctl);
        if (status != TRUE) break;
/* Position cursor at indent column in next paragraph */
        forwword(FALSE, 1);
        setccol(leftmarg);
    }

    return status;
}

/* Reformat the paragraphs containing the region into a list.
 * This is actually called via front-ends which set the format
 * to use. It must not be made user-callable.
 *
 * int f, n;            deFault flag and Numeric argument
 */
static const char *lbl_fmt;
static int region_listmaker(int f, int n)
{
    UNUSED(f);
    char label[80];
    struct region f_region; /* Formatting region */
    struct filler_control fp_ctl;
    fp_ctl.justify = 0;
    if (n < 0) {
        n = -n;
        fp_ctl.justify = 1;
    }
    if (n == 0) n = 1;

/* We will be working on the defined region (mark to/from point) so
 * get that.
 */
    if (!getregion(&f_region)) return FALSE;

/* The region must not be empty. This also caters for an empty buffer. */

    if (f_region.r_size <= 0) return FALSE;

/* We have to figure out where to stop.
 * The buffer lines may be re-allocated during the reformat, so we
 * count the paragraphs now and loop that many times.
 *
 * Start by getting to the end of the paragraph containing the end of
 * the region.
 */
    struct line *flp = f_region.r_linep;
    long togo = f_region.r_size + f_region.r_offset;
    while (1) {
        long left = togo - (llength(flp) + 1);   /* Incl newline */
        if (left < 0) break;
        togo = left;
        flp = lforw(flp);
    }
    curwp->w_dotp = flp;                /* Fix up line and ... */
    curwp->w_doto = togo;               /* ... offset for end-of-range */

/* We want to get to EOP, but if we are at the end of the last line
 * (which is a likely marking scenario for a region) we don't want to go
 * to the next paragraph!
 * So we first go back a word.
 * This also means we can put the mark *between* paragraphs.....
 */
    backword(FALSE, 1);
    gotoeop(FALSE, 1);
    struct line *eopline = curwp->w_dotp;

/* Now we go back to the beginning and advance by end of paragraphs
 * until we find the one we just found. This gives us the loop count.
 */

    curwp->w_dotp = f_region.r_linep;
    curwp->w_doto = f_region.r_offset;
    int pc = 0;
    while(1) {
        pc++;
        gotoeop(FALSE, 1);
        if (eopline == curwp->w_dotp) break;
    }

/* Back to the start and get to start of the paragraph.
 * To handle already being at the bop, or on an empty line, we
 * actually go to eop first.
 */
    curwp->w_dotp = f_region.r_linep;
    curwp->w_doto = f_region.r_offset;
    gotoeop(FALSE, 1);              /* In case we are already at bop */
    gotobop(FALSE, 1);

    int status = FALSE;
    int ix = 0;
    while (pc--) {                  /* Loop for paragraph count */
        (void)whitedelete(1, 1);    /* Don't care whether there was any */
        ix++;                       /* Insert the counter */
        int cc = snprintf(label, sizeof(label)-1, lbl_fmt, ix);
        curwp->w_doto = 0;          /* Should be 0 anyway... */
        linstr(label);
        status = filler(cc, fillcol, &fp_ctl);

/* Onto the next paragraph */

        if (forwword(FALSE, 1)) gotobol(FALSE, 1);
        if (status != TRUE) break;
    }

/* We've lost mark by the time we get here, but we need to think about what
 * ctl-C (reexec) will do.
 * If we leave things as they are it will reformat the preceding paragraph
 * - again. So we'll have two labels on it.
 * So we'll set the mark to be one-char ahead, which means that ctl-C
 * will reformat the next paragraph, which makes more sense.
 * But note that this will *not* propagate any current paragraph counter!
 */

    struct line *here_dotp = curwp->w_dotp;
    int here_doto = curwp->w_doto;
    forw_grapheme(1);
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;
    curwp->w_dotp = here_dotp;
    curwp->w_doto = here_doto;

    return status;
}

/* Reformat the paragraphs containing the region into a list.
 * This forces the user variable for the labelling to be " o "
 * (the number-formatting in region_listmaker() will have no effect).
 *
 * int f, n;            deFault flag and Numeric argument
 */
int makelist_region(int f, int n) {
    lbl_fmt = regionlist_text;
    int status = region_listmaker(f, n);
    return status;
}

/* Reformat the paragraphs containing the region into a list.
 * This uses the user variable %list-indent-text for the label.
 * This may have AT MOST one numeric variable template.
 *
 * int f, n;            deFault flag and Numeric argument
 */
int numberlist_region(int f, int n) {
    lbl_fmt = regionlist_number;
    int status = region_listmaker(f, n);
    return status;
}
