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
 *      matching.  Any inefficiencies, bugs, stupid coding examples, etc.,
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
 *      and to put the matched string into an environment variable.
 *      SOON TO COME: Meta-replacement characters!
 *
 *      25-Apr-87       DML
 *      - cleaned up an unnecessary if/else in forwsearch() and
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
 * 2018+
 * Various bits of the above no longer apply, but it's left for posterity.
 * The FAST search code is now hard-wired, as is a "magic" (regex) search.
 * There is also a navigable set of search and replace string buffers.
 *
 * scanner() is now fast_scanner() and mcscanner() is now step_scanner().
 *
 * June 2022 - an explanation of the arrival of srch_can_hunt,
 * do_preskip and GGR_SRCHOLAP flag.
 *
 * This is all to do with repeating a search.
 * Take the example of (forward) searching "mississippi".
 * A search for "iss" would find two matches
 * A search for "issi" would find only one UNLESS it was a reverse search
 * in Magic mode, when it would find two again.
 * This is a result of how matches are (now) done.
 * The non-Magic code uses jump tables (in either direction) and hence only
 * finds matches beyond any previous match in the direction of the search.
 * The Magic code *always* matches in a forward direction - just that for
 * a reverse search it moves backwards from the current location to test
 * start positions. (There may be a bug here, as "issi" isn't a magic string
 * and that should be noted, but the same would apply to "is.i".)
 * As a result this will find an "overlapping" match.
 * These new variables allow for consistent results (although it has to
 * handle the reverse-magic search in the "opposite" way to the handling
 * of the other three ways).
 *
 *  do_preskip
 * if set, this skips back to the "other end" of the previous match
 * (less one) before searching again, hence finding overlapping matches.
 * This is just an internal variable.
 *
 *  GGR_SRCHOLAP flag in $ggr_opts
 * A flag setting that determines whether to look for overlapping matches.
 * By default it is on in the standard uemacs.rc start-up file (all $ggr_opts
 * bits are set on), but it can be switched off by unsetting bit 0x08
 * in the variable.
 *
 *  srch_can_hunt
 * determines whether a search for a previously set string is valid.
 * It is not if the position has changed or the buffer may have been
 * modified since the previous search for the same string.
 * This setting can determine whether a preskip is needed and also enables
 * macro repeat search testing to work whilst reporting match information
 * along the way (this uses the system variable setting, but this should
 * not normally be set by a user).
 *
 */
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#define SEARCH_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "line.h"

#include "utf8proc.h"

/* Defines for the types in struct magic (and struct magic_replacement)
 * The pattern now ends when Group0 closes, so there is no longer
 * any need for the old MCNIL.
 */
#define LITCHAR         1       /* Literal character, or string. */
#define ANY             2
#define CCL             3
#define BOL             4
#define EOL             5
#define UCLITL          6
#define UCGRAPH         7
#define ANYGPH          8
#define UCPROP          9
#define UCKIND          10
#define SGRP            11
#define EGRP            12
#define CHOICE          13
#define REPL_VAR        14
#define REPL_GRP        15
#define REPL_CNT        16
#define REPL_FNC        17

/* Defines for the metacharacters in the regular expression patterns. */

#define MC_ANY          '.'     /* 'Any' character (except newline). */
#define MC_CCL          '['     /* Character class. */
#define MC_NCCL         '^'     /* Negate character class. */
#define MC_RCCL         '-'     /* Range in character class. */
#define MC_ECCL         ']'     /* End of character class. */
#define MC_SGRP         '('     /* Start a grouping */
#define MC_EGRP         ')'     /* End a grouping */
#define MC_BOL          '^'     /* Beginning of line at strat of match. */
#define MC_EOL          '$'     /* End of line at end of match. */
#define MC_REPEAT       '*'     /* == Closure. */
#define MC_ONEPLUS      '+'     /* non-zero Closure. */
#define MC_RANGE        '{'     /* Ranged Closure. */
#define MC_OR           '|'     /* OR choice */
#define MC_MINIMAL      '?'     /* Shortest Closure OR 0/1 match. */
#define MC_REPL         '$'     /* Use matching group/var in replacement.
                                   MUST be followed by a {...} group. */
#define MC_ESC          '\\'    /* Escape - suppress meta-meaning. */

#define BIT(n)          (1 << (n))      /* An integer with one bit set. */
#define CHCASE(c)       ((c) ^ DIFCASE) /* Toggle the case of a letter. */

/* MAXASCII is the largest character we will deal with "simply" in CCLs.
 * BMBYTES represents the number of bytes in the bitmap.
 */
#define MAXASCII        0x7f
#define BMBYTES          (MAXASCII + 1) >> 3

/* HICHAR is for the fast scan bitmap */
#define HICHAR          256

#define LASTUL 'Z'
#define LASTLL 'z'

#define isASCletter(c) (('a' <= c && LASTLL>= c) || ('A' <= c && LASTUL >= c))

/* Typedefs that define the meta-character structure for MAGIC mode searching
 * (struct magic), and replacement (struct magic_replacement).
 */
struct range {
    unsigned int low;
    unsigned int high;
};
struct mg_info {
    unsigned int type :8;
    unsigned int group_num :8;  /* Which group is start/ending */
    unsigned int repeat :1;
    unsigned int min_repeat :1;
    unsigned int negate_test :1;
};

struct xccl {
    struct mg_info xc;          /* for type and negate_test (prop only) */
    union {
        struct range cl_lim;    /* low/high codepoint to test */
        char prop[4];           /* +ve or -ve */
        unicode_t uchar;        /* A Unicode character point */
        struct grapheme gc;     /* A Unicode grapheme */
    } xval;
};

/* Keep the first two entries (struct mg_info and union val)
 * in the next two structures "in sync" for common items, then
 * dump_mc can report on both easily.
 */
struct magic {
    struct mg_info mc;
    union {                     /* Can only be one at a time */
        char *cclmap;           /* Pointer */
        int lchar;              /* An ASCII char */
        unicode_t uchar;        /* A Unicode character point */
        struct grapheme gc;     /* A Unicode grapheme */
        char prop[4];           /* Direct - 2 chars + NULL */
        struct magic *gpm;      /* Group handling sub pattern */
    } val;
    struct range cl_lim;        /* Limits if mc.repeat set */
    union {                     /* Extended info */
        struct xccl *xt_cclmap; /* Pointer */
        int next_or_idx;        /* next alternative check */
    } x;
};

struct magic_counter {
    int curval;
    int incr;
    char *fmt;
};

struct func_call {
    int type;
    struct func_call *next;
    union {                     /* Can only be one at a time */
        char *ltext;            /* malloc()ed literla text */
        int group_num;          /* Group replacement */
        struct magic_counter x; /* A counter */
    } val;
};
struct magic_replacement {
    struct mg_info mc;
    union {                     /* Can only be one at a time */
        int lchar;              /* An ASCII char */
        unicode_t uchar;        /* A Unicode character point */
        struct grapheme gc;     /* A Unicode grapheme */
        char *varname;          /* malloc()ed varname */
        struct magic_counter x; /* A counter */
        struct func_call *fc;   /* Function call info */
    } val;
};

/* The group_info data contains some only used when building the
 * struct magic array and some only used when processing it.
 */
enum GP_state { GPIDLE, GPOPEN, GPPENDING, GPVALID };
struct control_group_info {     /* Info that controls usage */
    int parent_group;           /* Group to fall into when this group ends */
    enum GP_state state;        /* For group balancing check in mcstr() and
                                   for "is group valid" in group_match() */
    int start_level;            /* amatch() level group was started */
    int pending_level;          /* amatch() level group was closed */
    int next_choice_idx;        /* In mcstr(), where to put OR for this group
                                 * In amatch()/step_scanner(), where the next
                                 * OR is. */
    int gpend;                  /* Index where the group pattern ends */
};
struct match_group_info {       /* String match info - reset on failures */
    struct line *mline;         /* Buffer location of match */
    int start;                  /* Byte offset of start within mline */
    int len;                    /* Length, in bytes */
    int base;                   /* byte count of chars matched before *start*
                                 * of group.
                                 * ambytes is reset to this on CHOICE. */
    };
static int max_grp = 0;
static struct control_group_info *cntl_grp_info = NULL;
static struct match_group_info *match_grp_info = NULL;
static char **grp_text = NULL;

/* A value with which to reset */
static const struct match_group_info null_match_grp_info = { NULL, 0, 0, 0 };

/* State information variables. */

static int patlenadd;
static int deltaf[HICHAR],deltab[HICHAR];
static int lastchfjump, lastchbjump;

/* Set start_line to NULL when not running a magic search!!
 * This defines the starting location for a search, and is used in boundry()
 * for reverse searching to add a boundary condition.
 */
static struct {
    struct line* start_line;
    int start_point;
} magic_search = { NULL, 0};

/* The rmagical variable determines whether there were actual metacharacters
 * in replace string - if not, then we can directly replace.
 * slow_scan will be set if we need the step_scanner() for searching.
 */
static int rmagical = FALSE;
static int magic_size = 0;
static struct magic *mcpat = NULL;              /* The magic pattern. */
static int magic_repl_size = 0;
static struct magic_replacement *rmcpat = NULL; /* Replacement magic array. */
static int mc_alloc = FALSE;                    /* Initial state */

static int slow_scan = FALSE;

/* Define an End of List struct mg_info type, with null flags */
static struct mg_info null_mg = { EGRP, 0, 0, 0, 0 };

/* Search ring buffer code... */
#define RING_SIZE 10

static char *srch_txt[RING_SIZE], *repl_txt[RING_SIZE];
/* This only needs to hold the Search/Query replace/etc. text */
char current_base[16] = "";

enum call_type {Search, Replace};  /* So we know the current call type */
static enum call_type this_rt;

/* Function to increase allocated sizes for group control info */
#define NGRP_INCR 10
static void increase_group_info(void) {
    max_grp += NGRP_INCR;
    cntl_grp_info = Xrealloc(cntl_grp_info,
         max_grp*sizeof(struct control_group_info));
    match_grp_info = Xrealloc(match_grp_info,
         max_grp*sizeof(struct match_group_info));
    grp_text = Xrealloc(grp_text, max_grp*sizeof(char *));
    for (int gi = 0; gi < max_grp; gi++) grp_text[gi] = NULL;
}

/* Functions to increase allocated sizes for magic patterns */
#define MAGIC_INCR 16
static void increase_magic_info(void) {
    magic_size += MAGIC_INCR;
    mcpat = Xrealloc(mcpat, magic_size*sizeof(struct magic));
    return;
}
static void increase_magic_repl_info(void) {
    magic_repl_size += MAGIC_INCR;
    rmcpat = Xrealloc(rmcpat,
         magic_repl_size*sizeof(struct magic_replacement));
    return;
}

/* Function to set things for the start of searching. */
void init_search_ringbuffers(void) {

/* Allocate all of these and set them to "".
 * update_ring() expects this.
 */
    for (int ix = 0; ix < RING_SIZE; ix++) {
        srch_txt[ix] = Xmalloc(1);
        terminate_str(srch_txt[ix]);
        repl_txt[ix] = Xmalloc(1);
        terminate_str(repl_txt[ix]);
    }

/* Allocate and initialize the arrays for group info.
 * Also initialize magic structures.
 */
    increase_group_info();
    increase_magic_info();
    increase_magic_repl_info();

/* Ensure that the first mcpat and rmcpat entries are "empty"
 * (it's used as a flag)
 */
    mcpat[0].mc = null_mg;
    rmcpat[0].mc = null_mg;

/* Ensure that pat and rpat are "", not NULL */

    db_set(pat, "");
    db_set(rpat, "");

    return;
}

/* The new string goes in place at the top, pushing the others down
 * and dropping the last one.
 * Actually done by rotating the bottom to the top then updating the top.
 * However - we don't want to push anything out if there is an empty entry.
 */
int nring_free = RING_SIZE;
static void update_ring(const char *str) {
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

/* expandp -- Expand control key sequences for output.
 *            Assumes caller has sent a large enough buffer.
 *
 * char *srcstr;                string to expand
 *  returns the expanded text in a dynamic buffer
 */
static db_strdef(expbuf);
static db *expandp(const char *srcstr) {
    unsigned char c;        /* current char to translate */

    db_set(expbuf, "");     /* NOT db_clear, to ensure val is not NULL */

/* Scan through the string. */

    while ((c = *srcstr++) != 0) {
        if (c == '\n') {    /* It's a newline */
            db_append(expbuf, "<NL>");
        }
        else if ((c > 0 && c < 0x20) || c == 0x7f) { /* Control character */
            db_addch(expbuf, '^');
            db_addch(expbuf, c ^ 0x40);
        }
        else {              /* Any other character */
            db_addch(expbuf, c);
        }
    }
    return &expbuf;
}

/* boundry -- Return information depending on whether we may search no
 *      further.  Beginning of file and end of file are the obvious
 *      cases, but we may want to add further optional boundary restrictions
 *      in future, a' la VMS EDT.  At the moment, just return TRUE or
 *      FALSE depending on if a boundary is hit (ouch).
 *      The end of file is now taken to be *on* the last line (b_linep)
 *      rather than at the end of the "last line with buffer text" so that
 *      we can include the terminating newline (which we always assume is
 *      there) in a search.
 */
static int boundry(struct line *curline, int curoff, int dir) {
    int border;

    if (dir == FORWARD) {
        border = (curline == curbp->b_linep);
    }
    else {
/* Reverse searching has an additional boundary to consider - the
 * original point.
 */
        if ((curline == magic_search.start_line) &&
            (curoff > magic_search.start_point)) {
             border = TRUE;
        }
        else {
/* A reverse slow scan has to be able to get to curoff == 0 on
 * the first line...
 */
            if (slow_scan)
                border = (curline == curbp->b_linep);
            else
                border = (curoff == 0) && (lback(curline) == curbp->b_linep);
	}
    }
    return border;
}

/* A function to regenerate the prompt string with the given
 * text as the default.
 * Also called from svar() if it sets $replace or $search
 */
void new_prompt(const char *dflt_str) {
    dbp_dcl(ep) = expandp(dflt_str);
    db_sprintf(prmpt_buf.prompt, "%s " MLpre "%s" MLpost ": ",
         current_base, dbp_val(ep));
    prmpt_buf.update = 1;
    return;
}

/* Here are the two callable search/replace string manipulating functions.
 * They are called from the CMPLT_* code in input.c when you are searching
 * or replacing.
 * For rotate_sstr the "natural" thing to do is to go back to the previous
 * string so +ve args go backwards in the array.
 */
void rotate_sstr(int n) {

    while (n < 0) n += RING_SIZE;       /* Make it +ve... */
    int rotator = n % RING_SIZE;        /* ...so this is not -ve */
    if (rotator == 0) return;           /* No movement */
    rotator = RING_SIZE - rotator;      /* So we go the right way */

/* Rotate by <n> by mapping into a temp array then memcpy back */

    char *tmp_txt[RING_SIZE];
    char **txt;
    db *t_db;
    if (this_rt == Search) {
        txt = srch_txt;
        t_db = &pat;
    }
    else {
        txt = repl_txt;
        t_db = &rpat;
    }
    for (int ix = 0; ix < RING_SIZE; ix++, rotator++) {
        rotator %= RING_SIZE;
        tmp_txt[rotator] = txt[ix];
    }
    memcpy(txt, tmp_txt, sizeof(tmp_txt));  /* Copy rotated array back */
    dbp_set(t_db, txt[0]);                  /* Update (r)pat */

/* We need to make getstring() show this new value in its prompt.
 * So we create what we want in prmpt_buf.prompt then set prmpt_buf.update
 * to tell getstring() to use it.
 */
    new_prompt(dbp_val(t_db));
    return;
}

/* Setting prmpt_buf.preload makes getstring() add it into the result buffer
 * at the start of its next get-character loop.
 * It will be inserted into any current search string at the current point.
 * Here as it needs to test this_rt.
 */
void select_sstr(void) {
    prmpt_buf.preload = (this_rt == Search)? srch_txt[0]: repl_txt[0];
    return;
}

/* clearbits -- Allocate and zero out a CCL bitmap.
 */
static char *clearbits(void) {
    char *cclstart;
    char *cclmap;
    int i;

    cclmap = cclstart = Xmalloc(BMBYTES);
    for (i = 0; i < BMBYTES; i++) *cclmap++ = 0;
    return cclstart;
}

/* Function to get text between {} braces.
 * It expects to be called with the first char *after* a brace, so
 * collects up to the next "}"
 * It does allow for internal {} groups.
 * It does not process the text in any way, so can do a byte-scan.
 * Since this returns the result in a static buffer the caller
 * must use this before calling here again.
 */
static db_strdef(btbuf);
static db *brace_text(char *fp) {
    int escaping = 0;
    int level = 0;

    db_set(btbuf, "");      /* NOT db_clear, to ensure val is not NULL */
    while (1) {
        if (!*fp) break;
        if (!escaping && (*fp == '\\')) {
            escaping = 1;
        }
        else {
            escaping = 0;
            db_addch(btbuf, *fp);
            if (*fp == '{') level++;
        }
        fp++;
        if (!escaping && (*fp == '}')) {
            if (level-- > 0) continue;
            return &btbuf;
        }
    }
    return NULL;
}

/* There are following routines with a lot of conditional blocks.
 * This pushes the code over to the right and can make it more difficult
 * to follow because of resulting code line wrapping.
 * The following macros can be used for the start/end of block tests,
 * which mean that uemacs won't try to indent to the wrong level.
 * NOTE! that these must NOT have a trailing ; when used!
 */
#define TEST_BLOCK(x) if (x) {
#define END_TEST(x) }
#define WHILE_BLOCK(x) while (x) {
#define END_WHILE(x) }

/* setbit -- Set a bit (ON only) in the bitmap.
 * This is used for all 256 values in non-Magic (fast_scanner) mode.
 */
static void setbit(int bc, char *cclmap) {
    bc = bc & 0xFF;
    if (bc < HICHAR) *(cclmap + (bc >> 3)) |= BIT(bc & 7);
}

/* parse_error
 * A simple routine to display where errors occur in patterns
 * Assumes that pptr arrives set to one beyond the error point AND
 * that this is a pointer into pat!
 */
static void parse_error(char *pptr, char *message) {    /* Helper routine */
    char byte_saved = *pptr;
    terminate_str(pptr);    /* Temporarily fudge in end-of-string */
    mlwrite("%s<-: %s!", db_val(pat), message);
    *pptr = byte_saved;
    return;
}

/* add2_xt_cclmap - add a new entry to the end of the xt_cclmap list
 *                  returns the ptr to the new entry *not* the base of
 *                  the list (which is stored in mp->x.xt_cclmap.
 */
static int uni_ccl_cnt;     /* add2_xt_cclmap isn't recursive... */
static struct xccl *add2_xt_cclmap(struct magic *mp, int type) {
    uni_ccl_cnt++;
    struct xccl *xp =
         Xrealloc(mp->x.xt_cclmap, (uni_ccl_cnt)*sizeof(struct xccl));
    mp->x.xt_cclmap = xp;
    xp += uni_ccl_cnt - 1;  /* Point to the last one */
    xp->xc = null_mg;       /* (re)Mark end of list */
    xp--;                   /* Where we want to add the new one */
    xp->xc.type = type;
    return xp;
}

/* cclmake -- create the bitmap for the character class.
 *      On success ppatptr is left pointing to the end-of-character-class
 *      character, so that a loop may automatically increment with safety.
 *      - indicates a range, but it is literal if the first or last character
 *      ] is literal if the first character (so you can't have an empty class!)
 *      [ is literal anywhere
 */
static struct grapheme null_gc = { UEM_NOCHAR, 0, NULL };
static int cclmake(char **ppatptr, struct magic *mcptr) {
    char *bmap;
    char *patptr;
    dbp_dcl(btext);

/* We *always* set up the bitmap structure, so mc_alloc must be set */

    mcptr->val.cclmap = bmap = clearbits();
    mc_alloc = TRUE;
    patptr = *ppatptr;

/* Test the initial character(s) in ccl for the special cases of negate ccl.
 * Anything else gets set in the class mapping.
 */
    mcptr->mc.type = CCL;
    if (*++patptr == MC_NCCL) {
        patptr++;
        mcptr->mc.negate_test = 1;
    }
    else
        mcptr->mc.negate_test = 0;
    mcptr->x.xt_cclmap = NULL;      /* So we can Xrealloc on it */
    uni_ccl_cnt = 1;                /* Space for EGRP(0) at end if used */

/* We can't handle things as we reach them, as we might have a range defined
 * and it's only when we get to the '-' (MC_RCCL) that we know this
 * so we must save each character until we know what the next one is.
 */
    struct grapheme prev_gc =  null_gc;
    int first = 1;
    int ccl_ended = 0;

/* Really need to run this through one final time when MC_ECCL is seen
 * Seeing NUL is an error!!!
 * We always get graphemes...
 * patptr will be pointing to the next unread char through the loop.
 */
    WHILE_BLOCK(*patptr)
    struct grapheme gc;
    patptr += build_next_grapheme(patptr, 0, -1, &gc, 0);

/* Check for a range character.
 * But it will be taken literally if it is the first character or the
 * last character in the class. See if it is...
 */
    TEST_BLOCK(!first && gc.uc == MC_RCCL && gc.cdm == 0)

/* We can also have a range based on Unicode chars.
 * So, what do we have next...let's look.
 * We overwrite gc here, but we know it was MC_RCCL, so can not have
 * alloc()ed any ex part.
 */
    patptr += build_next_grapheme(patptr, 0, -1, &gc, 0);
    if (gc.uc == MC_ECCL && gc.cdm == 0) {  /* - was at end -> literal */
        setbit(MC_RCCL, bmap);  /* ...so mark it */
        goto handle_prev;       /* to handle previous char and exit loop */
    }
    unicode_t low, high;
    if (gc.cdm || prev_gc.cdm) {
        parse_error(patptr, "Cannot use combined chars in ranges");
        if (gc.ex) {            /* Our responsibility! */
            Xfree_setnull(gc.ex);
        }
        if (prev_gc.ex) {       /* Our responsibility! */
            Xfree_setnull(prev_gc.ex);
        }
        goto error_exit;
    }
    low = prev_gc.uc;
    high = gc.uc;

/* If high > MAXASCII then we need to use an extended CCL, otherwise
 * we can set the bit pattern.
 */
    if (high <= MAXASCII) {
        while (low <= high) setbit(low++, bmap);
    }
    else {
        struct xccl *xp = add2_xt_cclmap(mcptr, CCL);
        xp->xval.cl_lim.low = low;
        xp->xval.cl_lim.high = high;
    }
/* We've used the current character, so remove it...(gc.ex cannot be set) */
    goto invalidate_current;
    END_TEST(!first....)        /* End of range handling */

/* For the first char/grapheme there is no more to do - except to handle
 * the loop transition.
 * We must not go to switch_current_to_prev, as that checks for the end
 * of the class if current char is MC_ECCL. And on the first pass
 * MC_ECCL is *NOT* the end of the class, but a literal.
 */
    if (first) goto loop_transition;

/* Now deal with the *previously* seen item
 * Do we have a previous non-ASCII grapheme?
 */
handle_prev:
    if (prev_gc.uc > 0x7f) {    /* We have Unicode to handle */
/* We set the real type later... */
        struct xccl *xp = add2_xt_cclmap(mcptr, 0);
        if (prev_gc.cdm) {
            xp->xc.type = UCGRAPH;
/* Structure copy - will copy any malloced gc.ex (i.e. copy the pointer),
 * so we don't need to free() it at the end if all has gone OK.
 */
            xp->xval.gc = prev_gc;
            prev_gc.ex = NULL;  /* So we don't try to free it... */
        }
        else {
            xp->xc.type = UCLITL;
            xp->xval.uchar = prev_gc.uc;
        }
        goto switch_current_to_prev;
    }

/* Must have an ASCII char now (in prev_gc.uc) */

    switch (prev_gc.uc) {
    case MC_ESC:
        if ((gc.uc > MAXASCII) || (gc.cdm)) {
            parse_error(patptr, "Attempt to quote non-ASCII");
            Xfree(gc.ex);
            return FALSE;
        }
/* So from here on we know there is no gc.ex to free */
        switch (gc.uc) {     /* All MUST finish with goto!! */
        case 'u': {
            if (*patptr != '{') {
                parse_error(patptr, "\\u{} not started");
                return FALSE;
            }
            btext = brace_text(++patptr);
            if (!btext) {
                parse_error(patptr, "\\u{} not ended");
                return FALSE;
            }
            struct xccl *xp = add2_xt_cclmap(mcptr, UCLITL);
            xp->xval.uchar = strtol(dbp_val(btext), NULL, 16);

/* For the moment(?), don't validate we have a hex-only field) */
            patptr += dbp_len(btext);
            goto invalidate_current;
        }
        case 'k':       /* "kind of" char - just check base unicode */
        case 'K':       /* Not "kind of" ... */
            if (*(patptr) != '{') { /* } balancer */
                parse_error(patptr, "\\k/K{} not started");
                return FALSE;
            }
            btext = brace_text(++patptr);
            if (!btext) {
                parse_error(patptr, "\\k/K{} not ended");
                return FALSE;
            }
            struct grapheme kgc;
            int klen = dbp_len(btext);
/* Don't build any ex... */
            int bc = build_next_grapheme(dbp_val(btext), 0, klen, &kgc, 1);
            if (bc != klen) {
                parse_error(patptr,
                     "\\k{} must only contain one unicode char");
                return FALSE;
            }
            if (kgc.cdm) {
                parse_error(patptr,
                     "\\k{} must only contain a base character");
                return FALSE;
            }
            struct xccl *xp = add2_xt_cclmap(mcptr, UCKIND);
            xp->xval.uchar = kgc.uc;
            xp->xc.negate_test = (gc.uc == 'K');
            patptr += dbp_len(btext);
            goto invalidate_current;
        case 'p':
        case 'P': {
            if (*patptr != '{') {       /* balancer: } */
                parse_error(patptr, "\\p/P{} not started");
                return FALSE;
            }
            btext = brace_text(++patptr);
            if (!btext) {
                parse_error(patptr, "\\p/P{} not ended");
                return FALSE;
            }
            struct xccl *xp = add2_xt_cclmap(mcptr, UCPROP);
            xp->xc.negate_test = (gc.uc == 'P');
/* For the moment(?), don't validate what we have.
 * Copy up to 2 chars (no more) and force the first to be Upper case and the
 * second, if there, to lower. This allows a simple comparison with the
 * result of a utf8proc_category_string() call.
 */
            xp->xval.prop[0] = '?';     /* Unknown default */
            terminate_str(xp->xval.prop + 1);
            if (dbp_charat(btext, 0))
                 xp->xval.prop[0] = dbp_charat(btext, 0) & (~DIFCASE);
            if (dbp_charat(btext, 1))
                 xp->xval.prop[1] = dbp_charat(btext, 1) | DIFCASE;
            terminate_str(xp->xval.prop + 2);   /* Ensure NUL terminated */
            patptr += dbp_len(btext);
            goto invalidate_current;
	}
        case 'd':
        case 'D': {
            struct xccl *xp = add2_xt_cclmap(mcptr, UCPROP);
            xp->xc.negate_test = (gc.uc == 'D');
            xp->xval.prop[0] = 'N';     /* Numeric... */
            xp->xval.prop[1] = 'd';     /* ...digit */
            terminate_str(xp->xval.prop + 2);   /* Ensure NUL terminated */
            goto invalidate_current;
        }
        case 'w':                       /* Letter, _, 0-9 */
        case 'W': {
            int negate_it = (gc.uc == 'W');
            struct xccl *xp = add2_xt_cclmap(mcptr, UCPROP);
            xp->xc.negate_test = negate_it;
            xp->xval.prop[0] = 'L';     /* Letter... */
            terminate_str(xp->xval.prop + 1);   /* Ensure NUL terminated */
/* We can't really (un)set a bit pattern here for the negative case,
 * So set up some UCLITL tests instead.
 */
            xp = add2_xt_cclmap(mcptr, UCLITL);
            xp->xc.negate_test = negate_it;     /* We have a new xp */
            xp->xval.uchar = ch_as_uc('_');
            xp = add2_xt_cclmap(mcptr, CCL);
            xp->xc.negate_test = negate_it;     /* We have a new xp */
            xp->xval.cl_lim.low = ch_as_uc('0');
            xp->xval.cl_lim.high = ch_as_uc('9');
            goto invalidate_current;
        }
        case 's':               /* \s is Whitespace */
        case 'S': {
/* We need to add a test for the Unicode "Pattern White Space" chars
 *  U+0009 CHARACTER TABULATION
 *  U+000A LINE FEED
 *  U+000B LINE TABULATION
 *  U+000C FORM FEED
 *  U+000D CARRIAGE RETURN
 *  U+0020 SPACE
 *  U+0085 NEXT LINE
 *  U+200E LEFT-TO-RIGHT MARK
 *  U+200F RIGHT-TO-LEFT MARK
 *  U+2028 LINE SEPARATOR
 *  U+2029 PARAGRAPH SEPARATOR
 */
            static int wsp_ints[] =
                 { 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x20, 0x85,
                   0x200E, 0x200F, 0x2028, 0x2029, UEM_NOCHAR };
            int negate_it = (gc.uc == 'S');
            struct xccl *xp;
            for (int *wsp = wsp_ints; *wsp != UEM_NOCHAR; wsp++) {
                xp = add2_xt_cclmap(mcptr, UCLITL);
                xp->xc.negate_test = negate_it;
                xp->xval.uchar = *wsp;
            }
            goto invalidate_current;
        }

/* For consistency (and particularly as a way to add a newline to the
 * negative-match case) allow these...
 */
        case 'n':
            setbit('\n', bmap);
            goto invalidate_current;
        case 'r':
            setbit('\r', bmap);
            goto invalidate_current;
        case 'f':
            setbit('\f', bmap);
            goto invalidate_current;
        case 't':
            setbit('\t', bmap);
            goto invalidate_current;
/* NOTE: that this means \\ is taken to mean \ in a class */
        default:            /* Set bit for current char */
            setbit(gc.uc, bmap);
            goto invalidate_current;
        }
    default:                /* Not MC_ESC - normal processing */
        setbit(prev_gc.uc, bmap);
        goto switch_current_to_prev;
    }
/* We must NOT drop to here from above!!! */
    parse_error(patptr, "Parsing code problem in cclmake!");
    goto error_exit;

/* We are sent here if the loop has already dealt with the current
 * character, in order to fetch another one...
 * For it to have done so, gc.ex cannot be set.
 */
invalidate_current:
    patptr += build_next_grapheme(patptr, 0, -1, &gc, 0);

switch_current_to_prev:
    if (gc.uc == MC_ECCL) {     /* We've hit the end! */
        ccl_ended = 1;
        break;  /* All done */
    }
loop_transition:
    first = 0;
    prev_gc = gc;
    END_WHILE(*patptr)

/* Caller expects us to leave this at "last handled", and we are on next
 * to be handled.
 */
    *ppatptr = --patptr;

    if (ccl_ended) return TRUE;     /* All looks OK */

    parse_error(patptr, "Character class not ended");

error_exit:
    Xfree(bmap);
/* Must also free any extended grapheme parts in any UCGRAPH entries. */
    struct xccl *xp = mcptr->x.xt_cclmap;
    if (xp) while (xp->xc.type != EGRP) {
        if (xp->xc.type == UCGRAPH) Xfree(xp->xval.gc.ex);
        xp++;
    }
    return FALSE;
}

/* biteq -- is the character in the bitmap?
 */
static int biteq(int bc, char *cclmap) {
    bc = bc & 0xFF;
    if (bc >= HICHAR) return FALSE;

    return (*(cclmap + (bc >> 3)) & BIT(bc & 7)) ? TRUE : FALSE;
}

/* set_lims - get the low/high values for a repeating range.
 * Recurses (once at most).
 */
static char *set_lims(char *patp, struct magic *mcp, int first_call) {
    char *rptr = patp + 1;      /* Remember where we start */
    char rchr;
    while ( (rchr = *(++patp)) != '\0' ) {
        if (rchr < '0' || rchr > '9') break;    /* End of numbers */
    }
    if ((rchr == ',') && first_call) {  /* OK */
        mcp->cl_lim.low = atoi(rptr);
        return set_lims(patp, mcp, 0);
    }
    if ((rchr == '}') && first_call) {  /* OK */
        mcp->cl_lim.low = atoi(rptr);
        mcp->cl_lim.high = mcp->cl_lim.low;
        return patp;
    }
/* Not a first_call, so we MUST end on a '}' */
    if (rptr == patp) mcp->cl_lim.high = INT_MAX; /* Empty -> MAX */
    else              mcp->cl_lim.high = atoi(rptr);
    if ((rchr != '}') || mcp->cl_lim.high < mcp->cl_lim.low) {
        parse_error(patp, "Invalid range pattern");
        return NULL;
    }
    return patp;
}

/* mcclear -- Free up any CCL bitmaps, and reset the struct magic search
 * array.
 */
static void mcclear(void) {
    struct magic *mcptr = mcpat;

/* Keep going until we reach the End group entry for Group 0 */
    while (!((mcptr->mc.type == EGRP) && (mcptr->mc.group_num == 0))) {
        if ((mcptr->mc.type) == CCL) {
            Xfree(mcptr->val.cclmap);
            if (mcptr->x.xt_cclmap) {
                struct xccl *xp = mcptr->x.xt_cclmap;
/* Free any extended gc allocs within the xccl alloc */
                while(xp->xc.type != EGRP) {
                    if (xp->xc.type == UCGRAPH) Xfree(xp->xval.gc.ex);
                    xp++;
                }
                Xfree(mcptr->x.xt_cclmap);
            }
	}
        if ((mcptr->mc.type) == UCGRAPH) Xfree(mcptr->val.gc.ex);
        mcptr++;
    }
/* We need to reset type and flags here */
    mcpat[0].mc = null_mg;
}

/* rmcclear -- Free up any extended unicode chars, varnames and reset
 * the struct magic_replacement array.
 */
static void rmcclear(void) {
    struct magic_replacement *rmcptr;

    rmcptr = rmcpat;

    while (rmcptr->mc.type != EGRP) {
        switch(rmcptr->mc.type) {
        case UCGRAPH:
            Xfree (rmcptr->val.gc.ex);
            break;
        case REPL_VAR:
            Xfree(rmcptr->val.varname);
            break;
        case REPL_CNT:
            Xfree(rmcptr->val.x.fmt);
            break;
        case REPL_FNC: {
            struct func_call *fcp = rmcptr->val.fc;
            while (fcp) {
                switch(fcp->type) {
                case LITCHAR:
                    Xfree(fcp->val.ltext);
                    break;
                case REPL_CNT:
                    Xfree(fcp->val.x.fmt);
                    break;
                default:
                    ;
                }
                struct func_call *next_fcp = fcp->next;
                Xfree(fcp);
                fcp = next_fcp;
            }
            break;
        }
        default:
            ;
        }
        rmcptr++;
    }
    rmcpat[0].mc = null_mg;
}

/* mcstr -- Set up the 'magic' array.  The closure symbol is taken as
 *      a literal character when (1) it is the first character in the
 *      pattern, and (2) when preceded by a symbol that does not allow
 *      repeat, such as a newline, beginning of line symbol, or another
 *      repeat symbol.
 *
 *      Coding comment (jmg):  yes, I know I have gotos that are, strictly
 *      speaking, unnecessary.  But right now we are so cramped for
 *      code space that i will grab what i can in order to remain
 *      within the 64K limit.  C compilers actually do very little
 *      in the way of optimizing - they expect you to do that.
 *
 *      NOTE! This code works out whether we need to use step_scanner()
 *      (Unicode search string or active regex chars found while in
 *       MAGIC mode) or fast_scanner().
 *
 *      ALSO NOTE! It expects its caller to call mcclear() if this fails.
 *      This allows a quick "return FALSE" on any parsing error.
 *      BUT!! we do need to ensure that the final type is EGRP(0) at all
 *      times to allow this!!
 */
static int group_cntr;  /* Number of possible groups in search pattern */
static int mcstr(void) {
    struct magic *mcptr = mcpat;
    char *patptr = strdupa(db_val(pat));
    int mj;
    int pchr;
    int status = TRUE;
    int repeat_allowed = FALSE; /* Set to can_repeat at end of each loop */
    dbp_dcl(btext);

/* If we allocated anything in the previous mcpat, free it now.
 */
    if (mc_alloc) mcclear();
    mc_alloc = FALSE;
    group_cntr = 0;

    cntl_grp_info[0].state = GPOPEN;    /* But group 0 is always OPEN */
    cntl_grp_info[0].parent_group = -1; /* None */
    int curr_group = 0;

/* Each group has to start with a group-holder, so that we
 * can chain any OR groups from it.
 */
    mcptr->mc = null_mg;            /* Initialize fields */
    mcptr->mc.type = SGRP;
    mcptr->mc.group_num = group_cntr;
    mcptr->x.next_or_idx = 0;
    cntl_grp_info[0].next_choice_idx = 0;   /* Where to put the next CHOICE */
    mcptr++;
    mj = 1;     /* This is the index in mcpat that mcptr points to!! */

    slow_scan = FALSE;      /* Assume not, until something needs it */

    WHILE_BLOCK((pchr = *patptr) && status)
    int can_repeat = TRUE;  /* Will be switched off as required */
    int possible_slow_scan = FALSE;     /* Not yet */
    mcptr->mc = null_mg;    /* Initialize fields */
    mcptr->mc.group_num = curr_group;
/* Is the next character non-ASCII? */
    struct grapheme gc;
    int bc = build_next_grapheme(patptr, 0, -1, &gc, 0);
    if (gc.uc > 0x7f || gc.cdm) {   /* not-ASCII */

/* We won't have called mcstr() for Exact + non-Magic, so if we
 * find a non-ASCII code we need to use the step_scanner() if we are
 * non-Exact, as non-ASCII case folding doesn't work with the fast_scanner.
 */
        if (!(curwp->w_bufp->b_mode & MDEXACT))
            slow_scan = TRUE;   /* Only case that sets this for non-MAGIC */
        patptr += bc;
        if (!gc.cdm)    {   /* No combining marks */
            mcptr->mc.type = UCLITL;
            mcptr->val.uchar = gc.uc;
        }
        else {
            mcptr->mc.type = UCGRAPH;
/* We copy all of the data into our saved one. This means that any
 * malloc'ed ex parts get their pointers copied, and there is no more to
 * do here. Any freeing will be done in mcclear().
 */
            mcptr->val.gc = gc;
            mc_alloc = TRUE;
        }
        goto pchr_done_noincr;  /* patptr was incremented a few lines back */
    }

/* IMPORTANT!!!!
 * If we AREN'T using magic then EVERYTHING is literal!!!
 * And since we know we have a bare ASCII char (any other would be trapped
 * in the preceding test) we just set a LITCHAR test here.
 */
    if (!(curwp->w_bufp->b_mode & MDMAGIC)) {   /* Non-magic mode ASCII */
        mcptr->mc.type = LITCHAR;
        mcptr->val.lchar = pchr;
        goto pchr_done;
    }

/* From now on anything we do, EXCEPT the default LITCHAR case (but EXCLUDING
 * the drop-though case from MS_ESC(\) handling) involves producing a
 * slow_scan control structure that is NOT the same as the literal characters
 * in apat, meaning we can't use the literal input, so must use the slow scan
 * (even a simple \n -> '\n' change requires this!).
 */
    possible_slow_scan = TRUE;

/* We have an unescaped char to look at... */
    switch (pchr) {
    case MC_CCL:
        status = cclmake(&patptr, mcptr);
        break;
    case MC_SGRP:
        group_cntr++;       /* Create a "new" group */
        if (group_cntr >= max_grp) increase_group_info();
        cntl_grp_info[group_cntr].state = GPOPEN;
        cntl_grp_info[group_cntr].parent_group = curr_group;
        curr_group = group_cntr;
        mcptr->mc.type = SGRP;
        mcptr->mc.group_num = group_cntr;               /* Set correct one */
        mcptr->x.next_or_idx = 0;                       /* No OR yet but... */
        cntl_grp_info[group_cntr].next_choice_idx = mj; /* it would go here */
        can_repeat = FALSE;
        break;
    case MC_EGRP:
        mcptr->mc.type = EGRP;  /* Close group */
        curr_group = cntl_grp_info[curr_group].parent_group;
/* We close the highest open group... */
        int closed_ok = 0;
        for (int gc = group_cntr; gc > 0; gc--) {
            if (cntl_grp_info[gc].state == GPOPEN) {
                cntl_grp_info[gc].state = GPVALID;
                cntl_grp_info[gc].gpend = mj;
                closed_ok = 1;
                break;
            }
        }
        if (!closed_ok) {
            parse_error(patptr+1, "no group to end");
            return FALSE;
        }
        break;
    case MC_BOL:
        if (mj != 1) {
            parse_error(patptr+1, "must be first");
            return FALSE;
        }
        mcptr->mc.type = BOL;
        can_repeat = FALSE;
        break;
    case MC_EOL:
        if (*(patptr + 1) != '\0') {
            parse_error(patptr+1, "must be last");
            return FALSE;
        }
        mcptr->mc.type = EOL;
        can_repeat = FALSE;
        break;
    case MC_ANY:
        mcptr->mc.type = ANY;
        break;
    case MC_REPEAT:
    case MC_ONEPLUS:
/* Does the closure symbol mean repeat here? If so, back up to the
 * previous element and indicate it is enclosed.
 * If not, that's an error...
 */
        if (!repeat_allowed) {
            parse_error(patptr+1, "repeating a non-repeatable item");
            return FALSE;
        }
        mj--;
        mcptr--;
        mcptr->mc.repeat = 1;
        mcptr->cl_lim.low = (pchr == MC_ONEPLUS)? 1: 0;
        mcptr->cl_lim.high = INT_MAX;   /* Not quite infinity */
        can_repeat = FALSE;
        break;
    case MC_RANGE:
        if (!repeat_allowed) {
            parse_error(patptr+1, "repeating a non-repeatable item");
            return FALSE;
        }
        mj--;
        mcptr--;
        mcptr->mc.repeat = 1;
/* Need to get the limits here. Allow {m,n}, {,n}, {m,}, {,}, {}.
 * Missing m is taken as 0. Missing n as INT_MAX (2**31 -1).
 * m must be <= n
 */
        patptr = set_lims(patptr, mcptr, 1);
        if (!patptr) return FALSE;
        can_repeat = FALSE;
        break;
    case MC_MINIMAL:
        if (mj <= 1) {
            parse_error(patptr+1, "nothing to minimally repeat");
            return FALSE;
        }
/* Not at start, so if we can do repeat we set it up for 0/1
 * and, if we can't, we check that the previous entry IS a repeat
 * and set that for minimal matching, complaining if it isn't
 * a repeat or is already minimized.
 */
        mj--;
        mcptr--;
        if (repeat_allowed) {
            mcptr->mc.repeat = 1;
            mcptr->cl_lim.low = 0;
            mcptr->cl_lim.high = 1;
            mcptr->mc.min_repeat = 0;   /* Greedy by default */
        }
        else {
            if (!(mcptr->mc.repeat)) {
                parse_error(patptr, "minimalizing a non-repeatable item");
                return FALSE;
            }
            if (mcptr->mc.min_repeat) {
                parse_error(patptr, "already minimized repeat");
                return FALSE;
            }
            mcptr->mc.min_repeat = 1;   /* Now non-greedy */
        }
        can_repeat = FALSE;
        break;
/* When we have an OR we link its location back into the previous OR
 * (or the start of the group if there is no previous OR) using next_or_index.
 * This means we can try each option in turn and if it fails, move onto any
 * following one.
 */
    case MC_OR:
        mcptr->mc.type = CHOICE;
/* Add this to the OR chain for this group and mark where next OR
 * has to be added.
 * Now uses indexed entries, not pointers.
 */
/* No following OR from here yet */
        mcptr->x.next_or_idx = 0;
/* Set the previous choice to point here (linking) */
        mcpat[cntl_grp_info[curr_group].next_choice_idx].x.next_or_idx = mj;
/* Remember where any folowing choice needs to put its chaining link */
        cntl_grp_info[curr_group].next_choice_idx = mj;
        can_repeat = FALSE;
        break;

/* MC_ESC is no longer just "escape next char"
 * For Unicode regexes it can also be the lead-in for:
 *  X       any Unicode grapheme
 *  u       Unicode char in hex (as in \u{0062})
 *  p       match Unicode property (e.g. \p{L} or \p{LU} \p{LL}...)
 *  P       Not match Unicode property
 * and we also allow:
 *  s       whitespace
 *  n       newline
 *  r       carriage return
 *  f       form feed
 *  t       tab
 */
    case MC_ESC:            /* ALL of these can repeat */
        pchr = *(patptr + 1);
        if (pchr) {         /* MC_ESC at EOB is taken literally... */
            patptr++;       /* Advance to next char */
            switch (pchr) { /* The first few MUST finish with goto!! */
            case 'X':
                mcptr->mc.type = ANYGPH;
                goto pchr_done;
/* These all use a {} text fetcher */
            case 'u':       /* Unicode char in hex */
                patptr++;
                if (*(patptr) != '{') { /* } balancer */
                    parse_error(patptr, "opening { expected");
                    return FALSE;
                }
                btext = brace_text(++patptr);
                if (!btext) {
                    parse_error(patptr, "found no closing }");
                    return FALSE;
                }
                mcptr->mc.type = UCLITL;
/* For the moment(?), don't validate we have a hex-only field) */
                mcptr->val.uchar = strtol(dbp_val(btext), NULL, 16);
                patptr += dbp_len(btext);
                goto pchr_done;
            case 'k':       /* "kind of" char - just check base unicode */
            case 'K':       /* Not "kind of" ... */
                patptr++;
                if (*(patptr) != '{') { /* } balancer */
                    parse_error(patptr, "opening { expected");
                    return FALSE;
                }
                btext = brace_text(++patptr);
                if (!btext) {
                    parse_error(patptr, "found no closing }");
                    return FALSE;
                }
                struct grapheme kgc;
                int klen = dbp_len(btext);
/* Don't build any ex... */
                int bc = build_next_grapheme(dbp_val(btext), 0, klen, &kgc, 1);
                if (bc != klen) {
                    parse_error(patptr+1,
                         "\\k{} must only contain one unicode char");
                    return FALSE;
                }
                if (kgc.cdm) {
                    parse_error(patptr+1,
                         "\\k{} may only contain a base character");
                    return FALSE;
                }
                mcptr->mc.type = UCKIND;
                mcptr->val.uchar = kgc.uc;
                mcptr->mc.negate_test = (pchr == 'K');
                patptr += dbp_len(btext);
                goto pchr_done;
            case 'p':
            case 'P':
                patptr++;
                if (*(patptr) != '{') { /* } balancer */
                    parse_error(patptr, "opening { expected");
                    return FALSE;
                }
                btext = brace_text(++patptr);
                if (!btext) {
                    parse_error(patptr, "found no closing }");
                    return FALSE;
                }
                mcptr->mc.type = UCPROP;
/* For the moment(?), don't validate what we have.
 * Copy up to 2 chars (no more) and force the first to be Upper case and the
 * second, if there, to lower. This allows a simple comparison with the
 * result of a utf8proc_category_string() call.
 */
                mcptr->val.prop[0] = '?';     /* Fail by default */
                terminate_str(mcptr->val.prop + 1);
                if (dbp_charat(btext, 0))
                     mcptr->val.prop[0] = dbp_charat(btext, 0) & (~DIFCASE);
                if (dbp_charat(btext, 1))
                     mcptr->val.prop[1] = dbp_charat(btext, 1) | DIFCASE;
                terminate_str(mcptr->val.prop + 2); /* Ensure NUL terminated */
                mcptr->mc.negate_test = (pchr == 'P');
                patptr += dbp_len(btext);
                goto pchr_done;
            case 'd':
            case 'D':
                mcptr->mc.type = UCPROP;
                mcptr->val.prop[0] = 'N';   /* Numeric... */
                mcptr->val.prop[1] = 'd';   /* ...digit */
                terminate_str(mcptr->val.prop + 2); /* Ensure NUL terminated */
                mcptr->mc.negate_test = (pchr == 'D');
                goto pchr_done;
/* w/W is word chars and s/S is whitespace.
 * All four can be passed off to cclmake() generically.
 */
            case 'w':                       /* Letter, _, 0-9 */
            case 'W':
            case 's':
            case 'S': {
                static char SETTER[] = "[\\X]";
                SETTER[2] = (pchr | DIFCASE);   /* Quick lowercase ASCII */
                char *dpatptr = SETTER;
                (void)cclmake(&dpatptr, mcptr);
                mcptr->mc.negate_test = !(pchr & DIFCASE);  /* If UPPER */
                goto pchr_done;
            }

            case 'n':   /* All 4 handled similarily */
            case 'r':
            case 'f':
            case 't':
                switch(pchr) {  /* Need to map to actual character */
                case 'n':
                    pchr = '\n';
                    break;
                case 'r':
                    pchr = '\r';
                    break;
                case 'f':
                    pchr = '\f';
                    break;
                case 't':
                    pchr = '\t';
                    break;
                }   /* Falls through */
            default:        /* Still slow_scan, for the unneeded \ */
                mcptr->mc.type = LITCHAR;
                mcptr->val.lchar = pchr;
                goto pchr_done; /* To skip possible_slow_scan reset */
            }
        } /* Falls through */
    default:
        mcptr->mc.type = LITCHAR;
        mcptr->val.lchar = pchr;
        possible_slow_scan = FALSE; /* Not from this one, at least */
        break;
    }               /* End of switch on original pchr */
pchr_done:
    patptr++;
pchr_done_noincr:
    mj++;           /* So now has value of the next 0-based mcpat entry */
    mcptr++;
/* Do we need to increase magic size? */
    if (mj >= magic_size) {
        int ptr_offs = mcptr - mcpat;   /* Allow for mcpat moving */
        increase_magic_info();
        mcptr = mcpat + ptr_offs;
    }

    if (possible_slow_scan) slow_scan = TRUE;
    repeat_allowed = can_repeat;    /* Can the next pass be a repeat? */
    END_WHILE(pchr = *patptr....)   /* End of while. */

/* Close off the meta-string. */
    mcptr->mc = null_mg;

/* We close the highest open group... */
    int closed_ok = 0;
    for (int gc = group_cntr; gc >= 0; gc--) {
        if (cntl_grp_info[gc].state == GPOPEN) {
            cntl_grp_info[gc].state = GPVALID;
            cntl_grp_info[gc].gpend = mj;
            closed_ok = 1;
            break;
        }
    }
    if (!closed_ok) {
        parse_error(patptr, "unbalanced group at end");
        return FALSE;
    }

    if (cntl_grp_info[0].state != GPVALID) {    /* Not Closed at end == error */
        parse_error(patptr-1, "unterminated grouping");
        return FALSE;
    }

/* Remove all of the cntl_grp_info next_choice_idx items.
 * amatch() will add the relevant value as required, but we need to be able
 * to check whether it has added any.
 */

    for (int gi = 0; gi <= group_cntr; gi++) {
        cntl_grp_info[gi].next_choice_idx = 0;
        cntl_grp_info[gi].state = GPIDLE;
    }

/* The only way the status would be bad is from the cclmake() routine,
 * and the bitmap for that member is guaranteed to be freed.
 */
    return status;
}

/* An internal function to create a new ("NULL") func_call item */
static struct func_call *new_fc(void) {
    struct func_call *retval = Xmalloc(sizeof(struct func_call));
    retval->type = EOL;
    retval->next = NULL;
    return retval;
}

/* rmcstr -- Set up the replacement 'magic' array.  Note that if there
 *      are no meta-characters encountered in the replacement string,
 *      the array is never actually created - we will just use the
 *      character array rpat[] as the replacement string.
 */
static int rmcstr(void) {
    int rmj = 0;        /* Entry counter */
    struct magic_replacement *rmcptr = rmcpat;
    char *patptr = strdupa(db_val(rpat));
    dbp_dcl(btext);

/* If we had metacharacters in the struct magic_replacement array previously,
 * free up any parts that may have been allocated (before we reset slow_scan).
 * NOTE: That we only free things befoe we create the next one.
 * So we have to call here from valgrind's DO_FREE section at the end.
 */
    if (rmagical) rmcclear();
    rmagical = FALSE;

    while (*patptr) {
        rmcptr->mc = null_mg;       /* Initialize fields */

/* Is the next character non-ASCII?
 * If so we know it can't be a control sequence (the only one of which
 * starts with a '$').
 */
        struct grapheme gc;
        int bc;
        bc = build_next_grapheme(patptr, 0, -1, &gc, 0);
        if (gc.uc > 0x7f || gc.cdm) {   /* not-ASCII */
            patptr += bc;
            if (!gc.cdm) {  /* No combining marks */
                rmcptr->mc.type = UCLITL;
                rmcptr->val.uchar = gc.uc;
            }
            else {
                rmcptr->mc.type = UCGRAPH;
/* We copy all of the data into our saved one. This means that any
 * malloc'ed ex parts get their pointers copied, and there is no more to
 * do here. Any freeing will be done in mcclear().
 */
                rmcptr->val.gc = gc;
            }
            goto pchr_done_noincr;
        }

        switch (*patptr) {
        case MC_REPL:
            patptr++;
            if (*patptr != '{') {   /* balancer: } */
                parse_error(patptr, "$ without {...}");
                return FALSE;
            }
            btext = brace_text(++patptr);
            if (!btext) {
                parse_error(patptr, "${} not ended");
                return FALSE;
            }
            int patptr_advance = dbp_len(btext);

/* What do we have?
 * Can be $var/%var/.var, @ (for a counter) or number...
 */
            switch(dbp_charat(btext, 0)) {
            case '$':
            case '%':
            case '.':
                rmcptr->mc.type = REPL_VAR;
/* Adding 1 for NUL */
                rmcptr->val.varname = Xmalloc(dbp_len(btext)+1);
                strcpy(rmcptr->val.varname, dbp_val(btext));
                break;
            case '@':   /* Replace with a counter - optional formatting */
                rmcptr->mc.type = REPL_CNT;
/* Defaults... */
                rmcptr->val.x.curval = 1;
                rmcptr->val.x.incr = 1;
                rmcptr->val.x.fmt = Xstrdup("%d");
/* ...but can expand on this using @:start=n,incr=m,fmt=%aad.
 * NOTE that the format spec MUST be for a d (or u) item!!!
 * We've already got the length of btext for advancing, so the fact that
 * strtok will write NULs into it doesn't worry us.
 */
                if (dbp_charat(btext, 1) == ':') {
                    char *tokp = strdupa(dbp_val(btext)+2);
                    char *ntp;
                    while ((ntp = strtok(tokp, ","))) {
                        tokp = NULL;
                        if (0 == strncmp("start=", ntp, 6)) {
                            rmcptr->val.x.curval = atoi(ntp+6);
                        }
                        else if (0 == strncmp("incr=", ntp, 5)) {
                            rmcptr->val.x.incr = atoi(ntp+5);
                        }
                        else if (0 == strncmp("fmt=", ntp, 4)) {
                            rmcptr->val.x.fmt = Xstrdup(ntp+4);
                        }
                    }
                }
                break;
            case '&':   /* Evaluate a function. May contain ${n} and ${@}. */
                rmcptr->mc.type = REPL_FNC;

/* Need to parse the btext looking for groups and counters.
 * We do not need to handle variables, as evaluating the function will
 * do that.
 * But we do need to take a copy of btext, as it comes from brace_text()
 * and we will call it again.
 */
                rmcptr->val.fc = new_fc();
                struct func_call *wkfcp = rmcptr->val.fc;
                char *bp = strdupa(dbp_val(btext));
                char *tr_start = bp;
                char *ep = bp + dbp_len(btext);
                while (*bp) {
                    char *nxt = strstr(bp, "${");
                    if (!nxt) break;
                    if (nxt - bp) { /* Save previous text, if any */
                        wkfcp->type = LITCHAR;      /* Change type */
                        wkfcp->val.ltext = strndup(bp, (nxt - bp));
                        wkfcp->next = new_fc();
                        wkfcp = wkfcp->next;
                    }
                    dbp_dcl(cnt);
                    cnt = brace_text(nxt+2);
                    if (dbp_charat(cnt, 0) == '@') {    /* A counter */
/* Defaults... */
                        wkfcp->type = REPL_CNT;
                        wkfcp->val.x.curval = 1;
                        wkfcp->val.x.incr = 1;
                        wkfcp->val.x.fmt = Xstrdup("%d");
/* ...but can expand on this using @:start=n,incr=m,fmt=%aad.
 * NOTE that the format spec MUST be for a d (or u) item!!!
 * We've already got the length of btext for advancing, so the fact that
 * strtok will write NULs into it doesn't worry us.
 */
                        if (dbp_charat(cnt, 1) == ':') {
                            char *tokp = strdupa(dbp_val(cnt)+2);
                            char *ntp;
                            while ((ntp = strtok(tokp, ","))) {
                                tokp = NULL;
                                if (0 == strncmp("start=", ntp, 6)) {
                                    wkfcp->val.x.curval = atoi(ntp+6);
                                }
                                else if (0 == strncmp("incr=", ntp, 5)) {
                                    wkfcp->val.x.incr = atoi(ntp+5);
                                }
                                else if (0 == strncmp("fmt=", ntp, 4)) {
                                    wkfcp->val.x.fmt = Xstrdup(ntp+4);
                                }
                            }
                        }
                    }
                    else {                  /* Group */
                        wkfcp->type = REPL_GRP;
                        wkfcp->val.group_num = atoi(dbp_val(cnt));
                    }
                    wkfcp->next = new_fc();
                    wkfcp = wkfcp->next;
                    bp = nxt + dbp_len(cnt) + 3;
                    tr_start = bp;
                }
/* Copy any trailing text */
                if (ep - tr_start) {        /* Save trailing text, if any */
                    wkfcp->type = LITCHAR;  /* Change type */
                    wkfcp->val.ltext = strndup(tr_start, (ep - tr_start));
                    wkfcp->next = new_fc();
                    wkfcp = wkfcp->next;
                 }
                 break;
            default:    /* Assume a group number... */
                rmcptr->mc.type = REPL_GRP;
                rmcptr->mc.group_num = atoi(dbp_val(btext));
                break;
            }
            rmagical = TRUE;
            patptr += patptr_advance;
            break;
        case MC_ESC:            /* Just insert the next grapheme! */
            if (!*(++patptr)) { /* Can't be last char */
                parse_error(patptr, "dangling \\ at end");
                return FALSE;
	    }
            rmagical = TRUE;    /* Can't do literal now... */
	    /* Fall through - to handle next char... */
        default:                /* Need to test for ASCII again after MC_ESC */
            bc = build_next_grapheme(patptr, 0, -1, &gc, 0);
            if (gc.uc > 0x7f || gc.cdm) {   /* not-ASCII */
                patptr += bc;
                if (!gc.cdm) {  /* No combining marks so just UCLITL */
                    rmcptr->mc.type = UCLITL;
                    rmcptr->val.uchar = gc.uc;
                }
                else {          /* Has combining marks, so UCGRAPH */
                    rmcptr->mc.type = UCGRAPH;
/* We copy all of the data into our saved one. This means that any
 * malloc'ed ex parts get their pointers copied, and there is no more to
 * do here. Any freeing will be done in rmcclear()/
 */
                    rmcptr->val.gc = gc;
                }
                goto pchr_done_noincr;
            }
/* Must just be ASCII then */
            rmcptr->mc.type = LITCHAR;
            rmcptr->val.lchar = *patptr;
        }
        patptr++;
        rmcptr++;
        rmj++;      /* Is now the 0-based index of rmcptr value */
/* Do we need to increase magic_repl size? */
        if (rmj >= magic_repl_size) {
            int ptr_offs = rmcptr - rmcpat; /* Allow for rmcpat moving */
            increase_magic_repl_info();
            rmcptr = rmcpat + ptr_offs;
        }

pchr_done_noincr:;
    }
    rmcptr->mc = null_mg;
    return TRUE;
}

/* unicode_eq -- Compare two unicode characters.
 *  If we are not in EXACT mode, fold out the case.
 */
int unicode_eq(unsigned int bc, unsigned int pc) {
    if (bc == pc) return TRUE;
    if ((curwp->w_bufp->b_mode & MDEXACT) != 0) return FALSE;
    return (utf8proc_toupper(bc) == utf8proc_toupper(pc));
}

/* asceq -- Compare two bytes.  The "bc" comes from the buffer, "pc"
 *      from the pattern.  If we are not in EXACT mode, fold out the case.
 * Used by fast_scanner, which will NOT be used for non-EXACT mode if
 * there are bytes >0x7f, so the simple case-change code is OK.
 */
#define isASClower(c)  (('a' <= c) && (LASTLL >= c))

int asceq(unsigned char bc, unsigned char pc) {
    if ((curwp->w_bufp->b_mode & MDEXACT) == 0) {
        if (islower(bc)) bc ^= DIFCASE;
        if (islower(pc)) pc ^= DIFCASE;
    }
    return bc == pc;
}

/* mgpheq -- meta-character equality with a grapheme.
 *  Used by step_scanner.
 * NOTE that this free()s any ex part of the incoming gc!
 * (because all callers are done with it after this call, so it
 * puts the free() in one location).
 */
static int mgpheq(struct grapheme *gc, struct magic *mt) {
    int res;
    switch(mt->mc.type) {
    case LITCHAR:
        if (gc->cdm) res = FALSE;               /* Can't have combining bits */
        else if (gc->uc > 0x7f) res = FALSE;    /* Can't match non-ASCII */
        else res = asceq(gc->uc, mt->val.lchar);
        break;

    case ANY:                   /* Any EXCEPT newline or UEM_NOCHAR */
        res = (gc->uc != '\n' && gc->uc != UEM_NOCHAR);
        break;

/* Match ANY grapheme, incl NL, but not UEM_NOCHAR (*IMPORTANT*)! */
    case ANYGPH:
        res = (gc->uc != UEM_NOCHAR);
        break;

/* The CCL logic allows for inverted logic in the UCPROP test as well
 * as the inverted logic in the overall CCL test.
 *
 * So [t1t2t3]  is t1 OR t2 OR t3
 *    [^t1t2t3] is NOT (t1 OR t2 OR t3) == (NOT t1) AND (NOT t2) AND (NOT t3)
 * hence:
 *  test - result for:  a   1   !
 *   [\w\s]             T   T   F
 *   [^\w\s]            F   F   T
 *   [\W\S]             T   T   T       Not all that useful....
 *   [^\W\S]            F   F   F       ...nor is this.
 *
 * So for any test we can stop as soon as one part is TRUE.
 */
    case CCL:                   /* Now have to allow for extended CCL too */
        if (gc->uc & 0x80) {
            res = FALSE;        /* So that we drop into xt_cclmap tests */
        }
        else if (!(res = biteq(gc->uc, mt->val.cclmap))) {
/* Must be ASCII to ever get here...perhaps it's just a case difference? */
            if ((curwp->w_bufp->b_mode & MDEXACT) == 0 &&
                 (isASCletter(gc->uc))) {
                res = biteq(CHCASE(gc->uc), mt->val.cclmap);
            }
        }
        if (!res && mt->x.xt_cclmap) {
            int done = 0;
            for (struct xccl *xp = mt->x.xt_cclmap; !done && !res; xp++) {
                switch(xp->xc.type) {
                case EGRP:
                    done = 1;
                    break;
/* This is for a range above MAXASCII.
 * NOTE that there is no case handling here. Non-ASCII case-mapping
 * doesn't fit well with ranges.
 */
                case CCL:    /* Cannot have any combining bit */
                    if (gc->cdm)
                        res = FALSE;
                    else
                        res = ((xp->xval.cl_lim.low <= gc->uc) &&
                               (xp->xval.cl_lim.high >= gc->uc));
                    break;
/* Whether UCLITL can contain any combining bit depends on whether
 * EQUIV mode is on. If it is then it is possible for a literal
 * unicode char to match a base char + combining char.
 * e.g.
 *    Å can be U+212B, U+00C5 or U+0041+U+030A
 *    ñ can be U+006E+U+0303 or U+00F1
 * So, if EQUIV mode is on (Magic must be on for us to be here)
 * we run same_grapheme() on the pair to match, otherwise
 * we run through a simpler test.
 */
                case UCLITL:    /* Can it have any combining bit? */
                    if (curwp->w_bufp->b_mode & MDEQUIV) {
                        struct grapheme testgc;
                        testgc.uc = xp->xval.uchar;
                        testgc.cdm = 0;
                        testgc.ex = NULL;
                        res = same_grapheme(gc, &testgc, USE_WPBMODE);
                        break;
                    }
                    if (gc->cdm) {
                        res = FALSE;
                        break;
                    }           /* Fall through */
                case UCKIND:    /* May have any combining bit */
                    if (curwp->w_bufp->b_mode & MDEXACT) {
                        res = (xp->xval.uchar == gc->uc);
                    }
                    else {      /* Upper so Greek sigmas work (2l->1U) */
                        res = (utf8proc_toupper(xp->xval.uchar) ==
                               utf8proc_toupper(gc->uc));
                    }
                    break;
/* May have any combining bit, but only the base character is tested */
                case UCPROP: {
                    const char *uccat = utf8proc_category_string(gc->uc);
                    int testlen = strlen(xp->xval.prop); /* 1 or 2 */
                    res = !strncmp(uccat, xp->xval.prop, testlen);
                    break;
                }
                default:        /* Nothing else can match */
                    break;
                }
                if (xp->xc.negate_test) res = !res;
            }
        }
        break;
/* Whether UCLITL can contain any combining bit depends on whether
 * EQUIV mode is on. If it is then it is possible for a literal
 * unicode char to match a base char + combining char.
 * e.g.
 *    Å can be U+212B, U+00C5 or U+0041+U+030A
 *    ñ can be U+006E+U+0303 or U+00F1
 * So, if EQUIV mode is on (Magic must be on for us to be here)
 * we run same_grapheme() on the pair to match, otherwise
 * we run through a simpler test.
 */
    case UCLITL:    /* Can it have any combining bit? */
        if (curwp->w_bufp->b_mode & MDEQUIV) {
            struct grapheme testgc;
            testgc.uc = mt->val.uchar;
            testgc.cdm = 0;
            testgc.ex = NULL;
            res =same_grapheme(gc, &testgc, USE_WPBMODE);
            break;
        }
        if (gc->cdm) {
            res = FALSE;
            break;
        }           /* Fall through */
    case UCKIND:    /* May have any combining bit */
        if (curwp->w_bufp->b_mode & MDEXACT) {
            res = (mt->val.uchar == gc->uc);
        }
        else {      /* Upper so Greek sigmas work (2l->1U) */
            res = (utf8proc_toupper(mt->val.uchar) ==
                   utf8proc_toupper(gc->uc));
        }
        break;

    case UCGRAPH: {
        res = same_grapheme(gc, &(mt->val.gc), USE_WPBMODE);
        break;
    }
    case UCPROP: {  /* Only tested against the base character...  */
        const char *uccat = utf8proc_category_string(gc->uc);
        int testlen = strlen(mt->val.prop); /* 1 or 2 */
        res = !strncmp(uccat, mt->val.prop, testlen);
        break;
    }
    default:    /* Should never get here... */
        mlforce("mgpheq: what is %d?", mt->mc.type);
        sleep(2);
        res = FALSE;
    }

/* May seem unusual to free it here, but once we've tested it we've
 * done with it.
 */
    if (gc->ex) {           /* Our responsibility! */
        Xfree_setnull(gc->ex);
    }
    if (mt->mc.negate_test) res = !res;
    return res;
}

/* rvstrcpy -- Reverse string copy.
 */
void rvstrcpy(db *rvstr, db *str) {

/* Get a copy of the original */

    char *wp = strdupa(dbp_val(str));

/* Now reverse this copy */

    char *bp = wp;
    char *ep = wp + dbp_len(str) - 1;
    do {
        char a = *bp;   /* Original begin */
        *bp++ = *ep;    /* Copy end to begin */
        *ep-- = a;      /* Original begin into end */
    } while (bp < ep);

/* Now set the result */
    dbp_set(rvstr, wp);
    return;
}

/*      Setting up search jump tables.
 *      the default for any character to jump
 *      is the pattern length
 */
void setpattern(db *apat, db *tap) {
    int i;

/* Add pattern to search run if called during command line processing */
    if (comline_processing) update_ring(dbp_val(apat));

    patlenadd = srch_patlen - 1;
    for (i = 0; i < HICHAR; i++) {
        deltaf[i] = srch_patlen;
        deltab[i] = srch_patlen;
    }

/* Now put in the characters contained in the pattern, duplicating the CASE
 */
    int nocase = !(curwp->w_bufp->b_mode & MDEXACT);
    for (i = 0; i < patlenadd; i++) {
        deltaf[ch_as_uc(dbp_charat(apat, i))] = patlenadd - i;
        if (nocase && isalpha(ch_as_uc(dbp_charat(apat, 1))))
            deltaf[ch_as_uc(dbp_charat(apat, i) ^ DIFCASE)] = patlenadd - i;
        deltab[ch_as_uc(dbp_charat(tap, i))] = patlenadd - i;
        if (nocase && isalpha(ch_as_uc(dbp_charat(tap, i))))
            deltab[ch_as_uc(dbp_charat(tap, i) ^ DIFCASE)] = patlenadd - i;
    }

/* The last character will have the pattern length unless there are
 * duplicates of it. Get the number to jump from the arrays delta, and
 * overwrite with zeros in delta duplicating the CASE.
 */
    lastchfjump = patlenadd + deltaf[ch_as_uc(dbp_charat(apat, patlenadd))];
    deltaf[ch_as_uc(dbp_charat(apat, patlenadd))] = 0;
    if (nocase && isalpha(ch_as_uc(dbp_charat(apat, patlenadd))))
        deltaf[ch_as_uc(dbp_charat(apat, patlenadd) ^ DIFCASE)] = 0;
    lastchbjump = patlenadd + deltab[ch_as_uc(dbp_charat(apat, 0))];
    deltab[ch_as_uc(dbp_charat(apat, 0))] = 0;
    if (nocase && isalpha(ch_as_uc(dbp_charat(apat, 0))))
        deltab[ch_as_uc(dbp_charat(apat, 0) ^ DIFCASE)] = 0;
}

/* readpattern -- Read a pattern.  Stash it in apat.
 * If it is the search string, create the reverse pattern and the
 * magic pattern, assuming we are in MAGIC mode (and defined that way).
 * apat is not updated if the user types in an empty line.
 * If the user typed an empty line, and there is no old pattern, it is
 * an error.
 * Display the old pattern, in the style of Jeff Lomicka.
 * There is some do-it-yourself control expansion.
 */
static int readpattern(char *prompt, db *apat, int srch) {
    int status;
    db_strdef(tpat);

    char saved_base[16];        /* Same size as current_base */

/* We save the base of the prompt for previn_ring to use.
 * Since this code can be re-entered we have to save (and restore at
 * the end) the current value.
 */
    strcpy(saved_base, current_base);
    strcpy(current_base, prompt);
    dbp_dcl(ep) = expandp(dbp_val_nc(apat));
    db_sprintf(tpat, "%s " MLpre "%s" MLpost ": ", prompt, dbp_val(ep));

/* Read a pattern.  Either we get one or we just get an empty result
 * and use the previous pattern.
 * Then, if it's the search string, make a reversed pattern.
 * *Then*, make the meta-pattern, if we are defined that way.
 * We set this_rt before and after the mlreply() call.
 */
    enum call_type our_rt = (srch == TRUE)? Search: Replace;
    this_rt = our_rt;           /* Set our call type for nextin_ring() */
    status = mlreply(db_val(tpat), &tpat, CMPLT_SRCH);
    this_rt = our_rt;           /* Set our call type for update_ring() */
    int do_update_ring = 1;

/* status values from mlreply (-> getstring()) are
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
            if (db_len(pat) > 0) {      /* Have a default to use? */
                db_set(tpat, db_val(pat));
                do_update_ring = 0;     /* Don't save this */
                status = TRUE;          /* So we do the next section... */
            }
        }
        else {                          /* Must be a Replace */
            db_set(tpat, db_val(rpat));
            if (*repl_txt[0] == '\0')   /* If current top is also empty... */
                do_update_ring = 0;     /* ...don't save ths one */
            status = TRUE;              /* So we do the next section... */
        }
/* Record this default if we are recording a keyboard macro, but only
 * at base level of the minibuffer (i.e. we aren't searching *in* the
 * mini-buffer).
 */
        if (kbdmode == RECORD && mb_info.mbdepth == 0)
             addto_kbdmacro(db_val(tpat), 0, 1);
    }
    if (status == TRUE) {
        dbp_set(apat, db_val(tpat));
/* Save this latest string in the search buffer ring? */
        if (do_update_ring) update_ring(db_val(tpat));

/* Note that we always rebuild any meta-pattern from scratch even if
 * we used the default pattern (which is reasonable, since it might not
 * be the last one, now we have a search ring!).
 * If we are NOT in Magic mode then if Exact mode is on we will never
 * use the meta-pattern, so there is no point in setting it up.
 * If that is not the case then we call mcstr/rmcstr, getting *them* to
 * determine whether there is any "magic" and setting slow_scan accordingly.
 * NOTE that for the NOT Magic/NOT Exact case then the presence of any
 * non-ASCII byte in the search string means we use the meta-pattern code
 * (step_scan) to allow for handling Unicode case-mapping.
 * We only ever build the replacement magic when in MAGIC mode.
 * The mcstr() and rmcstr() procedure structure data is only freed
 * the next time each is called.
 */

        if ((curwp->w_bufp->b_mode & MDMAGIC) ||
            (!(curwp->w_bufp->b_mode & MDEXACT) && srch)) {
            status = srch ? mcstr() : rmcstr();
        }
        else
            if (srch) slow_scan = 0;

/* If we are doing the search string remember the length for substitution
 * purposes and reverse string copy. For fast scans, set jump tables.
 */
        if (srch) {
            srch_patlen = dbp_len(apat);
            rvstrcpy(&tap, apat);
            if (!slow_scan) setpattern(apat, &tap);
        }

    }
    strcpy(current_base, saved_base);   /* Revert any change */

    db_free(tpat);
    return status;
}

/* nextgph -- retrieve the next/previous grapheme in the buffer,
 *      and advance/retreat the point.
 *  The order in which this is done is significant, and depends
 *  upon the direction of the search.  Forward searches gets the
 *  grapheme from the current point and move beyond it, while
 *  reverse searches get the previous grapheme and move to its start.
 * NOTE!!! that we return a pointer to a static, internal struct grapheme.
 * It is the CALLERs responsibility to deal with any alloc's on gc.ex!!
 *  Used by step_scanner.
 */

/* For non-overlapping, reverse slow searches we need to set-up an
 * artifical barrier beyond which we don't retrieve characters.
 */
static struct line *barrier_endline;
static int barrier_offset;
static int barrier_active = 0;

#define POS_ONLY 0x10000000
static struct grapheme gc;
static struct grapheme *nextgph(struct line **pcurline, int *pcuroff,
     int *bytes_used, int dir) {

    int offs_4_nl;
    int nextoff, curoff;
    struct line *nextline, *curline;
    gc = null_gc;

    int pos_only;
    if (dir & POS_ONLY) {
        pos_only = 1;
        dir = dir & ~POS_ONLY;
    }
    else
        pos_only = 0;

    curline = *pcurline;
    curoff = *pcuroff;

    offs_4_nl = (dir == FORWARD)? lused(curline): 0;
    if (curoff == offs_4_nl) {  /* Need to change lines */
        if (dir == FORWARD) {
            nextline = lforw(curline);
            nextoff = 0;
        }
        else {
            nextline = lback(curline);
            nextoff = lused(nextline);
        }
        if (bytes_used) *bytes_used = 1;
/* Fudge in a newline char. HOWEVER, if we are at the end of buffer (in
 * either direction) we set an invalid character. This will fail
 * comparisons, *including* the ANYGPH check in mgpheq()
 * Note that going FORWARD we are at EOB when "on" the marker, while when
 * in reverse it is if we are about to go onto it.
 */
        if (!pos_only) {
            if (((dir == FORWARD) && (curline == curbp->b_linep)) ||
                ((dir == REVERSE) && (nextline == curbp->b_linep)))
                gc.uc = UEM_NOCHAR;
            else
                gc.uc = '\n';
        }
    }
    else {
        if (pos_only) { /* Just get the position in the current line */
            if (dir == FORWARD)
                nextoff = next_utf8_offset(ltext(curline), curoff,
                     lused(curline), TRUE);
            else
                nextoff = prev_utf8_offset(ltext(curline), curoff, TRUE);

        }
        else {          /* Get the grapheme from the current line */

/* For back searching in slow-scan mode we may have an artificial barrier
 * Note that the direction will actually be FORWARD for this!!!
 */
            if (barrier_active &&
                   (curline == barrier_endline) &&
                   (curoff >= barrier_offset)) {
                gc.uc = UEM_NOCHAR;
                return &gc;
            }
            typedef int (*fn_gcall)(const char *, int, int,
                 struct grapheme *, int);
            fn_gcall get_graph;
            if (dir == FORWARD) get_graph = build_next_grapheme;
            else                get_graph = build_prev_grapheme;
            nextoff =
                 get_graph(ltext(curline), curoff, lused(curline), &gc, 0);
        }
        nextline = curline;
        if (bytes_used) *bytes_used = abs(nextoff - curoff);
    }
    *pcurline = nextline;
    *pcuroff = nextoff;
    return &gc;
}

/* check_next() - helper routine to fetch next char or grapheme.
 * Returns number of bytes used, or -1 on no match.
 *  Used by step_scanner
 */
static int check_next(struct line **cline, int *coff, struct magic *mcptr) {

/* Avoid a possible "may be used initialized" for used.
 * Since we aren't using -Winit-self we can initialize it to itself
 * and the optimizer will optimize it away (!?!).
 * From Flexo's answer at:
 *   https://stackoverflow.com/questions/5080848/disable-gcc-may-be-used-uninitialized-on-a-particular-variable
 */
    int used = used;
    struct grapheme *gc = nextgph(cline, coff, &used, FORWARD);
/* Check that there *is* a next character! */
    if (gc->uc == UEM_NOCHAR) {
        used = -1;      /* Failed (to get a next character) */
    }
    else if (!mgpheq(gc, mcptr)) {
        used = -1;      /* Failed (to match the next character) */
    }
/* mgpheq() will have freed any malloc()ed gc->ex parts.
 * And if we didn't get round to calling mgpheq(), then there won;t be
 * any gc->ex parts to free.
 */
    return used;
}

/* amatch -- Search for the *entirety* of a  meta-pattern,
 *           now *always* in the forwards direction.
 *
 *      Based on the recursive routine amatch() (for "anchored match") in
 *      Kernighan & Plauger's "Software Tools". (or was - may not be now).
 *
 * The return value from this is now the number of bytes matched
 * with a -ve number meaning FAILed.
 *  Used by step_scanner
 *
 * struct magic *mcptr;         string to scan for
 * struct line **pcwline;       current line during scan
 * int *pcwoff;                 position within current line
 *
 * The pre_match value here tells us whether this amatch call is anchored
 * to a buffer location or whether the first match of a repeat may
 * be shortened.
 * Use the number of already-matched chars when calling amatch()
 * Returns the total match length (in bytes!) from here to the end,
 * or AMFAIL on failure to match.
 *  Used by step_scanner
 */
#define AMFAIL -1
static int am_level = 0;
static int amatch(struct magic *mcptr, struct line **pcwline, int *pcwoff,
     int pre_match) {
    struct line *curline;           /* current line during scan */
    int curoff;                     /* position within current line */
    int skip_choices;

/* Set up local scan pointers to ".", and get the current character.
 * Then loop around the pattern pointer until success or failure.
 */
    curline = *pcwline;
    curoff = *pcwoff;
    int ambytes = 0;                /* Bytes *we've* matched */
    am_level++;                     /* Which round of amatch() we are */

try_next_choice:
    skip_choices = FALSE;           /* Can't be in a CHOICE here */
    while (1) {
        int bytes_used = 0;         /* Bytes on this pass, not yet in ambytes */

/* Handle start/end groups
 */
        int cgn = mcptr->mc.group_num;
        switch(mcptr->mc.type) {
        case CHOICE:
/* If we hit a CHOICE then, if we are still running a CHOICE (i.e the
 * previous CHOICE) we have to skip to the end of CHOICES and process that.
 * Otherwise we drop though to the group start (as CHOICEs always start groups)
 * and let things run.
 */
            if (skip_choices) {     /* Already in in a CHOICE */
                mcptr = mcpat + cntl_grp_info[mcptr->mc.group_num].gpend;
                continue;
            }
            skip_choices = TRUE;
            /* Falls through */
        case SGRP:
            match_grp_info[cgn].mline = curline;
            match_grp_info[cgn].start = curoff;
            match_grp_info[cgn].len = 0;
/* Need to remember where the start of the match is */
            match_grp_info[cgn].base = pre_match + ambytes;
/* This should not happen. Enable if it seems to be doing so. */
#if 0
            if (cntl_grp_info[cgn].state != GPIDLE) {
                mlwrite("Opening non-idle group %d", cgn);
                return AMFAIL;
            }
#endif
            cntl_grp_info[cgn].state = GPOPEN;
            cntl_grp_info[cgn].start_level = am_level;
            cntl_grp_info[cgn].next_choice_idx = mcptr->x.next_or_idx;
            if (mcptr->x.next_or_idx) {
                skip_choices = TRUE;
            }
            goto next_entry;
        case EGRP:
            match_grp_info[cgn].len =
                 (pre_match + ambytes) - match_grp_info[cgn].base;
            cntl_grp_info[cgn].state = GPPENDING;
            cntl_grp_info[cgn].pending_level = am_level;
            if (cgn == 0) goto success;
            goto next_entry;
        }
        int used;

/* Iff we have a repeat count we handle it here.
 * This can have limits, and also be a minimal or maximal repeat.
 */
        TEST_BLOCK(mcptr->mc.repeat)

/* Try to get as many matches as possible against the current meta-character.
 * We now allow a lower limit to this match...
 * We might also now want the shortest match...
 */
        int hi_lim = mcptr->cl_lim.high;
        if (hi_lim == 0) {  /* Zero-length match - currently valid, so OK */
            goto next_entry;
        }

        int lo_lim = mcptr->cl_lim.low;
        int nmatch = 0;     /* Match count...not chars */

/* First check that we have the minimum matches at the start.
 * If not, then we have failed early.
 */
        while (nmatch < lo_lim) {
            int nb = check_next(&curline, &curoff, mcptr);
            if (nb < 0) break;
            bytes_used += nb;   /* Add to our running count */
            nmatch++;
        }
        if (nmatch < lo_lim) goto failed;   /* Can't make lower limit */

/* If the pattern is a minimal repeat that is unanchored (no preceding match)
 * then anything more than a minimum must fail, so that we get the shortest
 * match at the start. (As we'll get a shorter match after we step over this
 * grapheme and try again).
 * This may not be the first pattern test, as it is possible for patterns
 * to match nothing (e.g. a*)
 */
        if (!pre_match && !ambytes) { /* No prev match */
            if (mcptr->mc.min_repeat) hi_lim = lo_lim;
        }

/* Now check for further matches against the current pattern.
 * For each success we run amatch on the rest of the string and pattern.
 *
 * For minimal match we keep going while check_next() succeeds until
 * amatch() also succeeds.  We then have the minimal match.
 *
 * For a maximal match we run check_next() until it *fails* then step
 * *backwards* until amatch() succeeds. We then have the maximal match.
 *
 * NOTE: that if amatch() returns a success then we have completed a
 * succesful search. We're succesfull to get here, and amatch() has
 * said were successful from here on. So we will have completed the
 * entire match and just have to unwind.
 */
        if (mcptr->mc.min_repeat) {
            while (nmatch <= hi_lim) {
                int sub_ambytes = amatch(mcptr+1, &curline, &curoff,
                     pre_match+ambytes+bytes_used);
                if (sub_ambytes >= 0) {     /* Successful sub-match */
                    bytes_used += sub_ambytes;
                    ambytes += bytes_used;
                    goto success;   /* We've now matched the rest */
                }
                int nb = check_next(&curline, &curoff, mcptr);
                if (nb < 0) break;
                bytes_used += nb;
                nmatch++;
            }
            goto failed;
        }

/* For a MAXIMAL match we eat as many of the current char as possible
 * first, then backtrack until we find a match. Or fail if we don't.
 */
        else {
            while (nmatch < hi_lim) {
                struct line *pline = curline;
                int poff = curoff;
                int nb = check_next(&curline, &curoff, mcptr);
                if (nb < 0) {   /* Undo move... */
                    curline = pline;
                    curoff = poff;
                    break;
                }
                bytes_used += nb;   /* Count the success */
                nmatch++;
            }

/* We are now at the character that made us fail (no match or beyond hi_lim).
 * Try to match the rest of the pattern, shrinking the repeat by one
 * before each successive test.
 * Since repeat matches *zero* or more occurrences of a pattern, a match
 * may start even if the previous loop matched no characters.
 * (NOTE: We actually now allow a non-zero lower limit to this match...)
 * There is no need to test individual chars/graphemes on the way back
 * as we already tested them to get here in the first place. We're just
 * adjusting curline and curoff.
 * Once amatch() succeeds we can goto success as we know the rest of
 * the pattern has matched (that's what amatch() does).
 */
            while(1) {
                int sub_ambytes = amatch(mcptr+1, &curline, &curoff,
                     pre_match+ambytes+bytes_used);
                if (sub_ambytes >= 0) {
                    bytes_used += sub_ambytes;
                    ambytes += bytes_used;
                    goto success;
                }
                (void)nextgph(&curline, &curoff, &used, POS_ONLY|REVERSE);
                bytes_used -= used;
                if (--nmatch < lo_lim) goto failed;
            }
        }
        goto next_entry;    /* To next loop entry...(can never get here?) */
        END_TEST(mcptr->mc.repeat)

/* The only way we'd get a BOL metacharacter here is at the end of the
 * reversed pattern.
 * The only way we'd get an EOL metacharacter here is at the end of a
 * regular pattern.
 * So if we match one or the other, and are at the appropriate position we
 * are guaranteed success (since the next pattern character has to be EGRP).
 * So BEFORE we read the next entry we'll check for these two, that way we
 * don't need to back-up if the test succeeds.
 * For example, a search for ")$" will leave the cursor at the end of the
 * line, while a search for ")<NL>" will leave the cursor at the
 * beginning of the next line.
 * This follows the notion that the meta-character '$' (and likewise
 * '^') match positions, not characters.
 */
        switch(mcptr->mc.type) {
        case BOL:
        case EOL:
            if ((size_t)curoff ==
                      ((mcptr->mc.type == BOL)? 0: lused(curline))) {
                goto next_entry;
            }
            else {
                goto failed;
            }
        }

/* The "Not a repeat" cases */

        int nb = check_next(&curline, &curoff, mcptr);
        if (nb < 0) goto failed;
        bytes_used += nb;

/* Increment the length counter and advance the pattern pointer. */
next_entry:
        ambytes += bytes_used;
        mcptr++;
    }                       /* End of mcptr loop. */

/* A SUCCESSFUL MATCH!!!  Set the "." pointers to the end of the match
 *  and set groups marked as GPPENDING at this level to be GPVALID.
 */

success:
    *pcwline = curline;
    *pcwoff = curoff;
    for (int gi = 0; gi <= group_cntr; gi++) {
        if ((cntl_grp_info[gi].state == GPPENDING) &&
            (cntl_grp_info[gi].pending_level == am_level)) {
            cntl_grp_info[gi].state = GPVALID;
        }
        else if ((cntl_grp_info[gi].state == GPOPEN) &&
            (cntl_grp_info[gi].start_level == am_level)) {
            cntl_grp_info[gi].state = GPIDLE;
        }
    }
    am_level--;
    return ambytes;

failed:
/* If we have a further choice, go back to near the start of this
 * function, but with a new mcptr and the saved curline+curoff and ambytes
 * from the group start.
 * If we do have another choice then we must invalidate all
 * groups at that group-level or higher before trying that next choice.
 * They must now be invalid, but lower ones are still currently OK.
 * NOTE!!!
 * If we do not have another choice then we must not invalidate any groups!
 * If we did we'd wipe out all earlier groups when a repeat pattern fails.
 */
    ;   /* Dummy statement to allow declaration to follow */
    int nc = -1;
    for (int gi = group_cntr; gi >= 0; gi--) {
        if (cntl_grp_info[gi].next_choice_idx) {
            nc = gi;
            break;
        }
    }
    if (nc >= 0) {
        for (int gi = group_cntr; gi >= nc; gi--) {
/* Just set the value to GPIDLE regardless of its current state. */
            cntl_grp_info[gi].state = GPIDLE;
        }
        mcptr = mcpat + cntl_grp_info[nc].next_choice_idx;
        curline = match_grp_info[nc].mline;
        curoff = match_grp_info[nc].start;
        ambytes = match_grp_info[nc].base;
        goto try_next_choice;
    }

    am_level--;
    return AMFAIL;
}

/* Called by step_/fast_scanner at the start of any search to
 * initialize group info (and clear out any old allocated bits).
 * Also at the end of replaces() to forget any matches there.
 */
static void init_dyn_group_status(void) {

/* If we allocated any result strings, free them now... */
    for (int gi = 0; gi < max_grp; gi++) {
        if (grp_text[gi]) {
            Xfree_setnull(grp_text[gi]);
        }
    }
/* ...and also initialize dynamic group matching info */
    for (int gi = 0; gi <= group_cntr; gi++) {
        match_grp_info[gi] = null_match_grp_info;
    }
    group_match_buffer = NULL;
    return;
}

/* step_scanner -- Search for a meta-pattern in either direction.
 *  If found, reset the "." to be at the start or just after the match
 *  string, and (perhaps) repaint the display.
 * NOTE: that for a REVERSE search we step towards the start of the file
 * but always check against the pattern-match in a "forwards" direction.
 *
 * struct magic *mcpatrn;       pointer into pattern
 * int direct;                  which way to go.
 * int beg_or_end;              put point at beginning or end of pattern.
 */
static int step_scanner(struct magic *mcpatrn, int direct, int beg_or_end) {
    struct line *curline;   /* current line during scan */
    int curoff;             /* position within current line */

/* Remove any old group info */

    init_dyn_group_status();    /* Forget the past */

/* We never actually test in reverse for step_scanner(), so there is no
 * need to toggle beg_or_end here.
 */

/* Setup local scan pointers to global ".". */

    curline = curwp->w.dotp;
    curoff = curwp->w.doto;

/* If we are searching backwards we need to go back one column
 * before looking, otherwise we might match without moving!
 * But scanmore wants to be able stay where we are for an extended match...
 */
    if (direct == REVERSE)
        (void)nextgph(&curline, &curoff, NULL, POS_ONLY|REVERSE);
    else if (direct == REVERSE_NOSKIP)
        direct = REVERSE;

    magic_search.start_line = curline;
    magic_search.start_point = curoff;
/* Set the initial values for group info markers at the start of each search */
    for (int gi = 0; gi <= group_cntr; gi++) {
        match_grp_info[gi] = null_match_grp_info;
        cntl_grp_info[gi].state = GPIDLE;
        cntl_grp_info[gi].next_choice_idx = 0;
    }

/* Scan each character until we hit the head link record. */
    while (!boundry(curline, curoff, direct)) {

/* Save the current position in case we need to restore it on a match,
 */
        struct line *matchline = curline;
        int matchoff = curoff;
        int amres = amatch(mcpatrn, &curline, &curoff, 0);

        if (amres >= 0) {       /* A SUCCESSFUL MATCH!!! */

/* Ensure that any groups that are not GPVALID have no mline set */

            for (int gi = 0; gi <= group_cntr; gi++) {
                if (cntl_grp_info[gi].state != GPVALID) {
                    match_grp_info[gi].mline = NULL;
                }
            }

/* Reset the global "." pointers. */
            if (beg_or_end == PTEND) {        /* Go to end of string */
                curwp->w.dotp = curline;
                curwp->w.doto = curoff;
            }
            else {                          /* Go to beginning of string */
                curwp->w.dotp = matchline;
                curwp->w.doto = matchoff;
            }
            curwp->w_flag |= WFMOVE;        /* flag that we've moved */
            magic_search.start_line = NULL;
            group_match_buffer = curwp->w_bufp;

            return TRUE;
        }

/* NEEDS TO BE next_grapheme for step_scanner
 * Just need to get the position, though, not the actual grapheme.
 */
        (void)nextgph(&curline, &curoff, NULL, POS_ONLY | direct);
    }
    magic_search.start_line = NULL;
    return FALSE;                           /* We could not find a match. */
}

/* fbound -- Return information depending on whether we may search no
 *      further.  Beginning of file and end of file are the obvious
 *      cases, but we may want to add further optional boundary restrictions
 *      in future, a' la VMS EDT.  At the moment, just return TRUE or
 *      FALSE depending on if a boundary is hit (ouch),
 *      when we have found a matching trailing character.
 *  Used by fast_scanner
 */
static int fbound(int jump, struct line **pcurline, int *pcuroff, int dir) {
    int spare, curoff;
    struct line *curline;

    curline = *pcurline;
    curoff = *pcuroff;

    if (dir == FORWARD) {
        while (jump != 0) {
            curoff += jump;
            spare = curoff - lused(curline);
            while (spare > 0) {
                curline = lforw(curline);   /* skip to next line */
                if (curline == curbp->b_linep)
                    return TRUE;            /* hit end of buffer */
                curoff = spare - 1;
                spare = curoff - lused(curline);
            }
            if (spare == 0) {
                jump = deltaf[ch_as_uc('\n')];
            }
            else {
                jump = deltaf[ch_as_uc(lgetc(curline, curoff))];
            }
        }
/* the last character matches, so back up to start of possible match */

        curoff -= patlenadd;
        while (curoff < 0) {
            curline = lback(curline);/* skip back a line */
            curoff += lused(curline) + 1;
        }
    }
    else {                  /* Reverse.*/
        jump++;             /* allow for offset in reverse */
        while (jump != 0) {
            curoff -= jump;
            while (curoff < 0) {
                curline = lback(curline);       /* skip back a line */
                curoff += lused(curline) + 1;
                if (curline == curbp->b_linep)
                    return TRUE;    /* hit end of buffer */
            }
            if ((size_t)curoff == lused(curline)) {
                jump = deltab[ch_as_uc('\n')];
            }
            else {
                jump = deltab[ch_as_uc(lgetc(curline, curoff))];
            }
        }
/* the last character matches, so back up to start of possible match */

        curoff += srch_patlen;
        spare = curoff - lused(curline);
        while (spare > 0) {
            curline = lforw(curline);/* skip back a line */
            curoff = spare - 1;
            spare = curoff - lused(curline);
        }
    }

    *pcurline = curline;
    *pcuroff = curoff;
    return FALSE;
}

/* nextbyte -- retrieve the next/previous byte (character) in the buffer,
 *      and advance/retreat the point.
 *      The order in which this is done is significant, and depends
 *      upon the direction of the search.  Forward searches look at
 *      the current character and move, reverse searches move and
 *      look at the character.
 *  Used by fast_scanner() and delins()
 */
static char nextbyte(struct line **pcurline, int *pcuroff, int dir) {
    struct line *curline;
    int curoff;
    char c;

    curline = *pcurline;
    curoff = *pcuroff;

    if (dir == FORWARD) {
        if ((size_t)curoff == lused(curline)) { /* if at EOL */
            curline = lforw(curline);       /* skip to next line */
            curoff = 0;
            c = '\n';                       /* and return a <NL> */
        }
        else c = lgetc(curline, curoff++);  /* get the char  */
    }
    else {                                  /* Reverse. */
        if (curoff == 0) {
            curline = lback(curline);
            curoff = lused(curline);
            c = '\n';
        }
        else c = lgetc(curline, --curoff);
    }
    *pcurline = curline;
    *pcuroff = curoff;

    return c;
}

/*  fast_scanner -- Search for a pattern in either direction.
 *  If found, reset the "." to be at the start or just after the
 *  match string, and (perhaps) repaint the display.
 *  Fast version using simplified version of Boyer and Moore
 *  Software-Practice and Experience, vol 10, 501-506 (1980)
 *
 * Unlike the step_scanner, which always runs matches from "left to right",
 * the fast scanner runs them in either direction, and has a jump
 * table for each direction.
 * It stores match information in the entry for group 0.
 */
static int fast_scanner(const char *patrn, int direct, int beg_or_end) {
    char c;                         /* character at current position */
    const char *patptr;             /* pointer into pattern */
    struct line *curline;           /* current line during scan */
    int curoff;                     /* position within current line */
    struct line *scanline;          /* current line during scanning */
    int scanoff;                    /* position in scanned line */
    int jump;                       /* next offset */

/* Remove any old group info */

    init_dyn_group_status();    /* Forget the past */

/* If we are going in reverse, then the 'end' is actually the beginning
 * of the pattern.  Toggle it.
 */
    beg_or_end ^= direct;

/* Set up local pointers to global ".". */

    curline = curwp->w.dotp;
    curoff = curwp->w.doto;

/* Scan each character until we hit the head link record.
 * Get the character resolving newlines, offset by the pattern length,
 * i.e. the last character of the potential match.
 */
    jump = patlenadd;
    while (!fbound(jump, &curline, &curoff, direct)) {

/* Save the current position in case we match the search string at
 * this point.
 */
        struct line *matchline = curline;
        int matchoff = curoff;
/* Setup scanning pointers. */
        scanline = curline;
        scanoff = curoff;
        patptr = patrn;

/* Scan through the pattern for a match. */
        while (*patptr != '\0') {
            c = nextbyte(&scanline, &scanoff, direct);

#if 0
/* Debugging info, show the character and the remains of the string
 * it is being compared with.
 */
 fprintf(stderr, "fast1: moved to %c(%02x) at %d in %.*s\n", c, c, scanoff,
    (int)lused(scanline), ltext(scanline));
#endif
            if (!asceq(c, *patptr++)) {
                jump = (direct == FORWARD)? lastchfjump: lastchbjump;
                goto fail;
            }
        }

/* We appear to have matched, but if we are searching forwards then we need
 * to check that the next unicode char is not a combining diacritical as, if
 * it is, then we haven't actually matched what we were looking for since
 * that is really part of the current character.
 * If we are searching backwards we need to check the character at the
 * "other end" of the match.
 */
        struct line *tline = scanline;
        int toff = scanoff;
        if (direct == REVERSE) {
/* We know where we matched, and we know the byte length of the pattern
 * we matched, so advance that number of bytes to find the test char.
 */
            int togo = strlen(patrn);
            while(togo > 0) {
                int on_tline = lused(tline) - toff;
                if (on_tline > togo) on_tline = togo;
                togo -= on_tline;
                if (togo > 0) {     /* So on to next line */
                    togo--;
                    tline = lforw(tline);
                    toff = 0;
                }
                else
                    toff += on_tline;
            }
        }
        struct grapheme gct;
/* Don't build any ex... */
        (void)build_next_grapheme(ltext(tline), toff, lused(tline), &gct, 1);
        if (combining_type(gct.uc)) {
            jump = (direct == FORWARD)? lastchfjump: lastchbjump;
            goto fail;
        }

/* A SUCCESSFUL MATCH!!! Reset the global "." pointers */
        if (beg_or_end == PTEND) {      /* at end of string */
            curwp->w.dotp = scanline;
            curwp->w.doto = scanoff;
        }
        else {                          /* at beginning of string */
            curwp->w.dotp = matchline;
            curwp->w.doto = matchoff;
        }
/* Put the match info into group 0 */
        if (direct == FORWARD) {
            match_grp_info[0].mline = matchline;
            match_grp_info[0].start = matchoff;
        }
        else {
            match_grp_info[0].mline = scanline;
            match_grp_info[0].start = scanoff;
        }
        curwp->w_flag |= WFMOVE;        /* Flag that we have moved.*/
        match_grp_info[0].len = srch_patlen;
        group_match_buffer = curwp->w_bufp;
        return TRUE;
fail:;                                  /* continue to search */
    }
    return FALSE;                       /* We could not find a match */
}

/* Return a malloc()ed copy of the text for the given group in
 * the last match.
 * If a group doesn't exist or has no match then an empty string is
 * returned.
 * We only allocate each group text on demand for each search.
 * Any allocated space is freed by the reset for a new search.
 */
const char *group_match(int grp) {

/* Is the group-match data valid?? */

    if (!group_match_buffer) return "";

/* Is there a match to return? */

    if ((grp < 0) || (grp > group_cntr)) return "";
    if (!match_grp_info[grp].mline) return "";

/* Have we already sorted out this match for this search?
 * If not, set one up.
 */
    if (!grp_text[grp]) {

        grp_text[grp] = Xmalloc(match_grp_info[grp].len + 1);

/* Create the match text for this group... */

        char *dp = grp_text[grp];
        int togo = match_grp_info[grp].len;
        struct line *cline = match_grp_info[grp].mline;
        int coff = match_grp_info[grp].start;
        while(togo > 0) {
            int on_cline = lused(cline) - coff;
            if (on_cline > togo) on_cline = togo;
            memcpy(dp, ltext(cline)+coff, on_cline);
            dp += on_cline;
            togo -= on_cline;

/* Add in the newline, if needed, and switch to next line */

            if (togo > 0) {
                *dp++ = '\n';
                togo--;
                cline = lforw(cline);
                coff = 0;
            }
        }
        terminate_str(dp);
    }
    return grp_text[grp];
}

/* Whether we should run the back_grapheme code on the first
 * loop pass in forwhunt.
 * We only what to do this if we have a previous match of the same
 * search string, so do NOT want it when we arrive via forw/backsearch
 * or at startup.
 * It will always be set to 0 on a call to forwsearch() and to 1 after
 * any use by forwhunt().
 */
static int do_preskip = 0;

static void report_match(void) {
    const char *fullm = group_match(0);
    const char *rpt_text;
    if (strchr(fullm, '\n') == NULL) {
        rpt_text = fullm;
    }
    else {
        rpt_text = "<<multiline>>";
    }
    mlwrite("match: len: %d, %s", glyphcount_utf8(fullm), rpt_text);
    return;
}

/* forwhunt -- Search forward for a previously acquired search string.
 *      If found, reset the "." to be just after the match string,
 *      and (perhaps) repaint the display.
 *
 * int f, n;            default flag / numeric argument
 */
int backhunt(int, int);     /* Forward declaration */
int forwhunt(int f, int n) {
    int status = TRUE;

    if (n < 0) return backhunt(f, -n);  /* search backwards */

/* Make sure a pattern exists, or that we didn't switch into MAGIC mode
 * until after we entered the pattern.
 */
    if (db_len(pat) == 0) {
        mlwrite_one("No pattern set");
        return FALSE;
    }
    if (srch_can_hunt != 1) {
        mlwrite_one("Search invalid for repeat");
        return FALSE;
    }
    if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && mcpat[0].mc.type == EGRP) {
        if (!mcstr()) return FALSE;
    }

/* We are at the end of any previous match.
 * So on a re-execute we need to go back to the start of the match
 * and advance 1 character before searching again.
 * NOTE that this means we need to reset to the starting position after
 * a failed search by remembering where we are on each pass.
 * And forwscanner() can only do one scan at a time. (it clears groups).
 */
    struct line *olp;
    int obyte_offset;

    do {
        olp = curwp->w.dotp;
        obyte_offset = curwp->w.doto;
/* This must precede group clean-out */
        if (do_preskip) back_grapheme(glyphcount_utf8(group_match(0)) - 1);

/* We are going forwards so check for eob as otherwise the rest
 * of this code (magical and ordinary) loops us round to the start
 * and searches from there.
 * Simpler to fudge the fix here....
 */
        if (curwp->w.dotp == curbp->b_linep) {
            status = FALSE;
            break;
        }
        status = (slow_scan)? step_scanner(mcpat, FORWARD, PTEND)
                            : fast_scanner(db_val(pat), FORWARD, PTEND);
/* We now have a valid group_match, or have failed */
        if (ggr_opts&GGR_SRCHOLAP) do_preskip = 1;
    } while ((--n > 0) && status);

/* Complain and restore if not there - we already have the saved match... */

    if (status) {
        if ((curbp->b_mode & MD_MAGRPT) == MD_MAGRPT) report_match();
    }
    else {
        mlwrite_one("Not found");
        curwp->w.dotp = olp;
        curwp->w.doto = obyte_offset;
    }
    return status;
}

/* forwsearch -- Search forward.  Get a search string from the user, and
 *      then use forwhunt to do the work...
 *
 * int f, n;                    default flag / numeric argument
 */
int backsearch(int, int);   /* Forward declaration */
int forwsearch(int f, int n) {
    int status;

/* If n is negative, search backwards.
 * Otherwise proceed by asking for the search string.
 */
    if (n < 0) return backsearch(f, -n);

    if (inreex && RXARG(forwsearch)) {
        ;       /* Already have a pattern from last time */
    }
    else {
/* Ask the user for the text of a pattern.  If the response is TRUE
 * (responses other than FALSE are possible) we will have a pattern to use.
 */
        db_strdef(opat);
        int could_hunt = srch_can_hunt;
        db_set(opat, db_val_nc(pat));
        if ((status = readpattern("Search", &pat, TRUE)) == TRUE) {
            srch_can_hunt = 1;
/* A search with the same string should be the same as a reexec */
            if (!(ggr_opts&GGR_SRCHOLAP) || !could_hunt ||
                 db_cmp(opat, db_val(pat))) {
                do_preskip = 0;
            }
            else {
                do_preskip = 1;
            }
        }
        db_free(opat);
        if (status != TRUE) return status;
    }
    return forwhunt(f, n);
}

/* backhunt -- Reverse search for a previously acquired search string,
 *      starting at "." and proceeding toward the front of the buffer.
 *      If found "." is left pointing at the first character of the pattern
 *      (i.e. "looking at" the match)
 *
 * int f, n;            default flag / numeric argument
 */
int backhunt(int f, int n) {
    int status;

    if (n < 0) return forwhunt(f, -n);  /* Search forwards */

/* Make sure a pattern exists, or that we didn't switch into MAGIC mode
 * until after we entered the pattern.
 */
    if (db_len(tap) == 0) {
        mlwrite_one("No pattern set");
        return FALSE;
    }
    if (srch_can_hunt != -1) {
        mlwrite_one("Search invalid for repeat");
        return FALSE;
    }
    if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && mcpat[0].mc.type == EGRP) {
        if (!mcstr()) return FALSE;
    }

/* We are at the start of a match (we're going backwards).
 * Since matching is always done forwards in slow_scan mode we don't need
 * to adjust our position before searching again there, but we might need
 * to in fast mode.
 */
    struct line *olp;
    int obyte_offset;

    do {
        olp = curwp->w.dotp;
        obyte_offset = curwp->w.doto;
        if (!slow_scan) {           /* Must precede group clean-out */
            if (do_preskip) forw_grapheme(glyphcount_utf8(group_match(0)) - 1);
        }

/* Search for the pattern for as long as n is positive (n == 0 will go
 * through once, which is just fine).
 * For the slow scan in reverse mode we might need to set an artificial
 * barrier to prevent overlapping matches.
 */
        if (slow_scan) {
            if (!(ggr_opts&GGR_SRCHOLAP)) {
                barrier_endline = curwp->w.dotp;
                barrier_offset = curwp->w.doto;
                barrier_active = 1;
            }
            status = step_scanner(mcpat, REVERSE, PTBEG);
            barrier_active = 0;
        }
        else {
            status = fast_scanner(db_val(tap), REVERSE, PTBEG);
        }
/* We now have a valid group_match, or have failed */
        if (ggr_opts&GGR_SRCHOLAP) do_preskip = 1;
    } while ((--n > 0) && status);

/* Complain and restore if not there - we already have the saved match... */

    if (status) {
        if ((curbp->b_mode & MD_MAGRPT) == MD_MAGRPT) report_match();
    }
    else {
        mlwrite_one("Not found");
        curwp->w.dotp = olp;
        curwp->w.doto = obyte_offset;
    }
    return status;
}

/* backsearch -- Reverse search.  Get a search string from the user, and
 *      then use backhunt to do the work...
 *
 * int f, n;            default flag / numeric argument
 */
int backsearch(int f, int n) {
    int status = TRUE;

/* If n is negative, search forwards. Otherwise proceed by asking for the
 * search string.
 */
    if (n < 0) return forwsearch(f, -n);

    if (inreex && RXARG(backsearch)) {
        ;       /* Already have a pattern from last time */
    }
    else {
/* Ask the user for the text of a pattern.  If the response is TRUE
 * (responses other than FALSE are possible), we will have a pattern to use.
 */
        db_strdef(opat);
        int could_hunt = srch_can_hunt;
        db_set(opat, db_val_nc(pat));
        if ((status = readpattern("Search", &pat, TRUE)) == TRUE) {
            srch_can_hunt = -1;
/* A search with the same string should be the same as a reexec */
            if (!(ggr_opts&GGR_SRCHOLAP) || !could_hunt ||
                 db_cmp(opat, db_val(pat))) {
                do_preskip = 0;
            }
            else {
                do_preskip = 1;
            }
        }
        db_free(opat);
        if (status != TRUE) return status;
    }
    return backhunt(f, n);
}

/* Entry point for isearch.
 * This needs to set-up the patterns for the search to work.
 */
static struct line *sm_line = NULL;
static int sm_off = 0;
int scanmore(db *patrn, int dir, int next_match, int extend_match) {
    int sts;                /* search status */

/* If called with a NULL pattern, just remove group info. */
    if (!patrn) {
        init_dyn_group_status();
        return TRUE;
    }

/* In terms of matches we are only interested in the length.
 */
    int prev_match_len;
    if (!next_match || !extend_match)
        prev_match_len = glyphcount_utf8(group_match(0));
    else
        prev_match_len = 0;

/* If we aren't in Exact mode we need to run mcstr to determine
 * which scanner to use.
 * There is no MAGIC mode here, so force it off whilst we run.
 */
    int real_mode = curwp->w_bufp->b_mode;
    curwp->w_bufp->b_mode &= ~MDMAGIC;
    if (curwp->w_bufp->b_mode & MDEXACT) slow_scan = FALSE;
    else mcstr();   /* Let this decide */

/* If this then fails, we have to restore where we were... */
    if (next_match) {
        sm_line = curwp->w.dotp;    /* Save the current line pointer */
        sm_off = curwp->w.doto;     /* Save the current offset       */
/* If we want to find overlapping matches we need to set point to the
 * other end of the match to that which we highlight.
 * Since next_match is TRUE, we've already worked out the glyphcount
 * to move above.
 */
        if (ggr_opts&GGR_SRCHOLAP) {
            if (dir > 0) back_grapheme(prev_match_len - 1);
            else         forw_grapheme(prev_match_len - 1);
        }
    }

/* If we've been asked to extend the previous match we step
 * back(forward) to the *start* of the previous match before running.
 * This is so that t t e finds a match in ...ttte... rather than missing
 * it by having matched the first "tt" and only trying from the end of
 * that when the e comes along.
 */
    if (slow_scan) {
        if (dir < 0) {      /* reverse search? (no preskip needed AT ALL!) */
            sts = step_scanner(mcpat, REVERSE_NOSKIP, PTBEG);
        }
        else {              /* Nope. Go forward (with possible preskip) */
            if (extend_match) back_grapheme(prev_match_len);
            sts = step_scanner(mcpat, FORWARD, PTEND);
        }
    }
    else {
        rvstrcpy(&tap, patrn);  /* Put reversed string in tap */
        srch_patlen = dbp_len(patrn);
        setpattern(patrn, &tap);
        if (dir < 0) {      /* reverse search? (with possible preskip) */
/* The +1 is to allow the new character to be found since in fast mode
 * the scan *is* done in reverse from "here".
 */
            if (extend_match) forw_grapheme(prev_match_len + 1);
            sts = fast_scanner(db_val(tap), REVERSE, PTBEG);
        }
        else {              /* Nope. Go forward (with possible preskip) */
            if (extend_match) back_grapheme(prev_match_len);
            sts = fast_scanner(dbp_val(patrn), FORWARD, PTEND);
        }
    }
    curwp->w_bufp->b_mode = real_mode;

    if (!sts) {
        TTputc(BELL);   /* Feep if search fails       */
        TTflush();      /* see that the feep feeps    */
        if (next_match) {
            curwp->w.dotp = sm_line;
            curwp->w.doto = sm_off;
        }
    }
    return sts;             /* else, don't even try       */
}

/* Work out the replacement text for the current match.
 * The working buffer is never freed - only grows as needed.
 */
static db_strdef(repl);

static const char *getrepl(void) {

/* Process rmcpat .... */
    repl.len = 0;   /* Start afresh */
    struct magic_replacement *rmcptr = rmcpat;
    char ucb[6];
    int nb;
    while (rmcptr->mc.type != EGRP) {
        switch(rmcptr->mc.type) {
        case LITCHAR:
            db_addch(repl, rmcptr->val.lchar);
            break;
        case UCLITL:
            nb = unicode_to_utf8(rmcptr->val.uchar, ucb);
            db_appendn(repl, ucb, nb);
            break;
        case UCGRAPH:
            nb = unicode_to_utf8(rmcptr->val.gc.uc, ucb);
            db_appendn(repl, ucb, nb);
            if (!rmcptr->val.gc.cdm) break;
            nb = unicode_to_utf8(rmcptr->val.gc.cdm, ucb);
            db_appendn(repl, ucb, nb);
            unicode_t *uc_ex = rmcptr->val.gc.ex;
            while (uc_ex) {
                nb = unicode_to_utf8(*uc_ex++, ucb);
                db_appendn(repl, ucb, nb);
            }
            break;
        case REPL_VAR: {
            db_append(repl, getval(rmcptr->val.varname));
            break;
        }
        case REPL_GRP: {
            db_append(repl, group_match(rmcptr->mc.group_num));
            break;
        }
        case REPL_CNT: {
#define MAX_COUNTER_LEN 128
            char mc_text[MAX_COUNTER_LEN];
            int nlen = snprintf(mc_text, MAX_COUNTER_LEN, rmcptr->val.x.fmt,
                 rmcptr->val.x.curval);
            if (nlen >= MAX_COUNTER_LEN) nlen = MAX_COUNTER_LEN - 1;
            rmcptr->val.x.curval += rmcptr->val.x.incr;
            db_appendn(repl, mc_text, nlen);
            break;
        }
        case REPL_FNC: {
            db_strdef(fnc_buf);
            db_set(fnc_buf, "");   /* Start with nothing */
            for (struct func_call *fcp = rmcptr->val.fc; fcp->type != EOL;
                    fcp = fcp->next) {
                switch (fcp->type) {
                case LITCHAR:
                    db_append(fnc_buf, fcp->val.ltext);
                    break;
                case REPL_GRP:
                    db_append(fnc_buf, group_match(fcp->val.group_num));
                    break;
                case REPL_CNT: {
                    char mc_text[MAX_COUNTER_LEN];
                    (void)snprintf(mc_text, MAX_COUNTER_LEN, fcp->val.x.fmt,
                         fcp->val.x.curval);
                    fcp->val.x.curval += fcp->val.x.incr;
                    db_append(fnc_buf, mc_text);
                    break;
                }
                default:
                    ;       /* So not empty - to make old gcc happy */
                }
            }
/* We need to fudge things here to get nextarg() to evaluate a
 * command-line text and return the resulting string
 * We have a function to do that...
 */
            db_strdef(result);
            evaluate_cmdb(db_val(fnc_buf), &result);
            db_free(fnc_buf);
            db_append(repl, db_val(result));
            db_free(result);
            break;
        }
        default:;
        }
    rmcptr++;
    }
    return db_val_nc(repl);
}

/* last_match info -- this is used to get the last match in a query replace
 * to allow a one-level undo.
 * The data is put in place by delins().
 * It can be invalidated by setting mline to NULL;
 * match and replace are never freed, just overwritten and realloc()ed
 */
static struct {
    char *match;            /* Text of the match */
    char *replace;          /* Text of the replacing string */
    struct line *mline;     /* Line it is on */
    int moff;               /* Start offset on mline */
    int mlen;               /* Length of match */
    int rlen;               /* Length of the replacement */
    int m_alloc;            /* Allocated size of match */
    int r_alloc;            /* Allocated size of replace */
} last_match = { NULL, NULL, NULL, 0, 0, 0, 0, 0 };

/* delins -- delete the match and insert the replacement
 * We need to end up at the end of the replacement string.
 * The replacement string is the actual text to insert - our caller
 * will have done any replacement magic already.
 */
static int delins(const char *repstr) {

/* Remember what the replacement was */

    last_match.rlen = strlen(repstr);
    int needed = last_match.rlen + 1;
    if (needed > last_match.r_alloc) {
        last_match.replace = Xrealloc(last_match.replace, needed);
        last_match.r_alloc = needed;
    }
    strcpy(last_match.replace, repstr);

/* Save the matched string in last_match (only allow 1 level of undo).
 * NOTE1: The match may be over multiple lines so we need a byte-by-byte
 * copy.
 * NOTE2: We cannot save mline until the end, as it might change!
 */
    needed = match_grp_info[0].len + 1;
    if (needed > last_match.m_alloc) {
        last_match.match = Xrealloc(last_match.match, needed);
        last_match.m_alloc = needed;
    }
/* This does NOT change the current dotp and doto! */
    struct line *sline = curwp->w.dotp;
    int soff = curwp->w.doto;
    char *mptr = last_match.match;
    for (int j = 0; j < match_grp_info[0].len; j++)
        *mptr++ = nextbyte(&sline, &soff, FORWARD);
    terminate_str(mptr);

/* Now that the text of a line is reallocated without reallocating the
 * line structure itself, we can save the line pointer here.
 * Previously we needed to save the previous line, do the delete/insert,
 * then save the forward line from the one we had saved.
 */
    last_match.moff = curwp->w.doto;
    last_match.mlen = match_grp_info[0].len;
    last_match.mline = curwp->w.dotp;

/* We end up positioned at the *start* of a match, so we can just delete
 * what we found (by byte count) then insert the new text.
 */
    int status = ldelete(match_grp_info[0].len, FALSE);

/* Add what we need to add. We will end up at the end of it, which
 * is where we wish to be.
 */
    if (status) {
        status = linstr(repstr);
        last_match.rlen = strlen(repstr);
    }
    return status;
}

/* replaces -- Search for a string and replace it with another
 *      string.  Query might be enabled (according to query).
 *
 * int query            Query enabled flag
 * int f;               default flag
 * int n;               # of repetitions wanted
 *
 * NOTE: that this ONLY EVER RUNS FORWARDS!!!
 *  so on a match we will be at the END of the string to replace.
 * ALSO: since this modifies as it goes we must ensure that the
 * group-match info is invalidated on ANY exit, so all exits
 * must go via the common one which does this.
 */
static int replaces(int query, int f, int n) {
    int numsub;             /* number of substitutions */
    int nummatch;           /* number of found matches */
    int nlflag;             /* last char of search string a <NL>? */
    int nlrepl;             /* was a replace done on the last line? */
    int c;                  /* input char for query - tgetc() returns unicode */
    struct line *origline;  /* original "." position */
    int origoff;            /* and offset (for . query option) */
    int undone = 0;         /* Set if we undo a replace */

    int status = TRUE;      /* Default assumption */

    int using_incremental_debug = FALSE;

    if (curbp->b_mode & MDVIEW) {   /* don't allow this command if  */
        status =  rdonly();         /* we are in read only mode     */
        goto end_replaces;
    }

/* Check for negative repetitions. */

    if (n < 0) {
        status = FALSE;
        goto end_replaces;
    }

/* Now the real check for in "incremental-debug" mode?
 * But only if we are querying!
 */
    using_incremental_debug = query && incremental_debug_check(1);

/* Ask the user for the text of a pattern. */

    if ((status = readpattern((query? "Query replace": "Replace"),
         &pat, TRUE)) != TRUE)
        goto end_replaces;

/* Ask for the replacement string. */

    if ((status = readpattern("with", &rpat, FALSE)) == ABORT)
        goto end_replaces;

/* Set up flags so we can make sure not to do a recursive replace on
 * the last line because we're replacing the final newline...
 */
    nlflag = (db_charat(pat, srch_patlen - 1) == '\n');
    nlrepl = FALSE;

/* Save original dot position, init the number of matches and substitutions,
 * and scan through the file.
 */
    origline = curwp->w.dotp;
    origoff = curwp->w.doto;
    numsub = 0;
    nummatch = 0;
    last_match.mline = NULL;    /* No last match, yet */

    while ((f == FALSE || n > nummatch) &&
           (nlflag == FALSE || nlrepl == FALSE)) {

        const char *match_p, *repl_p;

/* Search for the pattern. The true length of the matched string ends up
 * in match_grp_info[0].len
 */
        if (slow_scan) {    /* All done? */
            if (!step_scanner(mcpat, FORWARD, PTBEG)) break;
        }
        else {              /* All done? */
            if (!fast_scanner(db_val(pat), FORWARD, PTBEG)) break;
        }
        ++nummatch;     /* Increment # of matches */

/* Check if we are on the last line. */

        nlrepl = (lforw(curwp->w.dotp) == curwp->w_bufp->b_linep);

/* Set match_p and repl_p */

        match_p = group_match(0);
        if (rmagical) repl_p = getrepl();
        else          repl_p = db_val(rpat);

/* Check for query mode. */

        if (query) {     /* Get the query. */
pprompt:

/* Build query replace question string dynamically.
 * Both group_match(0) and getrepl() may change on successive matches
 * in Magic mode.
 * To allow for undo we set repl_p here and pass it to delins().
 * NOTE that rpat can't change on an undo unless rmagical is set.
 */
            if (undone) {
                undone = 0;
                match_p = last_match.match;
                if (rmagical) repl_p = last_match.replace;
                else          repl_p = db_val(rpat);
            }

/* We need to take a copy of one expandp() result, as it uses
 * a static buffer for its results.
 */
            dbp_dcl(ep) = expandp(match_p);
            char *rt = strdupa(dbp_val(ep));
            ep = expandp(repl_p);
            mlwrite("Replace '%s' with '%s'? ", rt, dbp_val(ep));

qprompt:
            update(TRUE);   /* show the proposed place to change */
            c = tgetc();    /* and input */
            mlwrite_one("");    /* and clear it */

/* And respond appropriately. GGR - make case-insensitive (lower..) */

            if ('A' <= c && c <= 'Z') c -= 'A' - 'a';
            switch (c) {
            case 'y':       /* yes, substitute */
            case ' ':
                break;

            case 'n':       /* no, onwards */
                forw_grapheme(1);
                continue;

            case '!':       /* yes/stop asking */
                query = FALSE;
                break;

            case 'u':       /* undo last and re-prompt */

/* Set the undone flag early, since even if we can't undo we do
 * want to use the previous match.
 */
                undone = 1;

/* In order to be able to undo the previous replace (one-level only!!)
 * we need to know:
 *  1. what the previously matched text was (so it can be restored)
 *  2. the starting position of the replacement (so we know where to work)
 *  3. the length of what was replaced (so we know how much to remove)
 *
 *  So we need to remember this information on each match, and invalidate
 *  any match each time we start.
 */

 /* Restore old position. */
                if (last_match.mline == NULL) { /* Nothing to undo. */
                    mlwrite_one("No previous match to undo");
                    TTbeep();
                    sleep(1);
                    goto pprompt;
                }
                curwp->w.dotp = last_match.mline;
                struct line *pline = lback(last_match.mline);
                curwp->w.doto = last_match.moff;
                last_match.mline = NULL;        /* Invalidate it */

/* Delete the replacement we put in, then insert the last match.
 * This will leave us at the end of it. We actually want to be at the
 * start, so go there. We use prev-line and forw(prev-line) as that
 * works even in the current line gets reallocated from the insert.
 */
                ldelete(last_match.rlen, FALSE);
                linstr(last_match.match);
                curwp->w.dotp = lforw(pline);
                curwp->w.doto = last_match.moff;

/* Record one less substitution and reprompt. */
                --numsub;
                goto pprompt;

            case '.':       /* abort! and return */
/* Restore old position */
                curwp->w.dotp = origline;
                curwp->w.doto = origoff;
                curwp->w_flag |= WFMOVE;
                /* Falls through */

            case BELL:      /* abort! and stay */
                mlwrite_one("Aborted!");
                status = FALSE;
                goto end_replaces;

            default:        /* bitch and beep */
                TTbeep();
                /* Falls through */
            case '?':       /* help me */
                mlwrite_one(
  "(Y)es, (N)o, (!)Do rest, (U)ndo, (^G)Abort, (.)Abort+back, (?)Help: ");
                goto qprompt;

            }       /* end of switch */
        }           /* end of "if query" */

/* Stop a potential loop...
 * If we've matched a zero-length match (which could happen in Magic mode)
 * then we'd loop forever.
 */
        if (match_grp_info[0].len == 0) {
            mlwrite_one("Replacing a zero-length match (loop). Aborting...");
            status = FALSE;
            goto end_replaces;
        }

/* Delete the sucker, and insert its replacement. */

        status = delins(repl_p);
        if (status != TRUE) goto end_replaces;

        numsub++;       /* increment # of substitutions */
    }

/* And report the results. */
    mlwrite("%d substitutions", numsub);

/* Invalidate the group matches when we leave */

end_replaces:
    init_dyn_group_status();
    if (using_incremental_debug) incremental_debug_cleanup();
    return TRUE;
}

/* sreplace -- Search and replace.
 *
 * int f;               default flag
 * int n;               # of repetitions wanted
 */
int sreplace(int f, int n) {
    return replaces(FALSE, f, n);
}

/* qreplace -- search and replace with query.
 *
 * int f;               default flag
 * int n;               # of repetitions wanted
 */
int qreplace(int f, int n) {
    int saved_discmd = discmd;      /* Save this in case we change it. */
    int retval = replaces(TRUE, f, n);
    discmd = saved_discmd;          /* Restore original... */
    return retval;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_search(void) {
    Xfree(last_match.match);
    Xfree(last_match.replace);
    for (int gi = 0; gi < max_grp; gi++) Xfree(grp_text[gi]);
    Xfree(cntl_grp_info);
    Xfree(match_grp_info);
    Xfree(grp_text);
    for (int ix = 0; ix < RING_SIZE; ix++) {
        Xfree(srch_txt[ix]);
        Xfree(repl_txt[ix]);
    }
    if (mc_alloc) mcclear();
    rmcclear();
    Xfree(mcpat);
    Xfree(rmcpat);

    db_free(repl);
    db_free(pat);
    db_free(tap);
    db_free(rpat);
    db_free(expbuf);
    db_free(btbuf);

    return;
}
#endif
