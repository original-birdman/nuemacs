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

#define BUFFER_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

int set_buffer_name(const char *bufn) {

/* Provided no buffer other than curbp has this name, set it for curbp.
 * Return whether the name was set.
 */
    int status = TRUE;
    for (struct buffer *bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if (bp != curbp) {
            if (strcmp(bufn, bp->b_bname) == 0) { /* Names the same? */
                status = FALSE;
                break;
            }
        }
    }
    if (status) update_val(curbp->b_bname, bufn);
    return status;
}

/* -+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+- */

/* This routine blows away all of the text in a buffer.
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
        struct buffer *obp = curwp->w_bufp;
        curwp->w_bufp = bp;             /* Ensure this buf is current */
        widen(0, -1);                   /* -1 == no reposition */
        curwp->w_bufp = obp;            /* Restore original buf */
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
    if (bp->bv) {       /* Must free the values too... */
        for (int vnum = 0; vnum < BVALLOC; vnum++) {
            if (bp->bv[vnum].name[0] == '\0') break;
            Xfree(bp->bv[vnum].value);
        }
        Xfree_setnull(bp->bv);
    }

/* If it's a Phonetic Translation table, remove that too. */

    if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);

    return TRUE;
}

/* Remove any entries from the list referring to buffer */

static void per_macro_level_remove(struct buffer *btogo) {
    if (!macro_pin_headp) return;  /* None there */
    linked_items **prev_p = &macro_pin_headp;
    linked_items *mp = macro_pin_headp;
    while (mp) {
        linked_items *next_p = mp->next;
        if (mmi(mp, bp) == btogo) {
            Xfree(mp);
            (*prev_p)->next = next_p;
        }
        mp = next_p;
        *prev_p = (*prev_p)->next;
    }
}

/* kill the buffer pointed to by bp
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
    Xfree(bp->b_linep);             /* Release header line (no l_text here) */
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

/* Free allocated fields */
    Xfree(bp->b_bname);
    Xfree(bp->b_dfname);
    Xfree(bp->b_rpname);
    Xfree(bp->b_key);

/* Release buffer block */
    Xfree(bp);

/* Remove any per-macro-level marks */
    per_macro_level_remove(bp);
    return TRUE;
}

/* Make a buffer active...by reading in (possibly delayed) a file.
 */
static void make_active(struct buffer *nbp) {

/* Set this now to avoid a potential loop if a file-hook prompts */
    nbp->b_active = TRUE;
    nbp->b_mode |= gmode;       /* readin() runs file-hooks, so set now */
/* Read it in and activate it */
    run_filehooks = 1;          /* set flag */
    struct buffer *real_curbp = curbp;
    curbp = nbp;
    readin(nbp->b_rpname, TRUE);
/* Set any buffer modes that are forced.
 * This comes *after* file-hooks.
 */
    if (force_mode_on) nbp->b_mode |= force_mode_on;
    if (force_mode_off) nbp->b_mode &= ~force_mode_off;
    curbp = real_curbp;
    nbp->b.dotp = lforw(nbp->b_linep);
    nbp->b.doto = 0;
    return;
}

/* make buffer BP current
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
    if (!inmb && do_savnam) db_set(savnam, curbp->b_bname);

    if (--curbp->b_nwnd == 0) curbp->b = curwp->w;  /* Last use */
    curbp = bp;                     /* Switch. */
    if (curbp->b_active != TRUE)    /* buffer not active yet */
        make_active(curbp);

    curwp->w_bufp = bp;
    curwp->w_linep = bp->b_linep;   /* For macros, ignored. */
    curwp->w_flag |= WFMODE | WFFORCE | WFHARD;     /* Quite nasty. */
    if (bp->b_nwnd++ == 0) {        /* First use.           */
        curwp->w = bp->b;
    }
    else {
        for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
            if (wp != curwp && wp->w_bufp == bp) {
                curwp->w = wp->w;
                break;
            }
        }
    }
    cknewwindow();
    return TRUE;
}

/* A template struct buffer for new buffers */

static struct buffer buf_templ = {
    NULL, NULL, NULL, NULL, NULL, NULL, /* structs... */
    NULL, NULL, NULL, NULL,             /* char *s */
    { NULL, NULL, 0, 0, 0 },            /* struct locs */
    { 0, 0, 0, 0, 0, 0 },               /* struct func_opts */
    BTNORM, 0, 0, 0, 0,                 /* ints */
    TRUE, 0, 0                          /* chars */
};

/* Find a buffer, by name. Return a pointer to the buffer structure
 * associated with it.
 * If the buffer is not found and the "cflag" is TRUE, create it.
 * The "bflag" is the settings for the flags in the buffer.
 */
struct buffer *bfind(const char *bname, int cflag, int bflag) {
    struct buffer *bp;
    struct buffer *sb;      /* buffer to insert after */
    struct line *lp;

    for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if (strcmp(bname, bp->b_bname) == 0) {
            if (bp->b_active != TRUE) { /* buffer not active yet */
/* silent was set to TRUE around this at one point, but it's been
 * removed as it makes sense to show the file being read in for
 * the first time.
 */
                make_active(bp);
            }
            return bp;
        }
    }
    if (cflag != FALSE) {
        bp = (struct buffer *)Xmalloc(sizeof(struct buffer));
        *bp = buf_templ;

/* Find the place in the list to insert this buffer */
        if (bheadp == NULL) {           /* Insert at the beginning */
            bp->b_bufp = bheadp;
            bheadp = bp;
        } else {                        /* Find lexical place... */
/* Note that we test the buffer *following* the current sb */
            for (sb = bheadp; sb->b_bufp != NULL; sb = sb->b_bufp) {
                if (strcmp(sb->b_bufp->b_bname, bname) > 0) break;
            }
            bp->b_bufp = sb->b_bufp;    /* ...and insert it */
            sb->b_bufp = bp;
        }

/* Now override the "default" values in the template*/

        lp = lalloc();      /* Head record has no text buffer */
        bp->b.dotp = lp;
        bp->b_linep = lp;
        lp->l_fp = lp;
        lp->l_bp = lp;

        bp->b_nwnd = 0;
        bp->b_flag = bflag;
        bp->b_mode = gmode;
/* Set any forced buffer modes at create time - after global mode set */
        if (force_mode_on) bp->b_mode |= force_mode_on;
        if (force_mode_off) bp->b_mode &= ~force_mode_off;
        update_val(bp->b_bname, bname);
        update_val(bp->b_dfname, "");
        update_val(bp->b_rpname, "");
    }
    return bp;
}

/* Attach a buffer to a window. The values of dot and mark come from
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
    const char *fbuf;

    db_strdef(bufn);

    if (f) fbuf = db_val(savnam);
    else {
/* GGR - handle saved buffer name in minibuffer */
        if ((s = mlreply("Use buffer: ", &bufn, CMPLT_BUF)) != TRUE) {
            if (s == ABORT) goto exit;
            fbuf = db_val(savnam);    /* Null reply == use saved name */
        }
        else fbuf = db_val(bufn);
    }
    s = ((bp = bfind(fbuf, TRUE, 0)) != NULL);

exit:
    db_free(bufn);
    return (s == TRUE)? swbuffer(bp, 0): s;
}

/* switch to the next buffer in the buffer list
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

/* Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get upset if it isn't there at all).
 * Get quite upset if the buffer is being displayed!
 * Clear the buffer (ask, if the buffer has been changed).
 * Then free the header line and the buffer header.
 * Bound to "C-X K".
 */
int killbuffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    struct buffer *bp;
    int s;

    db_strdef(bufn);

    if ((s = mlreply("Kill buffer: ", &bufn, CMPLT_BUF)) != TRUE)
        goto exit;
    if ((bp = bfind(db_val(bufn), FALSE, 0)) == NULL) { /* Easy if unknown. */
        s = TRUE;
        goto exit;
    }
/* We usually ignore deletion of invisible buffers, but we're actually
 * happy to allow deletion of "/xxx" procedure buffers (it allows you to
 * set up "delete-after-use" ones), but still want to ignore special
 * buffers starting "//".  (These can be zotbuf()ed internally).
 */
    if (bp->b_flag & BFINVS) {
        if ((db_charat(bufn, 0) == '/') &&
            (db_charat(bufn, 1) != '/'))
            ;
        else {
            s = TRUE;   /* By doing nothing.    */
            goto exit;
        }
    }
    s = zotbuf(bp);

exit:
    db_free(bufn);
    return s;
}

/* Rename the current buffer
 *
 * int f, n;            default Flag & Numeric arg
 */
int namebuffer(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;

    db_strdef(bufn);
/* Prompt for and get the new buffer name */
ask:
    if (mlreply("Change buffer name to: ", &bufn, CMPLT_BUF) != TRUE) {
        status = FALSE;
        goto exit;
    }

/* And check for duplicates */
    if (!set_buffer_name(db_val(bufn))) {
        mlforce("%s already exists!", db_val(bufn));
        sleep(1);
        goto ask;       /* Try again */
    }

    update_val(curbp->b_bname, db_val(bufn));   /* Copy name to structure */
    curwp->w_flag |= WFMODE;                    /* Make mode line replot */
    mlerase();
    status = TRUE;

exit:
    db_free(bufn);
    return status;
}

/* The argument "text" points to a string.
 * Append this line to the given buffer.
 * Handcraft the EOL on the end.
 * It is the CALLER'S responsibility to know that bp is a valid
 * buffer pointer.
 * ALSO the caller MUST NOT PUT ANY NEWLINE IN THE TEXT!!
 */
void addline_to_anyb(const char *text, struct buffer *bp) {
    struct line *lp;
    int ntext;

    ntext = strlen(text);
    lp = lalloc();
    db_setn(ldb(lp), text, ntext);
    bp->b_linep->l_bp->l_fp = lp;       /* Hook onto the end    */
    lp->l_bp = bp->b_linep->l_bp;
    bp->b_linep->l_bp = lp;
    lp->l_fp = bp->b_linep;
    if (bp->b.dotp == bp->b_linep)      /* If "." is at the end, move it */
        bp->b.dotp = lp;                /* to new line (doto will be 0)  */
    return;
}
/* This is for internal use to add to the //List buffer. */
#define addline(text) addline_to_anyb(text, blistp)

/* This routine rebuilds the text in the special secret buffer
 * that holds the buffer list.
 * It is called by the list buffers command.
 * Return TRUE if everything works.
 * Return FALSE if there is an error (if there is no memory).
 * Iflag indicates wether to list hidden buffers.
 *
 * int iflag;           list hidden buffer flag
 */
static int makelist(int iflag) {
    char *cp1;
    char *cp2;
    int c;
    struct buffer *bp;
    struct line *lp;
    int s;
    int i;
    ue64I_t nbytes;     /* # of bytes in current buffer */
    int mcheck;

    char *line = Xmalloc(term.t_mcol);

    blistp->b_flag &= ~BFCHG;           /* Don't complain!      */
    if ((s = bclear(blistp)) != TRUE)   /* Blow old text away   */
        return s;
    terminate_str(blistp->b_dfname);    /* Makes it empty */

    addline("ACT MODES   Type↴      Size Buffer        File");
    addline("--- ------------.      ---- ------        ----");

    bp = bheadp;                        /* For all buffers      */

/* Build line to report global mode settings */

    cp1 = line;
    for (i = 0; i < 4; i++) *cp1++ = ' ';

/* Output the mode codes */

    mcheck = 1;
    for (i = 0; i < NUMMODES; i++) {
        *cp1++ = (gmode & mcheck)? modecode[i]: '.';
        mcheck <<= 1;
    }
    strcpy(cp1, ".           Global Modes");
    addline(line);

/* Build line to report any mode settings forced on/off */

    if (force_mode_on || force_mode_on) {
        cp1 = line;
        for (i = 0; i < 4; i++) *cp1++ = ' ';

/* Output the mode codes */

        mcheck = 1;
        for (i = 0; i < NUMMODES; i++) {
            char cset;
            if (force_mode_on & mcheck) cset = modecode[i];
            else if (force_mode_off & mcheck) cset = DIFCASE | modecode[i];
            else cset = '-';
            *cp1++ = cset;
            mcheck <<= 1;
        }
        strcpy(cp1, ".           Forced modes (U==on, l==off)");
        addline(line);
    }

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

/* Output the mode codes - unknown for not-yet-active buffers */

        mcheck = 1;
        char mc = '-';  /* Will stay as this for not-yet-active) */
        for (i = 0; i < NUMMODES; i++) {
            if (bp->b_active) mc = (bp->b_mode & mcheck)? modecode[i]: '.';
            *cp1++ = mc;
            mcheck <<= 1;
        }

/* Append p or x to denote whether it is set to BTPHON or BTPROC */
        if (bp->b_type == BTPHON) *cp1++ = 'p';
        else if (bp->b_type == BTPROC) *cp1++ = 'x';
        else *cp1++ = '.';

        *cp1++ = ' ';                   /* Gap.                 */
        nbytes = 0L;                    /* Count bytes in buf.  */
        ue64I_t nlc = (bp->b_mode & MDDOSLE)? 2: 1;
        for (lp = lforw(bp->b_linep); lp != bp->b_linep; lp = lforw(lp)) {
            nbytes += (ue64I_t)lused(lp) + nlc;
        }
        char nb[21];                    /* To handle longest ue64I_t + NULL */
        sprintf(nb, "%20lld", nbytes);  /* Need 8 byte formatter */
        if (nb[11] != ' ') nb[11] = '+';    /* The last 9 chars */
        cp2 = nb + 11;
        while ((c = *cp2++) != 0) *cp1++ = c;
        *cp1++ = ' ';                   /* Gap.                 */
        cp2 = bp->b_bname;              /* Buffer name          */
        while ((c = *cp2++) != 0) *cp1++ = c;
        cp2 = bp->b_dfname;             /* File name            */
        if (*cp2 != 0) {
/* We know the current screen width, so use it...
 */
            if (((cp1 - line) + strlen(cp2)) > (unsigned)term.t_ncol) {
                *cp1++ = ' ';
                *cp1++ = 0xe2;      /* Carriage return symbol */
                *cp1++ = 0x86;      /* U+2185                 */
                *cp1++ = 0xb5;      /* as utf-8               */
                *cp1 = 0;           /* Add to the buffer.   */
                addline(line);
                cp1 = line;
                for (i = 0; i < 5; i++) *cp1++ = ' ';
            }
            else {
/* The header line is 3+1+13+1+9+1+13+1 to get to File
 * do...while ensures at least 1 space
 */
                do { *cp1++ = ' '; } while (cp1 < line+42);
            }
            while ((c = *cp2++) != 0) {
                if (cp1 < line+term.t_mcol) *cp1++ = c;
            }
        }
        *cp1 = 0;       /* Add to the buffer.   */
        addline(line);

/* This next section (SHOW_RPNAMES) is only for checking
 * what has been set by get_realpath.
 * It is not meant to be activated in normal use.
 */

#if SHOW_RPNAMES
        sprintf(line, "  rpname: ");
        cp2 = bp->b_rpname;             /* Resolved pathname */
        cp1 = line+strlen(line);
        if (*cp2 != 0) {
            while ((c = *cp2++) != 0) {
                if (cp1 < line+term.t_mcol) *cp1++ = c;
            }
        }
        *cp1 = 0;       /* Add to the buffer.   */
        addline(line);
#endif

        bp = bp->b_bufp;
    }
    Xfree(line);
    return TRUE;        /* All done             */
}

/* List all of the active buffers.  First update the special
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
    for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
        if (wp->w_bufp == blistp) {
            wp->w_linep = lforw(blistp->b_linep);
            wp->w.dotp = lforw(blistp->b_linep);
            wp->w.doto = 0;
            wp->w.markp = NULL;
            wp->w.marko = 0;
            wp->w_flag |= WFMODE | WFHARD;
        }
    }
    return TRUE;
}

/* Look through the list of buffers. Return TRUE if there are any
 * changed buffers.
 * Buffers that hold magic internal stuff are not considered; who cares
 * if the list of buffer names is hacked.
 * Return FALSE if no buffers have been changed.
 */
int anycb(void) {
    struct buffer *bp;

    for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if ((bp->b_flag & BFINVS) == 0 && (bp->b_flag & BFCHG) != 0)
            return TRUE;
    }
    return FALSE;
}

/* unmark the current buffer's change flag
 *
 * int f, n;            unused command arguments
 */
int unmark(int f, int n) {
    UNUSED(f); UNUSED(n);
    curbp->b_flag &= ~BFCHG;
    curwp->w_flag |= WFMODE;
    return TRUE;
}

/* Set the force_mode settings.
 *
 */
char do_force_mode(const char *opt) {    /* Returns 0 if all OK */

/* Are we just changing what is there, or setting an absolute value? */

    if (opt[0] == '+') opt++;      /* Skip the + */
    else force_mode_off = force_mode_on = 0;

    const char *op = opt;
    char c;
    int *word_to_set, *word_to_notset, bit_to_set;
    while ((c = *op++)) {
        if (c & 0x20) {     /* It's lowercase */
            word_to_set = &force_mode_off;
            word_to_notset = &force_mode_on;
        }
        else {
            word_to_set = &force_mode_on;
            word_to_notset = &force_mode_off;
        }
        switch(c & ~0x20) {
        case 'W': bit_to_set = MDWRAP;  break;
        case 'C': bit_to_set = MDCMOD;  break;
        case 'E': bit_to_set = MDEXACT; break;
        case 'V': bit_to_set = MDVIEW;  break;
        case 'O': bit_to_set = MDOVER;  break;
        case 'M': bit_to_set = MDMAGIC; break;
        case 'Q': bit_to_set = MDEQUIV; break;
        case 'D': bit_to_set = MDDOSLE; break;
        case 'R': bit_to_set = MDRPTMG; break;
        default:
            return c;       /* The character in error */
        }
        *word_to_set |= bit_to_set;
        *word_to_notset &= ~bit_to_set;
    }
    return 0;
}
int setforcemode(int f, int n) {
    UNUSED(f); UNUSED(n);
    db_strdef(cbuf);

/* Prompt the user and get an answer */

    int status = mlreply("Force mode to set:", &cbuf, CMPLT_NONE);
    if (status != TRUE) goto exit;
    char errch = do_force_mode(db_val(cbuf));
    if (errch) {
        mlforce("Invalid force mode: %c", errch);
        status = FALSE;
    }
exit:
    db_free(cbuf);
    return status;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_buffer(void) {
/* Free all of the buffers.
 * bclear() also frees all of the text, any buffer variables and
 * any Phonetic Translation table.
 * A loop over a cut-down zotbuf...with a cut-down bclear too.
 */
    struct buffer *nextbp;
    struct line *lp, *nextlp;
    for (struct buffer *bp = bheadp; bp; bp = nextbp) {
        nextbp = bp->b_bufp;                /* Get it while we can */
        if (bp->b_flag & BFNAROW) {
            struct buffer *obp = curwp->w_bufp;
            curwp->w_bufp = bp;             /* Ensure this is current */
            widen(0, -1);                   /* -1 == no reposition */
            curwp->w_bufp = obp;
        }
        for (lp = lforw(bp->b_linep); lp != bp->b_linep; lp = nextlp) {
            nextlp = lforw(lp);
/* Just need to free the text and struct. */
            db_free(ldb(lp));
            Xfree(lp);
        }
        Xfree(bp->b_linep); /* No text in this one */
        if (bp->bv) {       /* Must free the values too... */
            for (int vnum = 0; vnum < BVALLOC; vnum++) {
                if (bp->bv[vnum].name[0] == '\0') break;
                Xfree(bp->bv[vnum].value);
            }
        }
        Xfree(bp->bv);
        if ((bp->b_type == BTPHON) && bp->ptt_headp) ptt_free(bp);
        Xfree(bp->b_bname);
        Xfree(bp->b_dfname);
        Xfree(bp->b_rpname);
        Xfree(bp->b_key);
        Xfree(bp);
    }
    return;
}
#endif
