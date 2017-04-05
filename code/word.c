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
        int cnt;        /* size of word wrapped to next line */
        int c;          /* charector temporary */

        /* backup from the <NL> 1 char */
        if (!backchar(0, 1))
                return FALSE;

        /* back up until we aren't in a word,
           make sure there is a break in the line */
        cnt = 0;
        while (((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ')
               && (c != '\t')) {
                cnt++;
                if (!backchar(0, 1))
                        return FALSE;
                /* if we make it to the beginning, start a new line */
                if (curwp->w_doto == 0) {
                        gotoeol(FALSE, 0);
                        return lnewline();
                }
        }

        /* delete the forward white space */
        if (!forwdel(0, 1))
                return FALSE;

        /* put in a end of line */
        if (!lnewline())
                return FALSE;

        /* and past the first word */
        while (cnt-- > 0) {
                if (forwchar(FALSE, 1) == FALSE)
                        return FALSE;
        }
        return TRUE;
}

/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines. Error if you try to
 * move beyond the buffers.
 */
int backword(int f, int n)
{
        if (n < 0)
                return forwword(f, -n);
        if (backchar(FALSE, 1) == FALSE)
                return FALSE;
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
                while (inword() != FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
        }
        return forwchar(FALSE, 1);
}

/*
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
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

                while (inword() == state1) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                }

                while (inword() == !state1) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
        }
        return TRUE;
}

/*
 * Move the cursor forward by the specified number of words. As you move,
 * convert any characters to upper case. Error if you try and move beyond the
 * end of the buffer. Bound to "M-U".
 */
int upperword(int f, int n)
{
        int c;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if (n < 0)
                return FALSE;
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto);
#if     PKCODE
                        if (islower(c)) {
#else
                        if (c >= 'a' && c <= 'z') {
#endif
                                c -= 'a' - 'A';
                                lputc(curwp->w_dotp, curwp->w_doto, c);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
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
        int c;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if (n < 0)
                return FALSE;
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto);
#if     PKCODE
                        if (isupper(c)) {
#else
                        if (c >= 'A' && c <= 'Z') {
#endif
                                c += 'a' - 'A';
                                lputc(curwp->w_dotp, curwp->w_doto, c);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
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
        int c;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if (n < 0)
                return FALSE;
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                }
                if (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto);
#if     PKCODE
                        if (islower(c)) {
#else
                        if (c >= 'a' && c <= 'z') {
#endif
                                c -= 'a' - 'A';
                                lputc(curwp->w_dotp, curwp->w_doto, c);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                        while (inword() != FALSE) {
                                c = lgetc(curwp->w_dotp, curwp->w_doto);
#if     PKCODE
                                if (isupper(c)) {
#else
                                if (c >= 'A' && c <= 'Z') {
#endif
                                        c += 'a' - 'A';
                                        lputc(curwp->w_dotp, curwp->w_doto,
                                              c);
                                        lchange(WFHARD);
                                }
                                if (forwchar(FALSE, 1) == FALSE)
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

        /* get us into a word.... */
        while (inword() == FALSE) {
                if (forwchar(FALSE, 1) == FALSE)
                        return FALSE;
                ++size;
        }

        if (n == 0) {
                /* skip one word, no whitespace! */
                while (inword() == TRUE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return FALSE;
                        ++size;
                }
        } else {
                /* skip n words.... */
                while (n--) {

                        /* if we are at EOL; skip to the beginning of the next */
                        while (curwp->w_doto == llength(curwp->w_dotp)) {
                                if (forwchar(FALSE, 1) == FALSE)
                                        return FALSE;
                                ++size;
                        }

                        /* move forward till we are at the end of the word */
                        while (inword() == TRUE) {
                                if (forwchar(FALSE, 1) == FALSE)
                                        return FALSE;
                                ++size;
                        }

                        /* if there are more words, skip the interword stuff */
                        if (n != 0)
                                while (inword() == FALSE) {
                                        if (forwchar(FALSE, 1) == FALSE)
                                                return FALSE;
                                        ++size;
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

        if (backchar(FALSE, 1) == FALSE)
                return FALSE;
        size = 0;
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return FALSE;
                        ++size;
                }
                while (inword() != FALSE) {
                        ++size;
                        if (backchar(FALSE, 1) == FALSE)
                                goto bckdel;
                }
        }
        if (forwchar(FALSE, 1) == FALSE)
                return FALSE;

/* GGR
 * We have to cater for the function being called multiple times in
 * succession. This should be the equivalent of Esc<n>delbword, i.e. the
 * text can be yanked back and still all be in the same order as it was
 * originally.
 * This means that we have to fiddle with the kill ring buffers.
 */
bckdel:
    if ((kp = kbufh) != NULL) {
        kbufh = kbufp = NULL;
        ku = kused;
        kused = KBLOCK;
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
 */
int inword(void)
{
        int c;

        if (curwp->w_doto == llength(curwp->w_dotp))
                return FALSE;
        c = lgetc(curwp->w_dotp, curwp->w_doto);
#if     PKCODE
        if (isletter(c))
#else
        if (c >= 'a' && c <= 'z')
                return TRUE;
        if (c >= 'A' && c <= 'Z')
#endif
                return TRUE;
        if (c >= '0' && c <= '9')
                return TRUE;
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

/* Space factor needed for a unicode array copied from a char array */
#define MFACTOR sizeof(unicode_t)/sizeof(char)

/*
 * Fill the current paragraph according to the current
 * fill column
 *
 * f and n - deFault flag and Numeric argument
 */
int fillpara(int f, int n)
{
        unicode_t c;            /* current char during scan    */
        unicode_t *wbuf       ; /* buffer for current word     */
        size_t wbuf_ents;       /* Number of elements needed.. */
        int *space_ind;         /* Where the spaces are */
        int nspaces;            /* How many spaces there are */

        int wordlen;            /* length of current word       */
        int clength;            /* position on line during fill */
        int i;                  /* index during word copy       */
        int newlength;          /* tentative new line length    */
        int eopflag;            /* Are we at the End-Of-Paragraph? */
        int firstflag;          /* first word? (needs no space) */
        struct line *eopline;   /* pointer to line just past EOP */
        int eosflag;            /* was the last char a period?  */
        int trailing_eos;       /* Was last event to add an eos space? */
/* GGR added vars - to allow for padding to right-justify */
        int gap;        /* number of spaces to pad with */
        int rtol;       /* Pass direction */
        int startline, endline;

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if (fillcol == 0) {     /* no fill column set */
                mlwrite("No fill column set");
                return FALSE;
        }
#if     PKCODE
        justflag = FALSE;
#endif
/* GGR */
        rtol = 1;           /* Direction of first padding pass */

        /* record the pointer to the line just past the EOP */
        gotoeop(FALSE, 1);
        eopline = lforw(curwp->w_dotp);
/* GGR - if empty buffer we can get caught, so... */
        endline = getcline();

        /* and back top the beginning of the paragraph */
        gotobop(FALSE, 1);
/* GGR - ...check for the results of such a state */
        startline = getcline();
        if (endline < startline)
                return(TRUE);

        /* initialize various info */
        clength = curwp->w_doto;
        if (clength && curwp->w_dotp->l_text[0] == TAB)
                clength = 8;
        wordlen = 0;
        eosflag = FALSE;
        trailing_eos = 0;

        /* scan through lines, filling words */
        firstflag = TRUE;
        eopflag = FALSE;

/* GGR
 * wbuf needs to be sufficiently long to contain all of the longest
 * line (plus 1, for luck...) in case there is no word-break in it.
 * So we allocate it dynamically.
 */
        wbuf_ents = llength(curwp->w_dotp) + 1;
        wbuf = malloc(MFACTOR*wbuf_ents);
        if (n > 1) {            /* We've been asked to right-justify */
                space_ind = malloc(sizeof(int)*(wbuf_ents+1)/2);
        }
        else {
                space_ind = NULL;   /* no right-justify */
        }
        nspaces = 0;
        while (!eopflag) {
                int bytes = 1;

                /* get the next character in the paragraph */
                if (curwp->w_doto == llength(curwp->w_dotp)) {
                        c = ' ';
                        if (lforw(curwp->w_dotp) == eopline)
                                eopflag = TRUE;
/* GGR  - Reallocate buffer if more space needed.
 *        Remember we are using unicode_t, while llength is char
 *        Make sure we check the *next* line length
 */
                        if (llength(lforw(curwp->w_dotp)) >= wbuf_ents) {
                            wbuf_ents = llength(lforw(curwp->w_dotp)) + 1;
                            wbuf = realloc(wbuf, MFACTOR*wbuf_ents);
                            if (space_ind) {    /* We're padding */
                                space_ind = realloc(space_ind, sizeof(int)*(wbuf_ents+1)/2);
                            }
                        }
                } else
                        bytes = lgetchar(&c);

                /* and then delete it */
                ldelete(bytes, FALSE);

                /* if not a separator, just add it in */
                if (c != ' ' && c != '\t') {
/* GGR - Handle any defined punctuation characters */
                        eosflag = 0;
                        if (eos_list) {     /* Some eos defined */
                                for (unicode_t *eosch = eos_list;
                                    *eosch != END_UCLIST; eosch++) {
                                        if (c == *eosch) {
                                                eosflag = 1;
                                                break;
                                        }
                                }
                        }

/* GGR - dynamically sized, so this assignment can always be done */
                        wbuf[wordlen++] = c;
                } else if (wordlen) {
                        /* at a word break with a word waiting */
                        /* calculate tentative new length with word added */
                        newlength = clength + 1 + wordlen;
                        if (newlength <= fillcol) {
                                /* add word to current line */
                                if (!firstflag) {
                                        if (space_ind) { /* GGR */
/* We need to save the doto location, not clength, to allow for unicode */
                                                space_ind[nspaces] = curwp->w_doto;
                                                nspaces++;
                                        }
                                        linsert(1, ' ');        /* the space */
                                        ++clength;
                                }
                                firstflag = FALSE;
                        } else {

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
/* GGR - implement Esc n Esc q - justify right edges too..       */
/* Now written in a manner to handle unicode chars....           */
/* We can only do it if we've actually found some spaces         */

if (nspaces) {
/* Remember where we are on the line.
 * Also, was the last char added to line a space after a dot?
 * Ignore if so.
 */
    int end_doto = curwp->w_doto;
    gap = fillcol - clength;    /* How much we need to add (in columns) */
    if (trailing_eos) {         /* Back one char and increase gap */
        end_doto--;
        gap++;
        trailing_eos = 0;       /* We'll remove it... */
    }
    while (gap > 0) {
        if (rtol) for (int ns = nspaces-1; ns >= 0; ns--) {
            curwp->w_doto = space_ind[ns];
            linsert(1, ' ');
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
            linsert(1, ' ');
            end_doto++;     /* A space is one byte... */
            rtol = 1;       /* So next pass goes other way */
            if (--gap <= 0) break;
            for (int ni = ns; ni < nspaces; ni++) {
                space_ind[ni]++;
            }
        }
    }
/* Fix up our location in the line to be where we were, plus added spaces */
    curwp->w_doto = end_doto;
}
/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
/* Possibly start a new line... */
                                if (!firstflag) {   /* GGR */
                                    if (trailing_eos) backdel(FALSE, 1);
                                    lnewline();
                                }
                                firstflag = FALSE;
                                clength = 0;
                                nspaces = 0;
                        }
/* ...and add the word in in either case */
                        for (i = 0; i < wordlen; i++) {
                                linsert(1, wbuf[i]);
                                ++clength;
                        }
                        wordlen = 0;

/* Add any requested trailing eos space, if there is room
 */
                        if (eosflag && clength < fillcol) {
                                linsert(1, ' ');
                                ++clength;
                                trailing_eos = 1;
                        }
                        else trailing_eos = 0;
                }
        }
        /* If we arrive at eop with trailing_eos set, remove the space */
        if (trailing_eos) backdel(FALSE, 1);

        /* and add a last newline for the end of our new paragraph */
        lnewline();
        free(wbuf);     /* GGR - mustn't forget these... */
        if (space_ind) free(space_ind);
        return TRUE;
}

#if     PKCODE
/* Fill the current paragraph according to the current
 * fill column and cursor position
 *
 * int f, n;            deFault flag and Numeric argument
 */
int justpara(int f, int n)
{
        unicode_t c;            /* current char durring scan    */
        unicode_t *wbuf;        /* buffer for current word      */
        size_t wbuf_ents;
        int wordlen;            /* length of current word       */
        int clength;            /* position on line during fill */
        int i;                  /* index during word copy       */
        int newlength;          /* tentative new line length    */
        int eopflag;            /* Are we at the End-Of-Paragraph? */
        int firstflag;          /* first word? (needs no space) */
        struct line *eopline;   /* pointer to line just past EOP */
        int leftmarg;           /* left marginal */
        int startline, endline;
        int eosflag;            /* was the last char a period?  */
        int trailing_eos;       /* Was last event to add an eos space? */

        if (curbp->b_mode & MDVIEW)     /* don't allow this command if  */
                return rdonly();        /* we are in read only mode     */
        if (fillcol == 0) {             /* no fill column set */
                mlwrite("No fill column set");
                return FALSE;
        }
/* GGR - have to cater for tabs...can't use w_doto */
        leftmarg = getccol(FALSE);
        if (leftmarg + 10 > fillcol) {
                mlwrite("Column too narrow");
                return FALSE;
        }

        /* record the pointer to the line just past the EOP */
        gotoeop(FALSE, 1);
        eopline = lforw(curwp->w_dotp);
/* GGR - if empty buffer we can get caught, so... */
        endline = getcline();

        /* and back top the beginning of the paragraph */
        gotobop(FALSE, 1);
/* GGR - ...check for the results of such a state */
        startline = getcline();
        if (endline < startline)
                return(TRUE);

/* GGR - need to get rid of any leading whitespace, then pad first line
 * here, at bop */
        (void)whitedelete(1, 1);    /* Don't care whether there was any */
        curwp->w_doto = 0;          /* If bop == bof, would be 1. (!?!) */
        for (i = 0; i < leftmarg; i++)
                linsert(1, ' ');
        curwp->w_doto = clength = leftmarg;

/* GGR
 * wbuf needs to be sufficiently long to contain all of the longest
 * line (plus 1, for luck...) in case there is no word-break in it.
 */
        wbuf_ents = llength(curwp->w_dotp) + 1;
        wbuf = malloc(MFACTOR*wbuf_ents);
        eosflag = FALSE;
        trailing_eos = 0;

        wordlen = 0;

        /* scan through lines, filling words */
        firstflag = TRUE;
        eopflag = FALSE;
        while (!eopflag) {
                int bytes = 1;

                /* get the next character in the paragraph */
                if (curwp->w_doto == llength(curwp->w_dotp)) {
                        c = ' ';
                        if (lforw(curwp->w_dotp) == eopline)
                                eopflag = TRUE;
/* GGR  - Reallocate buffer if more space needed
 *        Make sure we check the *next* line length
 */
                        if (llength(lforw(curwp->w_dotp)) >= wbuf_ents) {
                            wbuf_ents = llength(lforw(curwp->w_dotp)) + 1;
                            wbuf = realloc(wbuf, MFACTOR*wbuf_ents);
                        }
                } else
                        bytes = lgetchar(&c);

                /* and then delete it */
                ldelete(bytes, FALSE);

                /* if not a separator, just add it in */
                if (c != ' ' && c != '\t') {
/* GGR - Handle any defined punctuation characters */
                        eosflag = 0;
                        if (eos_list) {     /* Some eos defined */
                                for (unicode_t *eosch = eos_list;
                                    *eosch != END_UCLIST; eosch++) {
                                        if (c == *eosch) {
                                                eosflag = 1;
                                                break;
                                        }
                                }
                        }

/* GGR - dynamically sized, so this assignment can always be done */
                        wbuf[wordlen++] = c;
                } else if (wordlen) {
                        /* at a word break with a word waiting */
                        /* calculate tentitive new length with word added */
                        newlength = clength + 1 + wordlen;
                        if (newlength <= fillcol) {
                                /* add word to current line */
                                if (!firstflag) {
                                        linsert(1, ' ');    /* the space */
                                        ++clength;
                                }
                                firstflag = FALSE;
                        } else {
                                /* start a new line */
                                if (trailing_eos) backdel(FALSE, 1);
                                lnewline();
                                for (i = 0; i < leftmarg; i++)
                                        linsert(1, ' ');
                                clength = leftmarg;
                        }

                        /* and add the word in in either case */
                        for (i = 0; i < wordlen; i++) {
                                linsert(1, wbuf[i]);
                                ++clength;
                        }
                        if (eosflag && clength < fillcol) {  /* GGR added */
                                linsert(1, ' ');
                                ++clength;
                                trailing_eos = 1;
                        }
                        else trailing_eos = 0;
                        wordlen = 0;
                }
        }
        /* If we arrive at eop with trailing_eos set, remove the space */
        if (trailing_eos) backdel(FALSE, 1);

        /* and add a last newline for the end of our new paragraph */
        lnewline();

        forwword(FALSE, 1);
        if (llength(curwp->w_dotp) > leftmarg)
                curwp->w_doto = leftmarg;
        else
                curwp->w_doto = llength(curwp->w_dotp);

        free(wbuf);     /* GGR - mustn't forget this... */
        return TRUE;
}
#endif

/*
 * delete n paragraphs starting with the current one
 *
 * int f        default flag
 * int n        # of paras to delete
 */
int killpara(int f, int n)
{
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
#if     PKCODE
                                   (isletter(ch)) ||
#else
                                   (ch >= 'a' && ch <= 'z') ||
                                   (ch >= 'A' && ch <= 'Z') ||
#endif
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
 * Actually we'll allocate one extra an put an illegal value at the end.
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
