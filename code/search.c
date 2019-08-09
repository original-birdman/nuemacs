/*      search.c
 *
 * The functions in this file implement commands that search in the forward
 * and backward directions.  There are no special characters in the search
 * strings.  Probably should have a regular expression search, or something
 * like that.
 *
 * Aug. 1986 John M. Gamble:
 *      Made forward and reverse search use the same scan routine.
 *
 *      Added a limited number of regular expressions - 'any',
 *      'character class', 'closure', 'beginning of line', and
 *      'end of line'.
 *
 *      Replacement metacharacters will have to wait for a re-write of
 *      the replaces function, and a new variation of ldelete().
 *
 *      For those curious as to my references, i made use of
 *      Kernighan & Plauger's "Software Tools."
 *      I deliberately did not look at any published grep or editor
 *      source (aside from this one) for inspiration.  I did make use of
 *      Allen Hollub's bitmap routines as published in Doctor Dobb's Journal,
 *      June, 1985 and modified them for the limited needs of character class
 *      matching.  Any inefficiences, bugs, stupid coding examples, etc.,
 *      are therefore my own responsibility.
 *
 * April 1987: John M. Gamble
 *      Deleted the "if (n == 0) n = 1;" statements in front of the
 *      search/hunt routines.  Since we now use a do loop, these
 *      checks are unnecessary.  Consolidated common code into the
 *      function delins().  Renamed global mclen matchlen,
 *      and added the globals matchline, matchoff, patmatch, and
 *      mlenold.
 *      This gave us the ability to unreplace regular expression searches,
 *      and to put the matched string into an evironment variable.
 *      SOON TO COME: Meta-replacement characters!
 *
 *      25-apr-87       DML
 *      - cleaned up an unneccessary if/else in forwsearch() and
 *        backsearch()
 *      - savematch() failed to malloc room for the terminating byte
 *        of the match string (stomp...stomp...). It does now. Also
 *        it now returns gracefully if malloc fails
 *
 *      July 1987: John M. Gamble
 *      Set the variables matchlen and matchoff in the 'unreplace'
 *      section of replaces().  The function savematch() would
 *      get confused if you replaced, unreplaced, then replaced
 *      again (serves you right for being so wishy-washy...)
 *
 *      August 1987: John M. Gamble
 *      Put in new function rmcstr() to create the replacement
 *      meta-character array.  Modified delins() so that it knows
 *      whether or not to make use of the array.  And, put in the
 *      appropriate new structures and variables.
 *
 *      4 November 1987 Geoff Gibbs
 *      5 January 1988 Geoff Gibbs move it to 3.9e
 *
 *      Fast version using simplified version of Boyer and Moore
 *      Software-Practice and Experience, vol 10, 501-506 (1980)
 *      mods to scanner() and readpattern(),
 *      add fbound() and setpattern(), also define FAST, (or not).
 *      scanner() should be callable as before, provided setpattern()
 *      has been called first.
 *      Modified by Petri Kutvonen
 *
 * 2018
 * Various bits of the above no longer apply, but it's left for posterity.
 * The FATS search code is now hard-wired, as is a "magic" (regex) search.
 * There is also a navigable set of search and replace string buffers.
 */
#include <stdio.h>
#include <ctype.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

static int patlenadd;
static int deltaf[HICHAR],deltab[HICHAR];
static int lastchfjump, lastchbjump;

/*
 * The variables magical and rmagical determine whether there
 * were actual metacharacters in the search and replace strings -
 * if not, then we don't have to use the slower MAGIC mode
 * search functions.
 */
static short int magical;
static short int rmagical;
static struct magic mcpat[NPAT]; /* The magic pattern. */
static struct magic tapcm[NPAT]; /* The reversed magic pattern. */
static struct magic_replacement rmcpat[NPAT]; /* Replacement magic array. */

/* Search ring buffer code...
 */
#define RING_SIZE 10

static char *srch_txt[RING_SIZE], *repl_txt[RING_SIZE];
char current_base[NSTRING] = "";

enum call_type {Search, Replace};  /* So we know the current call type */
static enum call_type this_rt;

void init_search_ringbuffers(void) {
    for (int ix = 0; ix < RING_SIZE; ix++) {
        srch_txt[ix] = Xmalloc(1);
        *(srch_txt[ix]) = '\0';
        repl_txt[ix] = Xmalloc(1);
        *(repl_txt[ix]) = '\0';
    }
}

/* The new string goes in place at the top, pushing the others down
 * and dropping the last one.
 * Actually done by rotating the bottom to the top then updating the top.
 * However - we don't want to push anything out if there is an empty entry.
 */
int nring_free = RING_SIZE;
static void update_ring(char *str) {
    char **txt;
    if (this_rt == Search) txt = srch_txt;
    else                   txt = repl_txt;

/* Although for Search strings we do count down to 0 as items are added:
 * if there is a free one we don't know where it is, so have to look.
 * If there is an empty entry, move it to the bottom.
 * Empty entries are fine for Replace.
 * We should be able to run this RING_SIZE times - no more.
 */
    if (this_rt == Search && nring_free > 0) {
        for (int ix = 0; ix < RING_SIZE-1; ix++) { /* Can't move last one */
            if (txt[ix][0] == '\0') {
                char *tmp = txt[ix];
                for (int jx = ix; jx < RING_SIZE-1; jx++) {
                    txt[jx] = txt[jx+1];
                }
                txt[RING_SIZE-1] = tmp;
                break;
            }
        }
        nring_free--;
    }
    char *tmp = txt[RING_SIZE-1];
    for (int ix = RING_SIZE-1; ix ; ix--) {
        txt[ix] = txt[ix-1];
    }
    int slen = strlen(str);
    txt[0] = Xrealloc(tmp, slen + 1);
    strcpy(txt[0], str);

    return;
}

/*
 * A function to regenerate the prompt string with the given
 * text as the default.
 * Also called from svar() if it sets $replace or $search
 */
void new_prompt(char *dflt_str) {
    strcpy(prmpt_buf.prompt, current_base); /* copy prompt to output string */
    strcat(prmpt_buf.prompt, " " MLpre);    /* build new prompt string */
    expandp(dflt_str, prmpt_buf.prompt+strlen(prmpt_buf.prompt), NPAT / 2);
                                            /* add default pattern */
    strcat(prmpt_buf.prompt, MLpost ": ");
    prmpt_buf.update = 1;
    return;
}

/*
 * nextin_ring and previn_ring are always called with non -ve n
 */

static void previn_ring(int n, char *dstr, char *txt[]) {
    char *tmp_txt[RING_SIZE];

    int rotate_count = n % RING_SIZE;
    if (rotate_count == 0) return;              /* No movement */
    rotate_count = RING_SIZE - rotate_count;    /* So we go the right way */
/* Rotate by <n> by mapping into a temp array then memcpy back */
    int dx = rotate_count;
    for (int ix = 0; ix < RING_SIZE; ix++, dx++) {
        dx %= RING_SIZE;
        tmp_txt[dx] = txt[ix];
    }
    memcpy(txt, tmp_txt, sizeof(tmp_txt));
    strcpy(dstr, txt[0]);

/* We need to make getstring() show this new value in its prompt.
 * So we create what we want in prmpt_buf.prompt then set prmpt_buf.update
 * to tell getsring() to use it.
 */
    new_prompt(dstr);

    return;
}

/* nextin_ring is done as (RING_SIZE - n) in previn_ring() */

static void nextin_ring(int n, char *dstr, char *txt[]) {
    if (n == 0) return;
    int revn = RING_SIZE - n;   /* This result can be -ve ... */
    revn %= RING_SIZE;
    revn += RING_SIZE;          /* ... so this ensures we get +ve */
    previn_ring(revn, dstr, txt);
    return;
}

/*
 * Here are the callable functions. They must *NOT* be put into the
 * names[] array in names.c as they cannot be bound to any key!
 * They are called by mapping commands that are invalid in the minibuffer
 * when you are searching.
 */
int prev_sstr(int f, int n) {               /* Mapped from prevwind */
    if (n < 0) return next_sstr(f, -n);
    switch(this_rt) {
    case Search:
        previn_ring(n, pat,  srch_txt);
        break;
    case Replace:
        previn_ring(n, rpat, repl_txt);
        break;
    }
    return TRUE;
}
int next_sstr(int f, int n) {               /* Mapped from nextwind */
    if (n < 0) return prev_sstr(f, -n);
    switch(this_rt) {
    case Search:
        nextin_ring(n, pat,  srch_txt);
        break;
    case Replace:
        nextin_ring(n, rpat, repl_txt);
        break;
    }
    return TRUE;
}

/* Setting prmpt_buf.preload makes getstring() add it into the result buffer
 * at the start of its next get-character loop.
 * It will be inserted into any current search string at the current point.
 */
int select_sstr(int f, int n) {             /* Mapped from listbuffers */
    UNUSED(f); UNUSED(n);
    prmpt_buf.preload = (this_rt == Search)? srch_txt[0]: repl_txt[0];
    return TRUE;
}

/*
 * clearbits -- Allocate and zero out a CCL bitmap.
 */
static char *clearbits(void) {
    char *cclstart;
    char *cclmap;
    int i;

    cclmap = cclstart = (char *)Xmalloc(HIBYTE);
    for (i = 0; i < HIBYTE; i++) *cclmap++ = 0;

    return cclstart;
}

/*
 * setbit -- Set a bit (ON only) in the bitmap.
 */
static void setbit(int bc, char *cclmap) {
    bc = bc & 0xFF;
    if (bc < HICHAR) *(cclmap + (bc >> 3)) |= BIT(bc & 7);
}

/*
 * cclmake -- create the bitmap for the character class.
 *      ppatptr is left pointing to the end-of-character-class character,
 *      so that a loop may automatically increment with safety.
 */
static int cclmake(char **ppatptr, struct magic *mcptr) {
    char *bmap;
    char *patptr;
    int pchr, ochr;

    if ((bmap = clearbits()) == NULL) {
        mlwrite("%%Out of memory");
        return FALSE;
    }

    mcptr->u.cclmap = bmap;
    patptr = *ppatptr;

/* Test the initial character(s) in ccl for special cases - negate ccl, or
 *  an end ccl  character as a first character.  Anything else gets set
 * in the bitmap.
 */
    if (*++patptr == MC_NCCL) {
        patptr++;
        mcptr->mc_type = NCCL;
    }
    else mcptr->mc_type = CCL;

    if ((ochr = *patptr) == MC_ECCL) {
        mlwrite("%%No characters in character class");
        return FALSE;
    }
    else {
        if (ochr == MC_ESC) ochr = *++patptr;
        setbit(ochr, bmap);
        patptr++;
    }

    while (ochr != '\0' && (pchr = *patptr) != MC_ECCL) {
        switch (pchr) {
/* Range character loses its meaning if it is the last character in
 *the class.
 */
        case MC_RCCL:
            if (*(patptr + 1) == MC_ECCL) setbit(pchr, bmap);
            else {
                pchr = *++patptr;
                while (++ochr <= pchr)
                    setbit(ochr, bmap);
                }
            break;

        case MC_ESC:
            pchr = *++patptr;
            /* Falls through */
        default:
            setbit(pchr, bmap);
            break;
        }
        patptr++;
        ochr = pchr;
    }

    *ppatptr = patptr;

    if (ochr == '\0') {
        mlwrite("%%Character class not ended");
        free(bmap);
        return FALSE;
    }
    return TRUE;
}

/*
 * biteq -- is the character in the bitmap?
 */
static int biteq(int bc, char *cclmap) {
    bc = bc & 0xFF;
    if (bc >= HICHAR) return FALSE;

    return (*(cclmap + (bc >> 3)) & BIT(bc & 7)) ? TRUE : FALSE;
}

/*
 * mcstr -- Set up the 'magic' array.  The closure symbol is taken as
 *      a literal character when (1) it is the first character in the
 *      pattern, and (2) when preceded by a symbol that does not allow
 *      closure, such as a newline, beginning of line symbol, or another
 *      closure symbol.
 *
 *      Coding comment (jmg):  yes, i know i have gotos that are, strictly
 *      speaking, unnecessary.  But right now we are so cramped for
 *      code space that i will grab what i can in order to remain
 *      within the 64K limit.  C compilers actually do very little
 *      in the way of optimizing - they expect you to do that.
 */
static int mcstr(void) {
    struct magic *mcptr, *rtpcm;
    char *patptr;
    int mj;
    int pchr;
    int status = TRUE;
    int does_closure = FALSE;

/* If we had metacharacters in the struct magic array previously,
 * free up any bitmaps that may have been allocated.
 */
    if (magical) mcclear();

    magical = FALSE;
    mj = 0;
    mcptr = mcpat;
    patptr = pat;

    while ((pchr = *patptr) && status) {
        switch (pchr) {
        case MC_CCL:
            status = cclmake(&patptr, mcptr);
            magical = TRUE;
            does_closure = TRUE;
            break;
        case MC_BOL:
            if (mj != 0) goto litcase;
            mcptr->mc_type = BOL;
            magical = TRUE;
            does_closure = FALSE;
            break;
        case MC_EOL:
            if (*(patptr + 1) != '\0') goto litcase;
            mcptr->mc_type = EOL;
            magical = TRUE;
            does_closure = FALSE;
            break;
        case MC_ANY:
            mcptr->mc_type = ANY;
            magical = TRUE;
            does_closure = TRUE;
            break;
        case MC_CLOSURE:
/* Does the closure symbol mean closure here? If so, back up to the
 * previous element and indicate it is enclosed.
 */
            if (!does_closure) goto litcase;
            mj--;
            mcptr--;
            mcptr->mc_type |= CLOSURE;
            magical = TRUE;
            does_closure = FALSE;
            break;
        case MC_ESC:
            if (*(patptr + 1) != '\0') {
                pchr = *++patptr;
                magical = TRUE;
            } /* Falls through */
        default:
            litcase:mcptr->mc_type = LITCHAR;
            mcptr->u.lchar = pchr;
            does_closure = (pchr != '\n');
            break;
        }               /* End of switch. */
        mcptr++;
        patptr++;
        mj++;
    }                       /* End of while. */

/* Close off the meta-string. */

    mcptr->mc_type = MCNIL;

/* Set up the reverse array, if the status is good.  Please note the
 * structure assignment - your compiler may not like that.
 * If the status is not good, nil out the meta-pattern.
 * The only way the status would be bad is from the cclmake() routine,
 * and the bitmap for that member is guarenteed to be freed.
 * So we stomp a MCNIL value there, and call mcclear() to free any
 * other bitmaps.
 */
    if (status) {
        rtpcm = tapcm;
        while (--mj >= 0) {
#if USG | BSD
            *rtpcm++ = *--mcptr;
#endif
        }
        rtpcm->mc_type = MCNIL;
    }
    else {
        (--mcptr)->mc_type = MCNIL;
        mcclear();
    }
    return status;
}

/*
 * rmcstr -- Set up the replacement 'magic' array.  Note that if there
 *      are no meta-characters encountered in the replacement string,
 *      the array is never actually created - we will just use the
 *      character array rpat[] as the replacement string.
 */
static int rmcstr(void) {
    struct magic_replacement *rmcptr;
    char *patptr;
    int status = TRUE;
    int mj;

    patptr = rpat;
    rmcptr = rmcpat;
    mj = 0;
    rmagical = FALSE;

    while (*patptr && status == TRUE) {
        switch (*patptr) {
        case MC_DITTO:

/* If there were non-magical characters in the string before reaching this
 * character, plunk it in the replacement array before processing the
 * current meta-character.
 */
            if (mj != 0) {
                rmcptr->mc_type = LITCHAR;
                rmcptr->rstr = Xmalloc(mj + 1);
                strncpy(rmcptr->rstr, patptr - mj, mj);
                rmcptr++;
                mj = 0;
            }
            rmcptr->mc_type = DITTO;
            rmcptr++;
            rmagical = TRUE;
            break;

        case MC_ESC:
            rmcptr->mc_type = LITCHAR;

/* We Xmalloc mj plus two here, instead of one, because we have to count the
 * current character.
 */
            rmcptr->rstr = Xmalloc(mj + 2);
            strncpy(rmcptr->rstr, patptr - mj, mj + 1);

/* If MC_ESC is not the last character in the string, find out what it is
 * escaping, and overwrite the last character with it.
 */
            if (*(patptr + 1) != '\0')
                *((rmcptr->rstr) + mj) = *++patptr;

            rmcptr++;
            mj = 0;
            rmagical = TRUE;
            break;

        default:
            mj++;
        }
        patptr++;
    }

    if (rmagical && mj > 0) {
        rmcptr->mc_type = LITCHAR;
        rmcptr->rstr = Xmalloc(mj + 1);
        strncpy(rmcptr->rstr, patptr - mj, mj);
        rmcptr++;
    }

    rmcptr->mc_type = MCNIL;
    return status;
}

/*
 * eq -- Compare two characters.  The "bc" comes from the buffer, "pc"
 *      from the pattern.  If we are not in EXACT mode, fold out the case.
 */
int eq(unsigned char bc, unsigned char pc) {
    if ((curwp->w_bufp->b_mode & MDEXACT) == 0) {
        if (islower(bc)) bc ^= DIFCASE;
        if (islower(pc)) pc ^= DIFCASE;
    }
    return bc == pc;
}

/*
 * mceq -- meta-character equality with a character.  In Kernighan & Plauger's
 *      Software Tools, this is the function omatch(), but i felt there
 *      were too many functions with the 'match' name already.
 */
static int mceq(int bc, struct magic *mt) {
    int result;

    bc = bc & 0xFF;
    switch (mt->mc_type & MASKCL) {
    case LITCHAR:
        result = eq(bc, mt->u.lchar);
        break;

    case ANY:
        result = (bc != '\n');
        break;

    case CCL:
        if (!(result = biteq(bc, mt->u.cclmap))) {
            if ((curwp->w_bufp->b_mode & MDEXACT) == 0 && (isletter(bc))) {
                result = biteq(CHCASE(bc), mt->u.cclmap);
            }
        }
        break;

    case NCCL:
        result = !biteq(bc, mt->u.cclmap);

        if ((curwp->w_bufp->b_mode & MDEXACT) == 0 && (isletter(bc))) {
            result &= !biteq(CHCASE(bc), mt->u.cclmap);
        }
        break;

    default:
        mlwrite("mceq: what is %d?", mt->mc_type);
        result = FALSE;
        break;

    }                       /* End of switch. */

    return result;
}

/*
 * mcclear -- Free up any CCL bitmaps, and MCNIL the struct magic search
 * arrays.
 */
void mcclear(void) {
    struct magic *mcptr;

    mcptr = mcpat;

    while (mcptr->mc_type != MCNIL) {
        if ((mcptr->mc_type & MASKCL) == CCL ||
            (mcptr->mc_type & MASKCL) == NCCL)
            free(mcptr->u.cclmap);
        mcptr++;
    }
    mcpat[0].mc_type = tapcm[0].mc_type = MCNIL;
}

/*
 * rmcclear -- Free up any strings, and MCNIL the struct magic_replacement
 * array.
 */
static void rmcclear(void) {
    struct magic_replacement *rmcptr;

    rmcptr = rmcpat;

    while (rmcptr->mc_type != MCNIL) {
        if (rmcptr->mc_type == LITCHAR) free(rmcptr->rstr);
        rmcptr++;
    }
    rmcpat[0].mc_type = MCNIL;
}

/*
 * readpattern -- Read a pattern.  Stash it in apat.
 * If it is the search string, create the reverse pattern and the
 * magic pattern, assuming we are in MAGIC mode (and defined that way).
 * Apat is not updated if the user types in an empty line.
 * If the user typed an empty line, and there is no old pattern, it is
 * an error.
 * Display the old pattern, in the style of Jeff Lomicka.
 * There is some do-it-yourself control expansion.
 */
static int readpattern(char *prompt, char *apat, int srch) {
    int status;
    char tpat[NPAT + 20];

    char saved_base[NSTRING];

/* We save the base of the prompt for previn_ring to use.
 * Since this code can be re-enterd we have to save (and resotree at
 * the end) the current value.
 */
    strcpy(saved_base, current_base);
    strcpy(current_base, prompt);

    strcpy(tpat, prompt);       /* copy prompt to output string */
    strcat(tpat, " " MLpre);    /* build new prompt string */
    expandp(apat, tpat+strlen(tpat), NPAT / 2); /* add old pattern */
    strcat(tpat, MLpost ": ");

/* Read a pattern.  Either we get one or we just get an empty result
 * and use the previous pattern.
 * Then, if it's the search string, make a reversed pattern.
 * *Then*, make the meta-pattern, if we are defined that way.
 * Since search is recursive (you can search a search string) we
 * have to remember the in_search_prompt value of when we arrived.
 * We also have set this_rt before and after the mlreplyt() call.
 */

    int prev_in_search_prompt = in_search_prompt;
    in_search_prompt = 1;
    enum call_type our_rt = (srch == TRUE)? Search: Replace;
    this_rt = our_rt;           /* Set our call type for nextin_ring() */
    status = mlreplyt(tpat, tpat, NPAT, metac);
    this_rt = our_rt;           /* Set our call type for update_ring() */
    in_search_prompt = prev_in_search_prompt;
    int do_update_ring = 1;
/*
 * status values from mlreplyt (-> getstring()) are
 *  ABORT  (something went wrong)
 *  FALSE  the result (tpat) is empty
 *  TRUE   we have something in tpat
 *
 * so if status is FALSE we try to use the default value.
 * Since we can now run around saved ring buffers we have to ensure that
 * we have things set up for the currently-displayed default.
 * So put any default into tpat (and hence apat), but mark that we don't
 * want to update the ring buffer with this value.
 * We'll always update Replace strings, as an empty Replace is valid.
 */
    if (status == FALSE) {              /* Empty response */
        if (our_rt == Search) {
            if (pat[0] != 0) {          /* Have a default to use? */
                strcpy(tpat, pat);
                do_update_ring = 0;     /* Don't save this */
                status = TRUE;          /* So we do the next section... */
            }
        }
        else {                          /* Must be a Replace */
            strcpy(tpat, rpat);
            if (*repl_txt[0] == '\0')   /* If current top is also empty... */
                do_update_ring = 0;     /* ...don't save ths one */
            status = TRUE;              /* So we do the next section... */
        }
/* Record this default if we are recording a keyboard macro, but only
 * at base level of the minibuffer (i.e. we aren't searching *in* the
 * mini-buffer).
 */
        if (kbdmode == RECORD && mb_info.mbdepth == 0)
             addto_kbdmacro(tpat, 0, 1);
    }
    if (status == TRUE) {
        strcpy(apat, tpat);

/* Save this latest string in the search buffer ring? */

        if (do_update_ring) update_ring(tpat);

/* If we are doing the search string, reverse string copy, and remember
 * the length for substitution purposes.
 */
        if (srch) {
            rvstrcpy(tap, apat);
            mlenold = matchlen = strlen(apat);
            setpattern(apat, tap);
        }

/* Only make the meta-pattern if in magic mode, since the pattern in
 * question might have an invalid meta combination.
 */
        if ((curwp->w_bufp->b_mode & MDMAGIC) == 0) {
            mcclear();
            rmcclear();
        } else
            status = srch ? mcstr() : rmcstr();
    }
    strcpy(current_base, saved_base);   /* Revert any change */

    return status;
}

/*
 * nextch -- retrieve the next/previous character in the buffer,
 *      and advance/retreat the point.
 *      The order in which this is done is significant, and depends
 *      upon the direction of the search.  Forward searches look at
 *      the current character and move, reverse searches move and
 *      look at the character.
 */
static int nextch(struct line **pcurline, int *pcuroff, int dir) {
    struct line *curline;
    int curoff;
    int c;

    curline = *pcurline;
    curoff = *pcuroff;

    if (dir == FORWARD) {
        if (curoff == llength(curline)) {   /* if at EOL */
            curline = lforw(curline);       /* skip to next line */
            curoff = 0;
            c = '\n';                       /* and return a <NL> */
        }
        else c = lgetc(curline, curoff++);  /* get the char  */
    }
    else {                                  /* Reverse. */
        if (curoff == 0) {
            curline = lback(curline);
            curoff = llength(curline);
            c = '\n';
        }
        else c = lgetc(curline, --curoff);
    }
    *pcurline = curline;
    *pcuroff = curoff;

    return c;
}

/*
 * amatch -- Search for a meta-pattern in either direction.  Based on the
 *      recursive routine amatch() (for "anchored match") in
 *      Kernighan & Plauger's "Software Tools".
 *
 * struct magic *mcptr;         string to scan for
 * int direct;          which way to go.
 * struct line **pcwline;       current line during scan
 * int *pcwoff;         position within current line
 */
static int amatch(struct magic *mcptr, int direct, struct line **pcwline,
     int *pcwoff) {
    int c;                          /* character at current position */
    struct line *curline;           /* current line during scan */
    int curoff;                     /* position within current line */
    int nchars;

/* Set up local scan pointers to ".", and get the current character.
 * Then loop around the pattern pointer until success or failure.
 */
    curline = *pcwline;
    curoff = *pcwoff;

/* The beginning-of-line and end-of-line metacharacters do not compare
 * against characters, they compare against positions.
 * BOL is guaranteed to be at the start of the pattern for forward searches
 * and at the end of the pattern for reverse searches.
 * The reverse is true for EOL.
 * So, for a start, we check for them on entry.
 */
    if (mcptr->mc_type == BOL) {
        if (curoff != 0) return FALSE;
        mcptr++;
    }

    if (mcptr->mc_type == EOL) {
        if (curoff != llength(curline)) return FALSE;
        mcptr++;
    }

    while (mcptr->mc_type != MCNIL) {
        c = nextch(&curline, &curoff, direct);

        if (mcptr->mc_type & CLOSURE) {
/* Try to match as many characters as possible against the current
 * meta-character.  A newline never matches a closure.
 */
            nchars = 0;
            while (c != '\n' && mceq(c, mcptr)) {
                c = nextch(&curline, &curoff, direct);
                nchars++;
            }

/* We are now at the character that made us fail.
 * Try to match the rest of the pattern.
 * Shrink the closure by one for each failure.
 * Since closure matches *zero* or more occurences of a pattern, a match
 * may start even if the previous loop matched no characters.
 */
            mcptr++;

            for (;;) {
                c = nextch(&curline, &curoff, direct ^ REVERSE);

                if (amatch(mcptr, direct, &curline, &curoff)) {
                    matchlen += nchars;
                    goto success;
                }

                if (nchars-- == 0) return FALSE;
            }
        }
        else {        /* Not closure. */

/* The only way we'd get a BOL metacharacter at this point is at the end
 * of the reversed pattern.
 * The only way we'd get an EOL metacharacter here is at the end of a
 * regular pattern. So if we match one or the other, and are at the
 * appropriate position, we are guaranteed success (since the next
 * pattern character has to be MCNIL).
 * Before we report success, however, we back up by one character, so
 * as to leave the cursor in the correct position.
 * For example, a search for ")$" will leave the cursor at the end of the
 * line, while a search for ")<NL>" will leave the cursor at the
 * beginning of the next line.
 * This follows the notion that the meta-character '$' (and likewise
 * '^') match positions, not characters.
 */
            if (mcptr->mc_type == BOL) {
                if (curoff == llength(curline)) {
                    c = nextch(&curline, &curoff, direct ^ REVERSE);
                    goto success;
                } else
                    return FALSE;
            }

            if (mcptr->mc_type == EOL) {
                if (curoff == 0) {
                    c = nextch(&curline, &curoff, direct ^ REVERSE);
                    goto success;
                } else
                    return FALSE;
            }

/* Neither BOL nor EOL, so go through the meta-character equal function. */

            if (!mceq(c, mcptr)) return FALSE;
        }

/* Increment the length counter and advance the pattern pointer. */
        matchlen++;
        mcptr++;
    }                       /* End of mcptr loop. */

/* A SUCCESSFULL MATCH!!!  Reset the "." pointers. */

success:
    *pcwline = curline;
    *pcwoff = curoff;

    return TRUE;
}

/*
 * mcscanner -- Search for a meta-pattern in either direction.  If found,
 *      reset the "." to be at the start or just after the match string,
 *      and (perhaps) repaint the display.
 *
 * struct magic *mcpatrn;                       pointer into pattern
 * int direct;                  which way to go.
 * int beg_or_end;              put point at beginning or end of pattern.
 */
int mcscanner(struct magic *mcpatrn, int direct, int beg_or_end) {
    struct line *curline;   /* current line during scan */
    int curoff;             /* position within current line */

/* If we are going in reverse, then the 'end' is actually the beginning
 * of the pattern.  Toggle it.
 */
    beg_or_end ^= direct;

/* Save the old matchlen length, in case it is very different (closure)
 * from the old length. This is important for query-replace undo command.
 */
    mlenold = matchlen;

/* Setup local scan pointers to global ".". */

    curline = curwp->w_dotp;
    curoff = curwp->w_doto;

/* Scan each character until we hit the head link record. */

    while (!boundry(curline, curoff, direct)) {

/* Save the current position in case we need to restore it on a match,
 * and initialize matchlen to zero in case we are doing a search for
 * replacement.
 */
        matchline = curline;
        matchoff = curoff;
        matchlen = 0;

        if (amatch(mcpatrn, direct, &curline, &curoff)) {
/* A SUCCESSFULL MATCH!!! Reset the global "." pointers. */
            if (beg_or_end == PTEND) {      /* at end of string */
                curwp->w_dotp = curline;
                curwp->w_doto = curoff;
            }
            else {        /* at beginning of string */
                curwp->w_dotp = matchline;
                curwp->w_doto = matchoff;
            }
            curwp->w_flag |= WFMOVE;        /* flag that we've moved */
            return TRUE;
        }

        nextch(&curline, &curoff, direct);  /* Advance the cursor. */
    }
    return FALSE;                           /* We could not find a match. */
}

/*
 * fbound -- Return information depending on whether we may search no
 *      further.  Beginning of file and end of file are the obvious
 *      cases, but we may want to add further optional boundry restrictions
 *      in future, a' la VMS EDT.  At the moment, just return TRUE or
 *      FALSE depending on if a boundry is hit (ouch),
 *      when we have found a matching trailing character.
 */
static int fbound(int jump, struct line **pcurline, int *pcuroff, int dir) {
    int spare, curoff;
    struct line *curline;

    curline = *pcurline;
    curoff = *pcuroff;

    if (dir == FORWARD) {
        while (jump != 0) {
            curoff += jump;
            spare = curoff - llength(curline);
            while (spare > 0) {
                curline = lforw(curline);   /* skip to next line */
                curoff = spare - 1;
                spare = curoff - llength(curline);
                if (curline == curbp->b_linep)
                    return TRUE;            /* hit end of buffer */
            }
            if (spare == 0) {
                jump = deltaf[(int) '\n'];
            }
            else {
                jump = deltaf[ch_as_uc(lgetc(curline, curoff))];
            }
        }
/* the last character matches, so back up to start of possible match */

        curoff -= patlenadd;
        while (curoff < 0) {
            curline = lback(curline);/* skip back a line */
            curoff += llength(curline) + 1;
        }
    }
    else {                  /* Reverse.*/
        jump++;             /* allow for offset in reverse */
        while (jump != 0) {
            curoff -= jump;
            while (curoff < 0) {
                curline = lback(curline);       /* skip back a line */
                curoff += llength(curline) + 1;
                if (curline == curbp->b_linep)
                    return TRUE;    /* hit end of buffer */
            }
            if (curoff == llength(curline)) {
                jump = deltab[(int) '\n'];
            }
            else {
                jump = deltab[(int) lgetc(curline, curoff)];
            }
        }
/* the last character matches, so back up to start of possible match */

        curoff += matchlen;
        spare = curoff - llength(curline);
        while (spare > 0) {
            curline = lforw(curline);/* skip back a line */
            curoff = spare - 1;
            spare = curoff - llength(curline);
        }
    }

    *pcurline = curline;
    *pcuroff = curoff;
    return FALSE;
}

/*
 * scanner -- Search for a pattern in either direction.  If found,
 *      reset the "." to be at the start or just after the match string,
 *      and (perhaps) repaint the display.
 *      Fast version using simplified version of Boyer and Moore
 *      Software-Practice and Experience, vol 10, 501-506 (1980)
 */
int scanner(const char *patrn, int direct, int beg_or_end) {
    int c;                          /* character at current position */
    const char *patptr;             /* pointer into pattern */
    struct line *curline;           /* current line during scan */
    int curoff;                     /* position within current line */
    struct line *scanline;          /* current line during scanning */
    int scanoff;                    /* position in scanned line */
    int jump;                       /* next offset */

/* If we are going in reverse, then the 'end' is actually the beginning
 * of the pattern.  Toggle it.
 */

    beg_or_end ^= direct;

/* Set up local pointers to global ".". */

    curline = curwp->w_dotp;
    curoff = curwp->w_doto;

/* Scan each character until we hit the head link record.
 * Get the character resolving newlines, offset by the pattern length,
 * i.e. the last character of the  potential match.
 */

    jump = patlenadd;

    while (!fbound(jump, &curline, &curoff, direct)) {

/* Save the current position in case we match the search string at
 * this point.
 */

        matchline = curline;
        matchoff = curoff;
/* Setup scanning pointers. */
        scanline = curline;
        scanoff = curoff;
        patptr = patrn;

/* Scan through the pattern for a match. */
        while (*patptr != '\0') {
            c = nextch(&scanline, &scanoff, direct);

/*
 * Debugging info, show the character and the remains of the string
 * it is being compared with.
 *
 *      strcpy(tpat, "matching ");
 *      expandp(&c, &tpat[strlen(tpat)], NPAT/2);
 *      expandp(patptr, &tpat[strlen(tpat)], NPAT/2);
 *      mlwrite(tpat);
 */
            if (!eq(c, *patptr++)) {
                jump = (direct == FORWARD)? lastchfjump: lastchbjump;
                goto fail;
            }
        }

/* A SUCCESSFULL MATCH!!! Reset the global "." pointers */
        if (beg_or_end == PTEND) {      /* at end of string */
            curwp->w_dotp = scanline;
            curwp->w_doto = scanoff;
        }
        else {                          /* at beginning of string */
            curwp->w_dotp = matchline;
            curwp->w_doto = matchoff;
        }
        curwp->w_flag |= WFMOVE;        /* Flag that we have moved.*/
        return TRUE;
fail:;                                  /* continue to search */
    }
    return FALSE;                       /* We could not find a match */
}

/*
 * savematch -- We found the pattern?  Let's save it away.
 */
static void savematch(void) {
    char *ptr;                      /* pointer to last match string */
    struct line *curline;           /* line of last match */
    int curoff;                     /* offset "      "    */

/* Free any existing match string, then attempt to allocate a new one. */

    if (patmatch != NULL) free(patmatch);

    ptr = patmatch = Xmalloc(matchlen + 1);

    curoff = matchoff;
    curline = matchline;

    for (unsigned int j = 0; j < matchlen; j++)
        *ptr++ = nextch(&curline, &curoff, FORWARD);

    *ptr = '\0';
}

static int forwscanner(int n) { /* Common to forwsearch()/forwhunt() */
    int status;

/* Search for the pattern for as long as n is positive (n == 0 will go
 * through once, which is just fine).
 */
    do {

/* We are going forwards so check for eob as otherwise the rest
 * of this code (magical and ordinary) loops us round to the start
 * and searches from there.
 * Simpler to fudge the fix here....
 */
        if (curwp->w_dotp == curbp->b_linep) {
            status = FALSE;
            break;
        }
        if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
            status = mcscanner(mcpat, FORWARD, PTEND);
        else
            status = scanner(pat, FORWARD, PTEND);
    } while ((--n > 0) && status);
    return status;
}

/*
 * forwsearch -- Search forward.  Get a search string from the user, and
 *      search for the string.  If found, reset the "." to be just after
 *      the match string, and (perhaps) repaint the display.
 *
 * int f, n;                    default flag / numeric argument
 */
int forwsearch(int f, int n) {
    int status = TRUE;

/* If n is negative, search backwards.
 * Otherwise proceed by asking for the search string.
 */
    if (n < 0) return backsearch(f, -n);

/* Ask the user for the text of a pattern.  If the response is TRUE
 * (responses other than FALSE are possible), search for the pattern for as
 * long as  n is positive (n == 0 will go through once, which is just fine).
 */
    if ((status = readpattern("Search", pat, TRUE)) == TRUE) {
        status = forwscanner(n);

/* Save away the match, or complain if not there. */

        if (status == TRUE)
            savematch();
        else
            mlwrite("Not found");
    }
    return status;
}

/*
 * forwhunt -- Search forward for a previously acquired search string.
 *      If found, reset the "." to be just after the match string,
 *      and (perhaps) repaint the display.
 *
 * int f, n;            default flag / numeric argument
 */
int forwhunt(int f, int n) {
    int status = TRUE;

    if (n < 0)              /* search backwards */
        return backhunt(f, -n);

/* Make sure a pattern exists, or that we didn't switch into MAGIC mode
 * until after we entered the pattern.
 */
    if (pat[0] == '\0') {
        mlwrite("No pattern set");
        return FALSE;
    }
    if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && mcpat[0].mc_type == MCNIL) {
        if (!mcstr()) return FALSE;
    }
    status = forwscanner(n);

/* Complain if not there - we already have the saved match... */

    if (status != TRUE) mlwrite("Not found");

    return status;
}

static int backscanner(int n) { /* Common to backsearch()/backwhunt() */
    int status;

/* Search for the pattern for as long as n is positive (n == 0 will go
 * through once, which is just fine).
 */
    do {
        if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
            status = mcscanner(tapcm, REVERSE, PTBEG);
        else
            status = scanner(tap, REVERSE, PTBEG);
    } while ((--n > 0) && status);
    return status;
}

/*
 * backsearch -- Reverse search.  Get a search string from the user, and
 *      search, starting at "." and proceeding toward the front of the buffer.
 *      If found "." is left pointing at the first character of the pattern
 *      (the last character that was matched).
 *
 * int f, n;            default flag / numeric argument
 */
int backsearch(int f, int n) {
    int status = TRUE;

/* If n is negative, search forwards. Otherwise proceed by asking for the
 * search string.
 */
    if (n < 0) return forwsearch(f, -n);

/* Ask the user for the text of a pattern.  If the response is TRUE
 * (responses other than FALSE are possible), search for the pattern for
 * as long as n is positive (n == 0 will go through once, which is just fine).
 */
    if ((status = readpattern("Reverse search", pat, TRUE)) == TRUE) {
        status = backscanner(n);

/* Save away the match, or complain if not there. */

        if (status == TRUE) savematch();
        else                mlwrite("Not found");
    }
    return status;
}

/*
 * backhunt -- Reverse search for a previously acquired search string,
 *      starting at "." and proceeding toward the front of the buffer.
 *      If found "." is left pointing at the first character of the pattern
 *      (the last character that was matched).
 *
 * int f, n;            default flag / numeric argument
 */
int backhunt(int f, int n) {
    int status = TRUE;

    if (n < 0) return forwhunt(f, -n);

/* Make sure a pattern exists, or that we didn't switch into MAGIC mode
 * until after we entered the pattern.
 */
    if (tap[0] == '\0') {
        mlwrite("No pattern set");
        return FALSE;
    }
    if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && tapcm[0].mc_type == MCNIL) {
        if (!mcstr()) return FALSE;
    }

/* Go search for it for as long as n is positive (n == 0 will go through
 * once, which  is just fine).
 */
    status = backscanner(n);

/* Complain if not there - we already have the saved match... */

    if (status != TRUE) mlwrite("Not found");

    return status;
}

/* Entry point for isearch.
 * This needs to set-up the patterns for the search to work.
 */
int scanmore(char *patrn, int dir) {
    int sts;                /* search status              */

    rvstrcpy(tap, patrn);   /* Put reversed string in tap */
    mlenold = matchlen = strlen(patrn);
    setpattern(patrn, tap);

    if (dir < 0)            /* reverse search?            */
        sts = scanner(tap, REVERSE, PTBEG);
    else                    /* Nope. Go forward           */
        sts = scanner(patrn, FORWARD, PTEND);

    if (!sts) {
        TTputc(BELL);   /* Feep if search fails       */
        TTflush();      /* see that the feep feeps    */
    }

    return sts;             /* else, don't even try       */
}

/*      Settting up search jump tables.
 *      the default for any character to jump
 *      is the pattern length
 */
void setpattern(const char apat[], const char tap[]) {
    int i;

    patlenadd = matchlen - 1;

    for (i = 0; i < HICHAR; i++) {
        deltaf[i] = matchlen;
        deltab[i] = matchlen;
    }

/* Now put in the characters contained in the pattern, duplicating the CASE
 */
    for (i = 0; i < patlenadd; i++) {
        deltaf[ch_as_uc(apat[i])] = patlenadd - i;
        if (isalpha(ch_as_uc(apat[i])))
            deltaf[ch_as_uc(apat[i] ^ DIFCASE)] = patlenadd - i;
        deltab[ch_as_uc(tap[i])] = patlenadd - i;
        if (isalpha(ch_as_uc(tap[i])))
            deltab[ch_as_uc(tap[i] ^ DIFCASE)] = patlenadd - i;
    }

/* The last character will have the pattern length unless there are
 * duplicates of it. Get the number to jump from the arrays delta, and
 * overwrite with zeroes in delta duplicating the CASE.
 */
    lastchfjump = patlenadd + deltaf[ch_as_uc(apat[patlenadd])];
    deltaf[ch_as_uc(apat[patlenadd])] = 0;
    if (isalpha(ch_as_uc(apat[patlenadd])))
        deltaf[ch_as_uc(apat[patlenadd] ^ DIFCASE)] = 0;
    lastchbjump = patlenadd + deltab[ch_as_uc(apat[0])];
    deltab[ch_as_uc(apat[0])] = 0;
    if (isalpha(ch_as_uc(apat[0])))
        deltab[ch_as_uc(apat[0] ^ DIFCASE)] = 0;
}

/*
 * rvstrcpy -- Reverse string copy.
 */
void rvstrcpy(char *rvstr, char *str) {
    int i;

    str += (i = strlen(str));

    while (i-- > 0) *rvstr++ = *--str;

    *rvstr = '\0';
}

/*
 * delins -- Delete a specified length from the current point
 *      then either insert the string directly, or make use of
 *      replacement meta-array.
 */
static int delins(int dlength, char *instr, int use_meta) {
    int status;
    struct magic_replacement *rmcptr;

/* Zap what we gotta, * and insert its replacement. */

    if ((status = ldelete((long) dlength, FALSE)) != TRUE)
        mlwrite("%%ERROR while deleting");
    else
        if ((rmagical && use_meta) &&
                    (curwp->w_bufp->b_mode & MDMAGIC) != 0) {
            rmcptr = rmcpat;
            while (rmcptr->mc_type != MCNIL && status == TRUE) {
                if (rmcptr->mc_type == LITCHAR)
                    status = linstr(rmcptr->rstr);
                else
                    status = linstr(patmatch);
                rmcptr++;
            }
        }
        else status = linstr(instr);

    return status;
}

/*
 * replaces -- Search for a string and replace it with another
 *      string.  Query might be enabled (according to kind).
 *
 * int kind;            Query enabled flag
 * int f;               default flag
 * int n;               # of repetitions wanted
 */
static int replaces(int kind, int f, int n) {
    int status;             /* success flag on pattern inputs */
    int rlength;            /* length of replacement string */
    int numsub;             /* number of substitutions */
    int nummatch;           /* number of found matches */
    int nlflag;             /* last char of search string a <NL>? */
    int nlrepl;             /* was a replace done on the last line? */
    char c;                 /* input char for query */
    char tpat[NPAT];        /* temporary to hold search pattern */
    struct line *origline;  /* original "." position */
    int origoff;            /* and offset (for . query option) */
    struct line *lastline;  /* position of last replace and */
    int lastoff;            /* offset (for 'u' query option) */

    if (curbp->b_mode & MDVIEW) /* don't allow this command if  */
        return rdonly();        /* we are in read only mode     */

/* Check for negative repetitions. */

    if (f && n < 0) return FALSE;

/* Ask the user for the text of a pattern. */

    if ((status = readpattern((kind == FALSE ? "Replace" : "Query replace"),
                                  pat, TRUE)) != TRUE)
        return status;

/* Ask for the replacement string. */

    if ((status = readpattern("with", rpat, FALSE)) == ABORT)
        return status;

/* Find the length of the replacement string. */

    rlength = strlen(rpat);

/* Set up flags so we can make sure not to do a recursive replace on
 * the last line.
 */
    nlflag = (pat[matchlen - 1] == '\n');
    nlrepl = FALSE;

    if (kind) {
/* Build query replace question string. */
        strcpy(tpat, "Replace '");
        expandp(pat, tpat+strlen(tpat), NPAT / 3);
        strcat(tpat, "' with '");
        expandp(rpat, tpat+strlen(tpat), NPAT / 3);
        strcat(tpat, "'? ");

/* Initialize last replaced pointers. */

        lastline = NULL;
        lastoff = 0;
    }

/* Save original . position, init the number of matches and substitutions,
 * and scan through the file.
 */
    origline = curwp->w_dotp;
    origoff = curwp->w_doto;
    numsub = 0;
    nummatch = 0;

    while ((f == FALSE || n > nummatch) &&
           (nlflag == FALSE || nlrepl == FALSE)) {

/* Search for the pattern. If we search with a regular expression,
 * matchlen is reset to the true length of the matched string.
 */
        if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0) {
            if (!mcscanner(mcpat, FORWARD, PTBEG)) break;
        }
        else {
            if (!scanner(pat, FORWARD, PTBEG)) break;  /* all done */
        }
        ++nummatch;     /* Increment # of matches */

/* Check if we are on the last line. */

        nlrepl = (lforw(curwp->w_dotp) == curwp->w_bufp->b_linep);

/* Check for query. */

        if (kind) {     /* Get the query. */
            pprompt:mlwrite(tpat, pat, rpat);
qprompt:
            update(TRUE);   /* show the proposed place to change */
            c = tgetc();    /* and input */
            mlwrite("");    /* and clear it */

/* And respond appropriately. GGR - make case-insensitive (lower..) */

            if ('A' <= c && c <= 'Z') c -= 'A' - 'a';
            switch (c) {
            case 'y':       /* yes, substitute */
            case ' ':
                savematch();
                break;

            case 'n':       /* no, onword */
                forw_grapheme(1);
                continue;

            case '!':       /* yes/stop asking */
                kind = FALSE;
                break;

            case 'u':       /* undo last and re-prompt */

/* Restore old position. */
                if (lastline == NULL) { /* There is nothing to undo. */
                    TTbeep();
                    goto pprompt;
                }
                curwp->w_dotp = lastline;
                curwp->w_doto = lastoff;
                lastline = NULL;
                lastoff = 0;

/* Delete the new string. */
                back_grapheme(rlength);
                matchline = curwp->w_dotp;
                matchoff = curwp->w_doto;
                status = delins(rlength, patmatch, FALSE);
                if (status != TRUE) return status;

/* Record one less substitution, Backup, save our place, and  reprompt. */
                --numsub;
                back_grapheme(mlenold);
                matchline = curwp->w_dotp;
                matchoff = curwp->w_doto;
                goto pprompt;

            case '.':       /* abort! and return */
/* Restore old position */
                curwp->w_dotp = origline;
                curwp->w_doto = origoff;
                curwp->w_flag |= WFMOVE;
                /* Falls through */

            case BELL:      /* abort! and stay */
                mlwrite("Aborted!");
                return FALSE;

            default:        /* bitch and beep */
                TTbeep();
                /* Falls through */
            case '?':       /* help me */
                mlwrite(
  "(Y)es, (N)o, (!)Do rest, (U)ndo, (^G)Abort, (.)Abort+back, (?)Help: ");
                goto qprompt;

            }       /* end of switch */
        }           /* end of "if kind" */

/* GGR fix */
        else if ((rmagical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
            savematch();

/* Delete the sucker, and insert its replacement. */

        status = delins(matchlen, rpat, TRUE);
        if (status != TRUE) return status;

/* Save our position, since we may undo this. */
        if (kind) {
            lastline = curwp->w_dotp;
            lastoff = curwp->w_doto;
        }

        numsub++;       /* increment # of substitutions */
    }

/* And report the results. */
    mlwrite("%d substitutions", numsub);
    return TRUE;
}

/*
 * sreplace -- Search and replace.
 *
 * int f;               default flag
 * int n;               # of repetitions wanted
 */
int sreplace(int f, int n) {
    return replaces(FALSE, f, n);
}

/*
 * qreplace -- search and replace with query.
 *
 * int f;               default flag
 * int n;               # of repetitions wanted
 */
int qreplace(int f, int n) {
    return replaces(TRUE, f, n);
}

/*
 * expandp -- Expand control key sequences for output.
 *
 * char *srcstr;                string to expand
 * char *deststr;               destination of expanded string
 * int maxlength;               maximum chars in destination
 */
int expandp(char *srcstr, char *deststr, int maxlength) {
    unsigned char c;        /* current char to translate */

/* Scan through the string. */

    while ((c = *srcstr++) != 0) {
        if (c == '\n') {                        /* it's a newline */
            *deststr++ = '<';
            *deststr++ = 'N';
            *deststr++ = 'L';
            *deststr++ = '>';
            maxlength -= 4;
        }
        else if ((c > 0 && c < 0x20) || c == 0x7f) { /* control character */
            *deststr++ = '^';
            *deststr++ = c ^ 0x40;
            maxlength -= 2;
        }
        else if (c == '%') {
            *deststr++ = '%';
            *deststr++ = '%';
            maxlength -= 2;
        }
        else {                                  /* any other character */
            *deststr++ = c;
            maxlength--;
        }

        if (maxlength < 4) {                    /* Check for maxlength */
            *deststr++ = '$';
            *deststr = '\0';
            return FALSE;
        }
    }
    *deststr = '\0';
    return TRUE;
}

/*
 * boundry -- Return information depending on whether we may search no
 *      further.  Beginning of file and end of file are the obvious
 *      cases, but we may want to add further optional boundry restrictions
 *      in future, a' la VMS EDT.  At the moment, just return TRUE or
 *      FALSE depending on if a boundry is hit (ouch).
 */
int boundry(struct line *curline, int curoff, int dir) {
    int border;

    if (dir == FORWARD) {
        border = (curoff == llength(curline)) &&
             (lforw(curline) == curbp->b_linep);
    }
    else {
        border = (curoff == 0) && (lback(curline) == curbp->b_linep);
    }
    return border;
}
