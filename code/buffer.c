/*      buffer.c
 *
 *      Buffer management.
 *      Some of the functions are internal,
 *      and some are actually attached to user
 *      keys. Like everyone else, they set hints
 *      for the display system
 *
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <unistd.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/*
 * Attach a buffer to a window. The values of dot and mark come from
 * the buffer if the use count is 0. Otherwise, they come from some
 * other window.
 * If *any* numeric arg is given, use the savnam buffer directly with
 * no prompting.
 * This command is disabled in the minibuffer (names in names.c)
 */
int usebuffer(int f, int n) {
    UNUSED(n);
    struct buffer *bp;
    int s;
    char bufn[NBUFN];

    if (f) strcpy(bufn, savnam);
    else
/* GGR - handle saved buffer name in minibuffer */
        if ((s = mlreply("Use buffer: ", bufn, NBUFN, CMPLT_BUF)) != TRUE) {
            if (s != ABORT) strcpy(bufn, savnam);
            else                     return(s);
        }

    if ((bp = bfind(bufn, TRUE, 0)) == NULL) return FALSE;
    return swbuffer(bp, 0);
}

/*
 * switch to the next buffer in the buffer list
 * This command is disabled in the minibuffer (names in names.c)
 *
 * int f, n;            default flag, numeric argument
 */
int nextbuffer(int f, int n) {
    struct buffer *bp;          /* test buffer pointer */
    struct buffer *bbp;         /* eligible buffer to switch to */

/* Make sure the arg is legit */
    if (f == FALSE) n = 1;

/* We now allow -ve changes. Since we only have forward pointers this is
 * implemented by counting the valid buffers and taking the (+ve) modulus.
 */
    if (n < 1) {
        int nb = 0;
        for (bp = bheadp; bp != NULL; bp = bp->b_bufp)
            if (!(bp->b_flag & BFINVS)) nb++;
        n %= nb;
        if (n == 0) return FALSE;
        if (n < 0) n += nb;     /* Ensure it is +ve - modulus does not */
    }

    bbp = curbp;                /* Start here */
    while (n-- > 0) {
        bp = bbp->b_bufp? bbp->b_bufp: bheadp;  /* Next buffer */
        while (bp->b_flag & BFINVS) {           /* Skip ineligible ones */
            bp = bp->b_bufp? bp->b_bufp: bheadp;
            if (bp == bbp)  return FALSE;       /* No infinite loops! */
        }
        bbp = bp;                               /* Next eligible one */
    }
    return swbuffer(bbp, 0);
}

/*
 * Make a buffer active...by reading in (possibly delayed) a file.
 */
static void make_active(struct buffer *nbp) {

/* Set this now to avoid a potential loop if a file-hook prompts */
    nbp->b_active = TRUE;
    nbp->b_mode |= gmode;       /* readin() runs file-hooks, so set now */
/* Read it in and activate it */
    run_filehooks = 1;          /* set flag */
    struct buffer *real_curbp = curbp;
    curbp = nbp;
    readin(nbp->b_fname, TRUE);
    curbp = real_curbp;
    nbp->b.dotp = lforw(nbp->b_linep);
    nbp->b.doto = 0;
    return;
}

/*
 * make buffer BP current
 */
int swbuffer(struct buffer *bp, int macro_OK) {
    struct window *wp;

/* Check for NULL bp... */
    if (bp == NULL) return FALSE;

/* We must not switch to the keyboard macro buffer when collecting a
 * macro, as that would allow the user to move around in the buffer...
 */
    if (kbdmode == RECORD && !macro_OK &&
          strcmp(bp->b_bname, kbdmacro_buffer) == 0) {
        mlwrite("Can't switch to %s when collecting macro", kbdmacro_buffer);
        sleep(2);
        return FALSE;
    }

/* Save last name so we can switch back to it on empty MB reply */
    if (!inmb && do_savnam) strcpy(savnam, curbp->b_bname);

    if (--curbp->b_nwnd == 0) curbp->b = curwp->w;  /* Last use */
    curbp = bp;                     /* Switch. */
    if (curbp->b_active != TRUE)    /* buffer not active yet */
        make_active(curbp);

    curwp->w_bufp = bp;
    curwp->w_linep = bp->b_linep;   /* For macros, ignored. */
    curwp->w_flag |= WFMODE | WFFORCE | WFHARD;     /* Quite nasty. */
    if (bp->b_nwnd++ == 0) {        /* First use.           */
        curwp->w = bp->b;
        cknewwindow();
        return TRUE;
    }
    wp = wheadp;                    /* Look for old.        */
    while (wp != NULL) {
        if (wp != curwp && wp->w_bufp == bp) {
            curwp->w = wp->w;
            break;
        }
        wp = wp->w_wndp;
    }
    cknewwindow();
    return TRUE;
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too
 * upset if it isn't there at all!). Get quite upset
 * if the buffer is being displayed. Clear the buffer (ask
 * if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
int killbuffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct buffer *bp;
    int s;
    char bufn[NBUFN];

    if ((s = mlreply("Kill buffer: ", bufn, NBUFN, CMPLT_BUF)) != TRUE)
        return s;
    if ((bp = bfind(bufn, FALSE, 0)) == NULL)   /* Easy if unknown. */
        return TRUE;
/* We usually ignore deletion of invisible buffers, but we're actually
 * happy to allow deletion of "/xxx" procedure buffers (it allows you to
 * set up "delete-after-use" ones), but still want to ignore special
 * buffers starting "//".  (These can be zotbuf()ed internally).
 */
    if (bp->b_flag & BFINVS) {
        if ((bufn[0] == '/') && (bufn[1] != '/'))
            ;
        else
            return TRUE;            /* by doing nothing.    */
    }
    return zotbuf(bp);
}

/*
 * kill the buffer pointed to by bp
 */
int zotbuf(struct buffer *bp) {
    struct buffer *bp1;
    struct buffer *bp2;
    int s;

    if (bp->b_nwnd != 0) {          /* Error if on screen.  */
        mlwrite_one("Buffer is being displayed");
        return FALSE;
    }
    if ((s = bclear(bp)) != TRUE)   /* Blow text away.      */
        return s;
    Xfree((char *) bp->b_linep);     /* Release header line. */
    bp1 = NULL;                     /* Find the header.     */
    bp2 = bheadp;
    while (bp2 != bp) {
        bp1 = bp2;
        bp2 = bp2->b_bufp;
    }
    bp2 = bp2->b_bufp;              /* Next one in chain.   */
    if (bp1 == NULL)                /* Unlink it.           */
        bheadp = bp2;
    else
        bp1->b_bufp = bp2;
    Xfree((char *) bp);              /* Release buffer block */
    return TRUE;
}

/*
 * Rename the current buffer
 *
 * int f, n;            default Flag & Numeric arg
 */
int namebuffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct buffer *bp;      /* pointer to scan through all buffers */
    char bufn[NBUFN];       /* buffer to hold buffer name */

/* Prompt for and get the new buffer name */
ask:
    if (mlreply("Change buffer name to: ", bufn, NBUFN, CMPLT_BUF) != TRUE)
        return FALSE;

/* And check for duplicates */
    bp = bheadp;
    while (bp != NULL) {
        if (bp != curbp) {
            if (strcmp(bufn, bp->b_bname) == 0) /* If the names the same */
                goto ask;       /* try again */
        }
        bp = bp->b_bufp;        /* onward */
    }

    strcpy(curbp->b_bname, bufn);   /* copy buffer name to structure */
    curwp->w_flag |= WFMODE;        /* make mode line replot */
    mlerase();
    return TRUE;
}

/* This is being put into a 9-char (+NL) field, but could actually
 * have 10 chars...
 */
static void ltoa(char *buf, int width, long num) {

    buf[width] = 0;                 /* End of string.       */
    while (num >= 10 && width) {    /* Conditional digits.  */
        buf[--width] = (int) (num % 10L) + '0';
        num /= 10L;
    }
    if (width) buf[--width] = (int) num + '0';  /* If room */
    else buf[0] = '+';                          /* if not */
    while (width != 0)      /* Pad with blanks.     */
        buf[--width] = ' ';
}

/*
 * The argument "text" points to a string. Append this line to the
 * buffer list buffer. Handcraft the EOL on the end.
 * Return TRUE if it worked and FALSE if you ran out of room.
 */
static int addline(char *text) {
    struct line *lp;
    int ntext;

    ntext = strlen(text);
    if ((lp = lalloc(ntext)) == NULL) return FALSE;
    lfillchars(lp, ntext, text);
    blistp->b_linep->l_bp->l_fp = lp;       /* Hook onto the end    */
    lp->l_bp = blistp->b_linep->l_bp;
    blistp->b_linep->l_bp = lp;
    lp->l_fp = blistp->b_linep;
    if (blistp->b.dotp == blistp->b_linep)  /* If "." is at the end */
        blistp->b.dotp = lp;                /* move it to new line  */
    return TRUE;
}

/*
 * This routine rebuilds the text in the special secret buffer
 * that holds the buffer list.
 * It is called by the list buffers command.
 * Return TRUE if everything works.
 * Return FALSE if there is an error (if there is no memory).
 * Iflag indicates wether to list hidden buffers.
 *
 * int iflag;           list hidden buffer flag
 */
#define MAXLINE MAXCOL
static int makelist(int iflag) {
    char *cp1;
    char *cp2;
    int c;
    struct buffer *bp;
    struct line *lp;
    int s;
    int i;
    long nbytes;                        /* # of bytes in current buffer */
    char b[9+1];                        /* field width for nbytes + NL */
    char line[MAXLINE];

    blistp->b_flag &= ~BFCHG;           /* Don't complain!      */
    if ((s = bclear(blistp)) != TRUE)   /* Blow old text away   */
        return s;
    strcpy(blistp->b_fname, "");
    if (addline("ACT MODES   Type↴      Size Buffer        File") == FALSE
     || addline("--- ------------.      ---- ------        ----") == FALSE)
        return FALSE;
    bp = bheadp;                        /* For all buffers      */

/* Build line to report global mode settings */

    cp1 = line;
    for (i = 0; i < 4; i++) *cp1++ = ' ';

/* Output the mode codes */

    for (i = 0; i < NUMMODES; i++)
        *cp1++ = (gmode & (1 << i))? modecode[i]: '.';
    strcpy(cp1, ".           Global Modes");
    if (addline(line) == FALSE) return FALSE;

/* Output the list of buffers */

    while (bp != NULL) {
/* Skip invisible buffers if iflag is false */
        if (((bp->b_flag & BFINVS) != 0) && (iflag != TRUE)) {
            bp = bp->b_bufp;
            continue;
        }
        cp1 = line;                 /* Start at left edge   */

/* Output status of ACTIVE flag (has the file been read in? */

        *cp1++ = (bp->b_active == TRUE)? '@': ' ';

/* Output status of changed flag */

        *cp1++ = ((bp->b_flag & BFCHG) != 0)? '*': ' ';

/* Report if the file is truncated */

        *cp1++ = ((bp->b_flag & BFTRUNC) != 0)? '#': ' ';
        *cp1++ = ' ';                   /* space */

/* Output the mode codes */

        for (i = 0; i < NUMMODES; i++)
            *cp1++ = (bp->b_mode & (1 << i))?  modecode[i]: '.';

/* Append p or x to denote whether it is set to BTPHON or BTPROC */
        if (bp->b_type == BTPHON) *cp1++ = 'p';
        else if (bp->b_type == BTPROC) *cp1++ = 'x';
        else *cp1++ = '.';

        *cp1++ = ' ';                   /* Gap.                 */
        nbytes = 0L;                    /* Count bytes in buf.  */
        lp = lforw(bp->b_linep);
        while (lp != bp->b_linep) {
            nbytes += (long) llength(lp) + 1L;
            lp = lforw(lp);
        }
        ltoa(b, 9, nbytes);             /* 9 digit buffer size. */
        cp2 = b;
        while ((c = *cp2++) != 0) *cp1++ = c;
        *cp1++ = ' ';                   /* Gap.                 */
        cp2 = &bp->b_bname[0];          /* Buffer name          */
        while ((c = *cp2++) != 0)
            *cp1++ = c;
        cp2 = &bp->b_fname[0];          /* File name            */
        if (*cp2 != 0) {
/*
 * We know the current screen width, so use it...
 */
            if (((cp1 - line) + strlen(cp2)) > (unsigned)term.t_ncol) {
                *cp1++ = ' ';
                *cp1++ = 0xe2;      /* Carriage return symbol */
                *cp1++ = 0x86;      /* U+2185                 */
                *cp1++ = 0xb5;      /* as utf-8               */
                *cp1 = 0;           /* Add to the buffer.   */
                if (addline(line) == FALSE) return FALSE;
                cp1 = line;
                for (i = 0; i < 5; i++) *cp1++ = ' ';
            }
            else {
/* The header line is 3+1+13+1+9+1+13+1 to get to File */
                while (cp1 < line+42) *cp1++ = ' ';
            }
            while ((c = *cp2++) != 0) {
                if (cp1 < &line[MAXLINE - 1]) *cp1++ = c;
            }
        }
        *cp1 = 0;       /* Add to the buffer.   */
        if (addline(line) == FALSE) return FALSE;
        bp = bp->b_bufp;
    }
    return TRUE;            /* All done             */
}

/*
 * List all of the active buffers.  First update the special
 * buffer that holds the list.  Next make sure at least 1
 * window is displaying the buffer list, splitting the screen
 * if this is what it takes.  Lastly, repaint all of the
 * windows that are displaying the list.  Bound to "C-X C-B".
 *
 * A numeric argument forces it to list invisible buffers as
 * well.
 */
int listbuffers(int f, int n) {
    UNUSED(n);
    struct window *wp;
    struct buffer *bp;
    int s;

    if ((s = makelist(f)) != TRUE) return s;

    if (blistp->b_nwnd == 0) {      /* Not on screen yet.   */
        if ((wp = wpopup()) == NULL) return FALSE;
        bp = wp->w_bufp;
        if (--bp->b_nwnd == 0) bp->b = wp->w;
        wp->w_bufp = blistp;
        ++blistp->b_nwnd;
    }
    wp = wheadp;
    while (wp != NULL) {
        if (wp->w_bufp == blistp) {
            wp->w_linep = lforw(blistp->b_linep);
            wp->w.dotp = lforw(blistp->b_linep);
            wp->w.doto = 0;
            wp->w.markp = NULL;
            wp->w.marko = 0;
            wp->w_flag |= WFMODE | WFHARD;
        }
        wp = wp->w_wndp;
    }
    return TRUE;
}

/*
 * Look through the list of buffers. Return TRUE if there are any
 * changed buffers.
 * Buffers that hold magic internal stuff are not considered; who cares
 * if the list of buffer names is hacked.
 * Return FALSE if no buffers have been changed.
 */
int anycb(void) {
    struct buffer *bp;

    bp = bheadp;
    while (bp != NULL) {
        if ((bp->b_flag & BFINVS) == 0 && (bp->b_flag & BFCHG) != 0)
            return TRUE;
        bp = bp->b_bufp;
    }
    return FALSE;
}

/*
 * Find a buffer, by name. Return a pointer to the buffer structure
 * associated with it.
 * If the buffer is not found and the "cflag" is TRUE, create it.
 * The "bflag" is the settings for the flags in the buffer.
 */
struct buffer *bfind(const char *bname, int cflag, int bflag) {
    struct buffer *bp;
    struct buffer *sb;      /* buffer to insert after */
    struct line *lp;

/* GGR Add a check on the sent namelength, given that we are going
 * to copy it into a structure if we create a new buffer.
 */
    if (strlen(bname) >= NBUFN) {   /* We need a NUL too */
        mlforce("Buffer name too long: %s. Ignored.", bname);
        sleep(1);
        return NULL;
    }
    bp = bheadp;
    while (bp != NULL) {
        if (strcmp(bname, bp->b_bname) == 0) {
            if (bp->b_active != TRUE) { /* buffer not active yet */
                silent = TRUE;          /* As probably not the current buffer */
                make_active(bp);
                silent = FALSE;
            }
            return bp;
        }
        bp = bp->b_bufp;
    }
    if (cflag != FALSE) {
        bp = (struct buffer *)Xmalloc(sizeof(struct buffer));
        if ((lp = lalloc(0)) == NULL) {
            Xfree((char *) bp);
            return NULL;
        }
/* Find the place in the list to insert this buffer */
        if (bheadp == NULL || strcmp(bheadp->b_bname, bname) > 0) {
            bp->b_bufp = bheadp;        /* insert at the beginning */
            bheadp = bp;
        } else {
            sb = bheadp;
            while (sb->b_bufp != NULL) {
                if (strcmp(sb->b_bufp->b_bname, bname) > 0) break;
                sb = sb->b_bufp;
            }

/* And insert it */
            bp->b_bufp = sb->b_bufp;
            sb->b_bufp = bp;
        }

/* And set up the other buffer fields */
        bp->b_topline = NULL;   /* GGR - for widen and  */
        bp->b_botline = NULL;   /* GGR - shrink windows */
        bp->bv = NULL;          /* No vars */
        bp->b_active = TRUE;
        bp->b.dotp = lp;
        bp->b.doto = 0;
        bp->b.markp = NULL;
        bp->b.marko = 0;
        bp->b.fcol = 0;
        bp->b_flag = bflag;
        bp->b_mode = gmode;
        bp->b_nwnd = 0;
        bp->b_linep = lp;
        strcpy(bp->b_fname, "");
        strcpy(bp->b_bname, bname);
        bp->b_key[0] = 0;
        bp->b_keylen = 0;
        bp->b_EOLmissing = 0;
        bp->ptt_headp = NULL;
        bp->b_type = BTNORM;
        bp->b_exec_level = 0;
        lp->l_fp = lp;
        lp->l_bp = lp;
    }
    return bp;
}

/*
 * This routine blows away all of the text in a buffer.
 * If the buffer is marked as changed then we ask if it is OK to blow it away;
 * this is to save the user the grief of losing text.
 * The window chain is nearly always wrong if this gets called; the caller
 * must arrange for the updates that are required.
 * Return TRUE if everything looks good.
 */
int bclear(struct buffer *bp) {
    struct line *lp;
    int s;

    if ((bp->b_flag & BFINVS) == 0      /* Not scratch buffer.  */
        && (bp->b_flag & BFCHG) != 0    /* Something changed    */
        && (s = mlyesno("Discard changes")) != TRUE)
            return s;
    bp->b_flag &= ~BFCHG;               /* Not changed          */

/* If we are modfying the buffer that the match-group info points
 * to we have to mark them as invalid.
 */
    if (bp == group_match_buffer) group_match_buffer = NULL;

/* If the buffer is narrowed we must widen it first to ensure we free
 * all lines - not just those from the narrowed region.
 */
    if (bp->b_flag & BFNAROW) {
        curwp->w_bufp = bp;             /* Ensure this is current */
        widen(0, -1);                   /* -1 == no reposition */
        bp = curwp->w_bufp;
    }

    while ((lp = lforw(bp->b_linep)) != bp->b_linep) lfree(lp);

    bp->b.dotp = bp->b_linep;           /* Fix "."              */
    bp->b.doto = 0;
    bp->b.markp = NULL;                 /* Invalidate "mark"    */
    bp->b.marko = 0;
    bp->b.fcol = 0;

/* If we are clearing a buffer that had variables defined then we
 * need to free those.
 */
    if (bp->bv) {
        Xfree(bp->bv);
        bp->bv = NULL;
    }
    return TRUE;
}

/*
 * unmark the current buffers change flag
 *
 * int f, n;            unused command arguments
 */
int unmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    curbp->b_flag &= ~BFCHG;
    curwp->w_flag |= WFMODE;
    return TRUE;
}
