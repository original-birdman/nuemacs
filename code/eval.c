/*      eval.c
 *
 *      Expression evaluation functions
 *
 *      written 1986 by Daniel Lawrence
 *      modified by Petri Kutvonen
 */

#include <stdio.h>
#include <sys/utsname.h>
#include <stddef.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#include <fenv.h>
#if __sun
#include <alloca.h>
#endif

#define EVAL_C

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "evar.h"
#include "line.h"
#include "util.h"
#include "version.h"
#include "idxsorter.h"

#if RANDOM_SEED
#include <sys/types.h>
#include <unistd.h>
#endif
static int seed;

/* We now want all uemacs "macro integers" to be 64-bit. */

/* ue_atoi
 * This allows the integer to be expressed in hex ("0x"), octal (0..)
 * or decimal.
 * Replaced with a simple #define macro.
 */
#define ue_atoi(ustr) (strtoll(ustr, NULL, 0))

/* ue_itoa:
 *      integer to ascii string.......... This is too
 *      inconsistent to use the system's
 *      Actually, this is no longer true...
 *
 * int i;               integer to translate to a string
 */
#define INTWIDTH        sizeof(ue64I_t) * 3
const char *ue_itoa(ue64I_t i) {
    static char result[INTWIDTH+1];     /* Enough for resulting string */

    sprintf(result, "%lld", i);
    return result;
}


/* Return the contents of the first item in the kill buffer
 */
static db_strdef(kvalue);      /* fixed buffer for value */
static const char *getkill(void) {
    db_set(kvalue, "");     /* default: no kill buffer....just null string */
    if (kbufh[0] != NULL) { /* else, copy in the contents...all of it */
        struct kill *kp = kbufh[0];
        while (kp != NULL) {
            int nb;
            if (kp->d_next == NULL) nb = kused[0];
            else                    nb = KBLOCK;
            char *sp = kp->d_chunk;
            db_appendn(kvalue, sp, nb);
            kp = kp->d_next;
        }
    }
    return db_val(kvalue);  /* Return the constructed value */
}

/* A user-settaable environment variable allowing the user to define
 * the initial sorting for a directory display.
 * The user may give up to 5 settings, 1-char for each.
 */
#define MAX_SD_OPTS 5
static char showdir_opts[MAX_SD_OPTS+1] = "";

/* User variables. External as used by completion code in input.c */

#define MAXVARS 64

/* This bit is internal. We keep a separate list of the names (uvnames) */
static struct simple_variable uv[MAXVARS];

/* Initialize the user variable list. */
void varinit(void) {
    int i;
    for (i = 0; i < MAXVARS; i++) {
        uv[i].value = NULL;
        uv[i].name[0] = 0;
    }
/* Take advantage of this initialization call */
#if RANDOM_SEED
    seed = abs(getpid());   /* abs() in case of overflow to -ve */
#else
    seed = 0;
#endif
}

/* SORT ROUTINES
 * Some parts are external for use by completion code in input.c
 */

int *envvar_index = NULL;
int *next_envvar_index;
static int evl_size = ARRAY_SIZE(evl);

/* env var ($...) sorting */

void init_envvar_index(void) {
    struct fields fdef;

    fdef.offset = offsetof(struct evlist, var);
    fdef.type = 'S';
    fdef.len = 0;
    envvar_index = Xmalloc((evl_size+1)*sizeof(int));
    idxsort_fields((unsigned char *)evl, envvar_index,
          sizeof(struct evlist), evl_size, 1, &fdef);
/* We want to step through this one, so need a next index too */
    next_envvar_index = Xmalloc((evl_size+1)*sizeof(int));
    make_next_idx(envvar_index, next_envvar_index, evl_size);
    return;
}

/* A function to allow you to step through the index in order.
 * For each input index, it returns the next one.
 * If given -1, it will return the first item and when there are no
 * further entries it will return -1.
 * If the index is out of range it will return -2.
 */
int nxti_envvar(int ci) {
    if (ci == -1) return envvar_index[0];
    if ((ci >= 0) && (ci < evl_size)) {
        int ni = next_envvar_index[ci];
        if (ni >= 0) return ni;
        return -1;
    }
    return -2;
}

/* User var (%...) sorting. Different - just build a sorted name array
 * of the names which exist.
 * We don't actually need the values for getf/nvar() in input.c
 */

char *uvnames[MAXVARS];
static int n_uvn;

void sort_user_var(void) {
    n_uvn = 0;
    for (int i = 0; i < MAXVARS; i++) {

/* When user vars are deleted (del_simple_var) the remaining ones are
 * moved to fill in the gap. So a null name is always the end of the list.
 */
        if (uv[i].name[0] == '\0') break;

/* Need to add this one into uvnames in alphabetic order and push any
 * followers down.
 */
        char *toadd = uv[i].name;
        for (int j = 0; j < n_uvn; j++) {
            if (strcmp(toadd, uvnames[j]) >= 0) continue;
            char *xp = uvnames[j];
            uvnames[j] = toadd;
            toadd = xp;
        }
        uvnames[n_uvn] = toadd;
        uvnames[++n_uvn] = NULL;
    }
    return;
}

/* Simple function to get the index of the next name.
 * Just advance by one if there is a further entry.
 */
int nxti_usrvar(int ci) {
    ci++;
    if (ci >= 0 && ci < n_uvn) return ci;
    return -1;
}

/* Convert a string to a numeric logical
 * Used by exec.c
 *
 * char *val;           value to check for stol
 */
int stol(const char *val) {
/* check for logical values */
    if (val[0] == 'F') return FALSE;
    if (val[0] == 'T') return TRUE;

/* check for numeric truth (!= 0) */
    return (ue_atoi(val) != 0);
}

/* Numeric logical to string logical
 *
 * int val;             value to translate
 *
 * Replaced with a simple #define macro
 */
#define ltos(v) ((v)? truem: falsem)

/* Make a string lower case
 * Internal to this routine - only needs to handle ASCII as it is only
 * used for the function names defined for this code.
 *
 * char *str;           string to lower case
 */
static char *mklower(char *str) {
    for (char *sp = str; *sp; sp++) {
        if ('A' <= *sp && *sp <= 'Z') *sp += 'a' - 'A';
    }
    return str;
}

/* Find pattern within source.
 * Returns grapheme index of the start, or 0.
 *
 * char *source;        source string to search
 * char *pattern;       string to look for
 */
static int strindex(const char *source, const char *pattern) {
    if (pattern[0] == '\0') return 0;
    char *locp = strstr(source, pattern);
    int res;
    if (!locp) res = 0; /* Not found */
/* For a non-zero result, convert to graphemes */
    else res = glyphcount_utf8_array(source, locp - source) + 1;
    return res;
}

/* Find the last pattern within source.
 * Returns grapheme index of the start, or 0.
 *
 * char *source;        source string to search
 * char *pattern;       string to look for
 */
static int rstrindex(const char *source, const char *pattern) {
    if (pattern[0] == '\0') return 0;

/* We keep trying from one past where the last match matched
 * until we fail.
 */
    const char *locp = source - 1;
    const char *lastp = NULL;
/* () around assigment used for result. */
    while((locp = strstr(locp+1, pattern))) lastp = locp;
    int res;
    if (!lastp) res = 0;    /* Not found */
/* For a non-zero result, convert to graphemes */
    else res = glyphcount_utf8_array(source, lastp - source) + 1;
    return res;
}

/* FreeBSD (cclang) doesn't have strndupa(), so make our own....
 * ...or rather copy the GCC definition from string.h.
 * If anything has strndupa as something other than a #define
 * this logic will need to be changed.
 */
#ifndef strndupa
 #define strndupa(s, n)                             \
  (__extension__                                    \
    ({                                              \
      const char *__old = (s);                      \
      size_t __len = (n);                           \
      char *__new = __builtin_alloca (__len + 1);   \
      __new[__len] = '\0';                          \
      memcpy (__new, __old, __len);                 \
    }))
#endif

/* Filter a string through a translation table
 *
 * char *source;        string to filter
 * char *lookup;        characters to translate
 * char *trans;         resulting translated characters
 *
 * This version now works on graphemes, and if the trans list
 * is shorter than the lookup list then the "trailing" matches
 * from lookup result in removals from the result.
 * Although the mapping is by grapheme, the actual string
 * replacing is done as utf8-bytes.
 * There is no Unicode EQUIV ability here...
 */
struct map_table {
    union { char c; char *utf8; } from;
    union { char c; char *utf8; } to;
    int fr_len;
    int to_len;
};

static db_strdef(xlres);
static const char *xlat(const char *source, const char *lookup,
     const char *trans) {

/* There cannot be more mappings than the number of bytes in the lookup.
 * So allocate a table of that size now.
 */
    int llen = strlen(lookup);
    int tlen = strlen(trans);
/* alloca() allocates on the stack, so automatically de-allocates
 * when we leave.
 */
    struct map_table *mtp = alloca(llen*sizeof(struct map_table));

/* Walk along lookup and trans filling in the mappings.
 * If we run out of trans, we mark them as removals.
 * If there is any trans left when we run out of lookup, it's an
 * error.
 */
    const char *lp = lookup;
    const char *tp = trans;
    struct map_table *wmtp = mtp;
    int mtlen = 0;
    while (*lp) {
        struct grapheme gph;
        int used;

/* Build the next lookup grapheme.
 * We're not actually interested in its Unicode value(s), Just how many
 * bytes (chars) it uses. So don't allocate any extended bits.
 */
        used = build_next_grapheme(lp, 0, llen, &gph, 1);
        if (used == 1) {
            wmtp->from.c = *lp;
            wmtp->fr_len = 1;
        }
        else {
/* strndupa uses alloca, so no need for us to free() */
            wmtp->from.utf8 = strndupa(lp, used);
            wmtp->fr_len = used;
        }
        lp += used;
        llen -= used;
/* Build the next trans grapheme.
 * We're not actually interested in its Unicode value(s), Just how many
 * bytes (chars) it uses.
 */
        if (!*tp)
            wmtp->to_len = 0;
        else {
            used = build_next_grapheme(tp, 0, tlen, &gph, 1);
            if (used == 1) {
                wmtp->to.c = *tp;
                wmtp->to_len = 1;
            }
            else {
                wmtp->to.utf8 = strndupa(tp, used);
                wmtp->to_len = used;
            }
            tp += used;
            tlen -= used;
        }
/* Advance to next map_table entry and count how many we have */
        wmtp++;
        mtlen++;
    }

/* Out of lookup - check that trans has run out too... */
    if (*tp) {
        mlforce("Translation table longer than lookup table.");
        return "ERROR";
    }

/* Now copy the source to the result, mapping any matching bytes/strings */

    const char *sp = source;
    db_clear(xlres);    /* Empty at start */
    while (*sp) {
        int ri = -1;    /* No matching index - yet */
        for (int i = 0; i < mtlen; i++) {
            if (mtp[i].fr_len == 1) {
                if (mtp[i].from.c == *sp) {
                    ri = i;
                    break;
                }
            }
            else {
                if (strncmp(sp, mtp[i].from.utf8, mtp[i].fr_len) == 0) {
                    ri = i;
                    break;
                }
            }
        }
        if (ri >= 0) {  /* We found a match, so lookup the translation */
            switch(mtp[ri].to_len) {
            case 0:         /* Remove from output */
                break;
            case 1:
                db_addch(xlres, mtp[ri].to.c);
                break;
            default:
                db_appendn(xlres, mtp[ri].to.utf8, mtp[ri].to_len);
            }
            sp += mtp[ri].fr_len;
        }
        else            /* Just copy the source byte to the result */
            db_addch(xlres, *sp++);
    }
    return db_val_nc(xlres);
}

/* ptt_expand
 * A function to expand the given string with ptt handling.
 * This expects to be given a single lookup item as it ONLY
 * expands for the final character.
 * Meant for use by test scripts, but might have other uses as
 * a lookup method?
 */
static db_strdef(pttres);
static const char *ptt_expand(db *str) {
    struct buffer *bp;

/* If there is no active PTT, return the given text */

    if (!(curbp->b_mode & MDPHON)) {
        db_set(pttres, dbp_val(str));
        return db_val(pttres);
    }

/* The work is done by creating a new temporary buffer, placing the string
 * in it, removing the final char(grapheme), then calling ptt_handler to
 * add the final char and returning the buffer contents as the result.
 * ptt_handler() has to set no_newline_in_pttex to indicate whether the
 * final newline is valid.
 */
    if ((bp = bfind("//ptt_expander", TRUE, BFINVS)) == NULL) {
        return errorm;
    }
    bclear(bp);                 /* Ensure empty */
    struct buffer *sbp = curbp; /* save the old buffer */
    swbuffer(bp, 0);
    bp->b_mode = curbp->b_mode; /* Set mode to my original mode */
    linstr(dbp_val(str));       /* Add string */
    backdel(0, 1);              /* Remove last grapheme */
                                /* Re-add via handler */
    ptt_handler(dbp_charat(str, dbp_len(str)-1), FALSE);

/* Allow for the expansion being multi-line */

    db_set(pttres, "");
    for (struct line *lp = lforw(curbp->b_linep); lp != curbp->b_linep;
         lp = lforw(lp)) {
        db_appendn(pttres, ltext(lp), lused(lp));
        db_addch(pttres, '\n');
     }

/* Is the final newline really part of the expansion? */
    if (no_newline_in_pttex) {  /* Remove it */
        db_setcharat(pttres, db_len(pttres)-1, '\0');
    }
    swbuffer(sbp, 0);
    return db_val(pttres);
}

/* ue_printf
 *
 * printf-like handling for multiple args together...
 * All values are strings, but this allows for integer and float args too
 * by converting them from the strings then formatting them according
 * to the template.
 * The formatting does not allow the use of any formatting optins that
 * specify specific parameters (* and $) and ignores options which specify
 * parameter sizes.
 *
 * The output is built up in a local buffer and only copied to the
 * single static buffer at exit. This is to allow for re-entrancy.
 */

/* The returned value - only set this on exit! */
static db_strdef(ue_buf);

/* The order here is IMPORTANT!!!
 * It is int, unsigned, double, char, str, ptr
 * The latter isn't of any practical use, but is a valid conversion char
 */
#define TMPL_CHAR "di ouxX eEfFgGaA c s p"
#define TMPL_SKIP "hlqLjzZt"

static const char *ue_printf(const char *fmt) {
    unicode_t c;
    db_strdef(nexttok);
    db_strdef(lue_buf);
    db_strdef(t_fmt);

/* GGR - loop through the bytes getting any utf8 sequence as unicode */
    int bytes_togo = strlen(fmt);
    if (bytes_togo == 0) goto finalize; /* Nothing else...clear line only */

    while (bytes_togo > 0) {
        int used = utf8_to_unicode(fmt, 0, bytes_togo, &c);
        bytes_togo -= used;
        if (c != '%') {         /* So copy in the *bytes*! */
            db_appendn(lue_buf, fmt, used);
            fmt += used;
            continue;
        }

/* We have a '%': test the next char */

        fmt += used;
        if (*fmt == '%') {      /* %% is a literal '%' */
            db_addch(lue_buf, '%');
            fmt++;
            bytes_togo--;
            continue;
        }

/* Now keep going until we find a conversion specifier, to pick up the
 * template - but skip anything we don't want, and any attempt to
 * specify specific args to use is an error.
 * We know this should just be ASCII, so can go byte-by-byte.
 */
        db_set(t_fmt, "%");      /* Start the template... */
        while (!strchr(TMPL_CHAR, *fmt)) {
            if ((*fmt == '*') || (*fmt == '$')) {
                db_set(lue_buf, errorm);
                goto finalize;
            }
            if (!strchr(TMPL_SKIP, *fmt)) db_addch(t_fmt, *fmt);
            fmt++;
            if (--bytes_togo == 0) {
                db_set(lue_buf, errorm);
                goto finalize;
            }
        }

/* The current char MUST be in TMPL_CHAR - anything else is an error! */
        if (!strchr(TMPL_CHAR, *fmt)) {
            db_set(lue_buf, errorm);
            goto finalize;
        }
        --bytes_togo;   /* We're about to use it... */

/* We have to specify the precision. Since we use 64-bits for integer
 * variables we have to use that precision here.
 */
        if (strchr("diouxX", *fmt)) {
            db_append(t_fmt, "ll");
        }
        char conv_char = *fmt++;
        db_addch(t_fmt, conv_char);

/* Get what to format. */
        if (macarg(&nexttok) != TRUE) { /* Done */
             bytes_togo = 0;    /* So we exit the loop */
             continue;
        }

/* We now have nexttok as a string. Need to convert it to the correct
 * type for sprintf to format it if it is numeric.
 * Note that we still need to format a string (not just copy it) as
 * it may have specified field width or justifiction.
 */
        if ((conv_char == 'p')  /* Not useful...but valid option */
            || (conv_char == 's')) {
            db_sprintf(glb_db, db_val(t_fmt), db_val(nexttok));
        }
        else if (conv_char == 'c') {
            db_sprintf(glb_db, db_val(t_fmt), db_charat(nexttok, 0));
        }
        else {  /* Check in reverse order of presence in TMPL_CHAR */
            char *t_pos = (strchr(TMPL_CHAR, conv_char));
            if (t_pos >= &TMPL_CHAR[8]) {       /* Doubles */
                double dv = strtod(db_val(nexttok), NULL);
                db_sprintf(glb_db, db_val(t_fmt), dv);
            }
            else if (t_pos >= &TMPL_CHAR[3]) {  /* Unsigned */
                unsigned long long uv = strtoull(db_val(nexttok), NULL, 10);
                db_sprintf(glb_db, db_val(t_fmt), uv);
            }
            else {                              /* Must be signed */
                long long lv = strtoll(db_val(nexttok), NULL, 10);
                db_sprintf(glb_db, db_val(t_fmt), lv);
            }
        }
        db_append(lue_buf, db_val(glb_db));
    }

/* Finalize the result with a NUL char an move it to the static buffer
 * so we can return a pointer to it.
 */
finalize:
    db_free(t_fmt);
    db_free(nexttok);

/* Now switch lue_buf to ue_buf, freeing any original ue_buf */
    db_free(ue_buf);
    ue_buf = lue_buf;
    return db_val_nc(ue_buf);
}

/* Evaluate a function.
 *
 * @fname: name of function to evaluate.
 */
static db_strdef(funres);      /* Freed in free_eval(), if at all */
static const char *gtfun(const char *fname) {
    char lfname[4];         /* What we lookup */
    unsigned int fnum;      /* index to function to eval */
    int status;             /* status */
    const char *tsp;        /* Temporary string pointer */
    db_strdef(arg1);        /* Value of first argument */
    db_strdef(arg2);        /* Value of second argument */
    db_strdef(arg3);        /* Value of third argument */
    const char *retval;     /* Value to return */
    struct mstr csinfo;     /* Casing info structure */
    ue64I_t int1, int2 = 0;

/* Look the function up in the function table */
    strncpy(lfname, fname, 4);
    lfname[3] = 0;          /* only first 3 chars significant */
    mklower(lfname);        /* and let it be upper or lower case */
    for (fnum = 0; fnum < ARRAY_SIZE(funcs); fnum++)
        if (strcmp(lfname, funcs[fnum].f_name) == 0) break;

/* Return errorm on a bad reference */
    if (fnum == ARRAY_SIZE(funcs)) {
        retval = errorm;
        goto exit;
    }

/* Retrieve the required arguments */

    do {
        int ft = funcs[fnum].f_type;
        if (ft == NILNAMIC) {
            status = TRUE;
            break;
        }
        if ((status = macarg(&arg1)) != TRUE) break;
        if (ft == MONAMIC) break;
        if ((status = macarg(&arg2)) != TRUE) break;
        if (ft == DINAMIC) break;
        if ((status = macarg(&arg3)) != TRUE) break;
    } while(0);     /* A 1-pass loop */
    if (status != TRUE) {
        retval = errorm;
        goto exit;
    }

/* And now evaluate it!
 * The switch statements are grouped by functionality type so they
 * can share the initial arg1/(arg2/arg3 conversions.
 */
    int tag = funcs[fnum].tag;  /* Useful for internal switches */
    switch(funcs[fnum].tag) {   /* enum checks all values are handled */

/* Integer arithmetic*/
    case UFADD:
    case UFSUB:
    case UFTIMES:
    case UFDIV:
    case UFMOD: {
        int2 = ue_atoi(db_val(arg2));
        if ((tag == UFDIV) || (tag == UFMOD)) {
            if (int2 == 0) {    /* The only "illegal" case for integer maths */
                retval = "ZDIV";
                goto exit;
            }
        }
    }   /* Falls through */
    case UFNEG:
    case UFABS: {
        int1 = ue_atoi(db_val(arg1));
        switch(tag) {
        case UFADD:   retval = ue_itoa(int1 + int2); break;
        case UFSUB:   retval = ue_itoa(int1 - int2); break;
        case UFTIMES: retval = ue_itoa(int1 * int2); break;
        case UFDIV:   retval = ue_itoa(int1 / int2); break;
        case UFMOD:   retval = ue_itoa(int1 % int2); break;
        case UFNEG:   retval = ue_itoa(-int1); break;
        default: /* UFABS */    retval = ue_itoa(llabs(int1));
        }
        goto exit;
    }

/* Logical operators */
    case UFEQUAL:
    case UFLESS:
    case UFGREATER: {
        int1 = ue_atoi(db_val(arg1));
        int2 = ue_atoi(db_val(arg2));
        switch(tag) {
        case UFEQUAL:   retval = ltos(int1 == int2); break;
        case UFLESS:    retval = ltos(int1 < int2); break;
        default:        retval = ltos(int1 > int2);     /* UFGREATER */
        }
        goto exit;
    }
    case UFAND:
    case UFOR:
        int2 = stol(db_val(arg2));      /* Falls through */
    case UFNOT:
        int1 = stol(db_val(arg1));
        switch(tag) {
        case UFNOT: retval = ltos(int1 == FALSE); break;
        case UFAND: retval = ltos(int1 && int2); break;
        default:    retval =  ltos(int1 || int2);   /* UFOR */
        }
        goto exit;

/* Bitwise functions */
    case UFBAND:
    case UFBOR:
    case UFBXOR:
        int2 = ue_atoi(db_val(arg2));   /* Falls through */
    case UFBNOT:
    case UFBLIT:
        int1 = ue_atoi(db_val(arg1));
        switch(tag) {
        case UFBAND:    retval = ue_itoa(int1 & int2); break;
        case UFBOR:     retval = ue_itoa(int1 | int2); break;
        case UFBXOR:    retval = ue_itoa(int1 ^ int2); break;
        case UFBNOT:    retval = ue_itoa(~int1); break;
        default:        retval = ue_itoa(int1); /* UFBLIT */
        }
        goto exit;

/* String functions */
    case UFCAT:
        db_set(funres, db_val(arg1));
        db_append(funres, db_val(arg2));
        retval = db_val(funres);
        goto exit;

/* There is some similarity between the beginning and ending of
 * &lef, &rig and &mid, so code for that.
 * The count is now in graphemes, not bytes.
 */
    case UFLEFT:
    case UFRIGHT:
    case UFMID: {
        const char *rp; /* Where the return value starts */
        int offs;       /* Eventually, how much to return */
        int inbytes = strlen(db_val(arg1));
        int gph_count = ue_atoi(db_val(arg2));
        if (gph_count <= 0) {
            retval = "";
            goto exit;
        }

/* Now the call-specific bits
 * For UFLEFT and UFMID we need to count from start of the string.
 * UFLEFT needs us to go just beyond the last char, but UFMID needs us
 * end up just before it.
 */
        if (tag != UFRIGHT) {   /* So it's UFLEFT or UFMID */
            int reloop = FALSE;
            if (tag == UFMID) {
                gph_count--;    /* So we get start pos... */
                reloop = TRUE;  /* ...and run through loop twice */
            }

/* Now we step over the chars.
 * For UFMID we then set things up to run again the scan again
 * continuing from, and remembering, where we reached on the first pass.
 */
            rp = db_val(arg1);
            offs = 0;
            while (1) {
                while (gph_count--) {
                    int next_offs = next_utf8_offset(rp, offs, inbytes, TRUE);
                    if (next_offs < 0) break;   /* No bytes left */
                    offs = next_offs;
                }
                if (!reloop) break;
/* Set things up for UFMID's second pass through the loop */
                reloop = FALSE;     /* Only reloop once */
                rp = db_val(arg1) + offs;   /* What we have left */
                offs = 0;
                gph_count = ue_atoi(db_val(arg3));  /* How much to get */
                if (gph_count <= 0) {
                    retval = "";
                    goto exit;
                }
            }
        }
/* The UFRIGHT scan runs backwards....
 */
        else {                  /* So is UFRIGHT */
            offs = inbytes;     /* Start at other end */
            while (gph_count--) {
                offs = prev_utf8_offset(db_val(arg1), offs, TRUE);
                if (offs == 0) break;   /* No bytes left */
            }
            rp = db_val(arg1)+offs;
            offs = inbytes - offs;
        }
        db_setn(funres, rp, offs);
        retval = db_val(funres);
        goto exit;
    }

    case UFSEQUAL:
        retval = ltos(strcmp(db_val(arg1), db_val(arg2)) == 0);
        goto exit;
    case UFSLESS:
        retval = ltos(strcmp(db_val(arg1), db_val(arg2)) < 0);
        goto exit;
    case UFSGREAT:
        retval = ltos(strcmp(db_val(arg1), db_val(arg2)) > 0);
        goto exit;
    case UFLENGTH:
        retval = ue_itoa(glyphcount_utf8(db_val(arg1)));
        goto exit;
    case UFUPPER:
    case UFLOWER:
        utf8_recase(tag == UFUPPER? UTF8_UPPER: UTF8_LOWER,
             db_val(arg1), -1, &csinfo);
        db_set(funres, csinfo.str);
        Xfree(csinfo.str);
        retval = db_val(funres);
        goto exit;
    case UFESCAPE: {            /* Only need to escape ASCII chars */
        const char *ip = db_val(arg1);  /* This is SHELL escaping... */
        db_set(funres, "");  /* Start empty */
        while (*ip) {           /* Escape it? */
            if (strchr(" \"$&'()*;<>?\\`{|", *ip)) db_addch(funres, '\\');
            db_addch(funres, *ip++);
        }
        retval = db_val(funres);
        goto exit;
    }
    case UFSINDEX:
        retval = ue_itoa(strindex(db_val(arg1), db_val(arg2)));
        goto exit;
    case UFRINDEX:
        retval = ue_itoa(rstrindex(db_val(arg1), db_val(arg2)));
        goto exit;

/* Miscellaneous functions */
    case UFIND: {   /* Evaluate the next arg via temporary execstr */
        const char *oldestr = execstr;
        execstr = strdupa(db_val(arg1));    /* Writable copy */
        macarg(&funres);
        execstr = oldestr;
        retval = db_val(funres);
        goto exit;
    }

    case UFTRUTH:
        retval = ltos(ue_atoi(db_val(arg1)) == 42);
        goto exit;
    case UFASCII: {     /* Returns base unicode char - but keep old name... */
        unicode_t uc_res;
        (void)utf8_to_unicode(db_val(arg1), 0, strlen(db_val(arg1)), &uc_res);
        retval = ue_itoa(uc_res);
        goto exit;
    }
/* Allow for unicode as:
 *      decimal codepoint
 *      hex codepoint   (0x...)
 *      U+hex           [0x must be chars 1 and 2]
 */
    case UFCHR:
        if ((db_charat(arg1, 0) == 'U') && (db_charat(arg1, 1) == '+')) {
            static char targ[20] = "0x";    /* Fudge to 0x instead */
            strcpy(targ+2, db_val(arg1)+2); /* strtol then handles it */
            tsp = targ;
        }
        else {
            tsp = db_val(arg1);
        }
        char temp[8];
        int nb = unicode_to_utf8(strtol(tsp, NULL, 0), temp);
        terminate_str(temp+nb);
        db_set(funres, temp);
        retval = db_val(funres);
        goto exit;
    case UFGTKEY: {     /* Allow for unicode input. -> utf-8 */
        char temp[8];
        int nb = unicode_to_utf8(tgetc(), temp);
        db_setn(funres, temp, nb);
        retval = db_val(funres);
        goto exit;
    }
    case UFRND: {
            seed = abs(seed * 1721 + 10007);
            retval = ue_itoa(seed % llabs(ue_atoi(db_val(arg1))) + 1);
            goto exit;
    }
    case UFENV:
        tsp = getenv(db_val(arg1));
        retval = (tsp == NULL)? "" : tsp;
        goto exit;
    case UFBIND:
        retval = transbind(db_val(arg1));
        goto exit;
    case UFEXIST:   /* Need to "fixup" the arg */
        retval = ltos(fexist(fixup_fname(db_val(arg1))));
        goto exit;
    case UFBXIST:
        retval = ltos(bfind(db_val(arg1), 0, 0) != NULL);
        goto exit;
    case UFFIND:
        tsp = flook(db_val(arg1), TRUE, ONPATH);
        retval = (tsp == NULL)? "" : tsp;
        goto exit;
    case UFXLATE:
        retval = xlat(db_val(arg1), db_val(arg2), db_val(arg3));
        goto exit;
    case UFGRPTEXT:
        retval = group_match(ue_atoi(db_val(arg1)));
        goto exit;
    case UFPRINTF:
        retval = ue_printf(db_val(arg1));
        goto exit;
    case UFPTTEX:
        retval = ptt_expand(&arg1);
        goto exit;

/* Real arithmetic is to precision 12 in G-style strings.
 * Use &ptf to format the results as you wish.
 * The first 5 all take 2 real args and retval = 1 real result (as strings)
 * Overflow results in +/-INF
 * Undeflow results in 0.
 * Invalid bit patterns result in NAN - but you shouldn't be able
 * to get those. However, you can enter NAN as a number, or
 * multiply INF by 0.
 * Arithmetic with these string values should still work.
 *
 */
    case UFRADD:
    case UFRSUB:
    case UFRTIMES:
    case UFRDIV:
    case UFRPOW:
    case UFRLESS:
    case UFRGREAT: {
        static char rdv_result[20];
        double d1 = strtod(db_val(arg1), NULL);
        double d2 = strtod(db_val(arg2), NULL);
        double res;
        switch(tag) {
        case UFRADD:    res = d1 + d2; break;
        case UFRSUB:    res = d1 - d2; break;
        case UFRTIMES:  res = d1 * d2; break;
        case UFRDIV:    res = d1 / d2; break;
        case UFRPOW:    res = pow(d1, d2); break;
        case UFRLESS:   retval = ltos(d1 < d2); goto exit;
        default:        retval = ltos(d1 > d2); goto exit;  /* UFRGREAT */
        }
        snprintf(rdv_result, 20, "%.12G", res);
/* Solaris may use inf, Inf, INF, infinity, Infinity or INFINITY
 * and nan or NaN.
 * Standardize them.
  */
#if __sun
    int si = (rdv_result[0] == '-')? 1: 0;
    if ((rdv_result[si] == 'i') || (rdv_result[si] == 'I'))
        strcpy(rdv_result+si, "INF");
    else if ((rdv_result[si] == 'n') || (rdv_result[si] == 'N'))
        strcpy(rdv_result+si, "NAN");
#endif
        retval = rdv_result;
        goto exit;
    }
/* There IS NO UFREQUAL!!! You should never compare reals for equality! */
    case UFR2I: {       /* Add checks for overflow on conversion */
        errno = 0;
        feclearexcept(FE_ALL_EXCEPT);
        int1 = lround(strtod(db_val(arg1), NULL));
        if ((errno != 0) || (fetestexcept(FE_INVALID|FE_OVERFLOW) != 0)) {
            retval = "TOOBIG";
        }
        else {
            retval = ue_itoa(int1);
        }
        goto exit;
    }
    }
    exit(-11);          /* Should never get here */

exit:
    db_free(arg1);
    db_free(arg2);
    db_free(arg3);
    return retval;
}

/* Look up a user var's value (%xxx)
 *
 * char *vname;                 name of user variable to fetch
 */
static const char *gtusr(const char *vname) {
    int vnum;       /* Ordinal number of user var */

/* Scan the list looking for the user var name */
    for (vnum = 0; vnum < MAXVARS; vnum++) {
        if (uv[vnum].name[0] == 0) return errorm;
/* If a user var is being used in the same statement as it is being set
 *      set %test &add %test 1
 * then we can up with the name existing, but no value set...
 * We must check for this to avoid a crash!
 */
        if (strcmp(vname, uv[vnum].name) == 0)
             return uv[vnum].value? uv[vnum].value: errorm;
    }

/* Return errorm if we run off the end */
    return errorm;
}

/* Look up a buffer var's value (.xxx)
 *
 * char *vname;                 name of user variable to fetch
 */
static const char *gtbvr(const char *vname) {
    int vnum;       /* Ordinal number of user var */

    if (!execbp || !execbp->bv) return errorm;
/* Scan the list looking for the user var name */
    struct simple_variable *tp = execbp->bv;
    for (vnum = 0; vnum < BVALLOC; vnum++, tp++) {
        if (!strcmp(vname, tp->name)) return tp->value? tp->value: errorm;
    }

/* Return errorm if we run off the end */
    return errorm;
}

/* gtenv()
 *
 * char *vname;                 name of environment variable to retrieve
 *
 * This returns things which already exist, so doesn't need any db vars.
 */
static const char *gtenv(const char *vname) {
    unsigned int vnum;  /* Ordinal number of var referenced */

/* Scan the list, looking for the referenced name */
    for (vnum = 0; vnum < ARRAY_SIZE(evl); vnum++)
        if (strcmp(vname, evl[vnum].var) == 0) break;

/* Return errorm on a bad reference */
    if (vnum == ARRAY_SIZE(evl)) {
        char *ename = getenv(vname);
        if (ename != NULL)
            return ename;
        else
            return errorm;
    }

/* Otherwise, fetch the appropriate value */
    switch (evl[vnum].tag) {
    case EVFILLCOL:         return ue_itoa(fillcol);
    case EVPAGELEN:         return ue_itoa(term.t_nrow);
    case EVCURCOL:          return ue_itoa(getccol() + 1);
    case EVCURLINE:         return ue_itoa(getcline());
    case EVCURWIDTH:        return ue_itoa(term.t_ncol);
    case EVCBUFNAME:        return curbp->b_bname;
    case EVCFNAME:          return curbp->b_rpname;
    case EVDEBUG:           return ue_itoa(macbug);
    case EVSTATUS:          return ltos(cmdstatus);
    case EVASAVE:           return ue_itoa(gasave);
    case EVACOUNT:          return ue_itoa(gacount);
    case EVLASTKEY:         return ue_itoa(lastkey);
    case EVCURCHAR:     /* Make this return the current Unicode base char */
        if (lused(curwp->w.dotp) == (size_t)curwp->w.doto) return ue_itoa('\n');
        unicode_t uc_res;
        (void)utf8_to_unicode(ltext(curwp->w.dotp), curwp->w.doto,
             lused(curwp->w.dotp), &uc_res);
        return ue_itoa(uc_res);
    case EVDISCMD:          return ltos(discmd);
    case EVVERSION:         return VERSION;
    case EVPROGNAME:        return PROGRAM_NAME_LONG;
    case EVSEED:            return ue_itoa(seed);
    case EVDISINP:          return ltos(disinp);
    case EVWLINE:           return ue_itoa(curwp->w_ntrows);
    case EVCWLINE:          return ue_itoa(getwpos());
    case EVTARGET:          return ue_itoa(curgoal);
    case EVSEARCH:          return db_val_nc(pat);
    case EVREPLACE:         return db_val_nc(rpat);
    case EVMATCH:           return group_match(0);
    case EVKILL:            return getkill();
    case EVCMODE:           return ue_itoa(curbp->b_mode);
    case EVGMODE:           return ue_itoa(gmode);
    case EVPENDING:         return ltos(typahead());
    case EVLWIDTH:          return ue_itoa(lused(curwp->w.dotp));
    case EVLINE:            return getctext();
    case EVRVAL:            return ue_itoa(rval);
    case EVTAB:             return ue_itoa(tabmask + 1);
    case EVOVERLAP:         return ue_itoa(overlap);
    case EVSCROLLJUMP:      return ue_itoa(scrolljump);
    case EVSCROLL:          return ltos(term.t_scroll != NULL);
    case EVINMB:            return ue_itoa(inmb);
    case EVFCOL:            return(ue_itoa(curwp->w.fcol) + 1);
    case EVHJUMP:           return(ue_itoa(hjump));
    case EVHSCROLL:         return(ltos(hscroll));
    case EVYANKMODE:        switch (yank_mode) {
                            case Old:   return "old";   break;
                            case GNU:   return "gnu";   break;
                            }
                            return "";
                            break;
    case EVAUTOCLEAN:       return ue_itoa(autoclean);
    case EVREGLTEXT:        return regionlist_text;
    case EVREGLNUM:         return regionlist_number;
    case EVAUTODOS:         return ltos(autodos);
    case EVSDTKSKIP:        return ue_itoa(showdir_tokskip);
    case EVUPROCOPTS:       return ue_itoa(uproc_opts);
    case EVFORCESTAT:       return force_status;
    case EVEQUIVTYPE:
/* Can't use switch - utf8proc_NFC etc. are not constants.
 * We'll put the standard setting first...
 */
            if (equiv_handler == utf8proc_NFKC) return "NFKC";
            if (equiv_handler == utf8proc_NFC)  return "NFC";
            if (equiv_handler == utf8proc_NFD)  return "NFD";
            return "NFKD";
            break;
    case EVSRCHCANHUNT:     return ue_itoa(srch_can_hunt);
    case EVULPCOUNT:        return ue_itoa(uproc_lpcount);
    case EVULPTOTAL:        return ue_itoa(uproc_lptotal);
    case EVULPFORCED:       return ue_itoa(uproc_lpforced);
    case EVSDOPTS:          return showdir_opts;
    case EVGGROPTS:         return ue_itoa(ggr_opts);
    case EVSYSTYPE:
    case EVPROCTYPE: {
        static struct utsname tuname;
        uname(&tuname);
        return (evl[vnum].tag == EVSYSTYPE)? tuname.sysname: tuname.machine;
    }
    case EVFORCEMODEON:     return ue_itoa(force_mode_on);
    case EVFORCEMODEOFF:    return ue_itoa(force_mode_off);
    case EVPTTMODE:         if (curbp->b_mode & MDPHON)
                                return ptt->b_bname + 1;    /* Omit "/" */
                            else
                                return "";
    case EVVISMAC:          return ltos(vismac);
    case EVFILOCK:          return ltos(filock);
    case EVCRYPT:           return ue_itoa(crypt_mode);
    case EVBRKTMS: {
/* We deal in ms, so need to convert from the s + ns of timespec */

        time_t wt = (pause_time.tv_sec * 1000000000) + pause_time.tv_nsec;
        wt = wt/1000000;    /* ns -> ms */
        return ue_itoa(wt);
    }
    }

    exit(-12);              /* Again, we should never get here */
#ifdef __TINYC__
    return "";              /* Avoid "function might return no value" */
#endif
}

/* find the type of a passed token
 *
 * char *token;         token to analyze
 */
int gettyp(const char *token) {
    char c;         /* first char in token */

    if (!token || ((c = *token) == '\0')) return TKNUL; /* No blanks!!! */

/* A numeric literal?
 * *GGR* allow for -ve ones too. -n and .nnn are numbers
 */
    char cn;
    if (c == '-' || c == '.')   cn = token[1];
    else                        cn = token[0];
    if (cn >= '0' && cn <= '9') return TKLIT;

    switch (c) {
    case '"':   return TKSTR;
    case '!':   return TKDIR;
    case '@':   return TKARG;
    case '#':   return TKBUF;
    case '$':   return TKENV;
    case '%':   return TKVAR;
    case '&':   return TKFUN;
    case '*':   return TKLBL;
    case '.':   return TKBVR;
    }
    return TKCMD;
}

/* find the value of a token
 *
 * char *token;         token to evaluate
 */
static db_strdef(valres);       /* static returned val */
const char *getval(const char *token) {
    struct buffer *bp;          /* temp buffer pointer */

    switch (gettyp(token)) {
    case TKNUL:
        return "";

    case TKARG: {               /* interactive argument */

/* We allow internal uemacs code to set the response of the next TKARG
 * (this was set-up so that the showdir user-proc could be given a
 * "pre-loaded" response).
 */
        int do_fixup = (uproc_opts & UPROC_FIXUP);
        uproc_opts = 0;         /* Always reset flags after use */
        if (userproc_arg) {
            db_set(valres, userproc_arg);
        }
        else {
/* GGR - There is the possibility (actually, certainty) of an illegal
 * overlap of args here. So it must be done to a temporary buffer.
 *              strcpy(token, getval(token+1));
 */
            db_strdef(tbuf);        /* string buffer for some workings */
            db_set(tbuf, getval(token+1));
            int distmp = discmd;    /* Remember initial state */
            discmd = TRUE;
            int status = getstring(db_val(tbuf), &valres, CMPLT_NONE);
            db_free(tbuf);
            discmd = distmp;
            if (status == ABORT) return errorm;
        }
        if (do_fixup) db_set(valres, fixup_full(db_val(valres)));
        return db_val_nc(valres);
    }
    case TKBUF:                 /* buffer contents fetch */
/* Grab the right buffer */
        bp = bfind(getval(token+1), FALSE, 0);
        if (bp == NULL) return errorm;

/* If the buffer is displayed, get the window vars instead of the buffer vars */
        if (bp->b_nwnd > 0) {
            curbp->b.dotp = curwp->w.dotp;
            curbp->b.doto = curwp->w.doto;
        }

/* Make sure we are not at the end */
        if (bp->b_linep == bp->b.dotp) return errorm;

/* Grab the line as an argument - then */
        int blen = lused(bp->b.dotp) - bp->b.doto;
        db_setn(valres, ltext(bp->b.dotp) + bp->b.doto, blen);

/* And step the buffer's line ptr ahead a line */
        bp->b.dotp = bp->b.dotp->l_fp;
        bp->b.doto = 0;

/* If displayed buffer, reset window ptr vars */
        if (bp->b_nwnd > 0) {
            curwp->w.dotp = curbp->b.dotp;
            curwp->w.doto = 0;
            curwp->w_flag |= WFMOVE;
        }

/* And return the spoils */
        return db_val_nc(valres);

    case TKVAR:
        return gtusr(token + 1);
    case TKBVR:
        return gtbvr(token + 1);
    case TKENV:
        return gtenv(token + 1);
    case TKFUN:
        return gtfun(token + 1);
    case TKDIR:
        return errorm;
    case TKLBL:
        return errorm;
    case TKLIT:
        return token;
    case TKSTR:
        return token + 1;
    case TKCMD:
        return token;
    }
    return errorm;
}

/* Find a variables type and name.
 *
 * @var: name of variable to get.
 * @vd: structure to hold type and pointer.
 * @create: only add new entry for user and buffer vars if this is set
 *   (use to be @size: size of variable array., but that was always NVSIZE+1)
 */
static void findvar(const char *var, struct variable_description *vd,
     int vcreate) {
    unsigned int vnum;  /* subscript in variable arrays */
    int vtype;          /* type to return */

    vnum = -1;
fvar:
    vtype = -1;
    switch (var[0]) {

    case '$':           /* Check for legal enviromnent var */
        for (vnum = 0; vnum < ARRAY_SIZE(evl); vnum++)
            if (strcmp(var+1, evl[vnum].var) == 0) {
                vtype = TKENV;
                break;
            }
        break;

    case '%':           /* Check for existing legal user variable */
        for (vnum = 0; vnum < MAXVARS; vnum++)
            if (strcmp(var+1, uv[vnum].name) == 0) {
                vtype = TKVAR;
                break;
            }
        if (vnum < MAXVARS || !vcreate) break;
        for (vnum = 0; vnum < MAXVARS; vnum++)  /* Create a new one??? */
            if (uv[vnum].name[0] == 0) {
                vtype = TKVAR;
                strcpy(uv[vnum].name, var+1);
                break;
            }
        break;

    case '.':               /* A buffer variable - only for execbp! */
        if (!execbp) break;
        if (!execbp->bv) {  /* Need to create a set...free()d in bclear() */
            execbp->bv = Xmalloc(BVALLOC*sizeof(struct simple_variable));
            struct simple_variable *tp = execbp->bv;
            int count = BVALLOC;
            while(count--) {
                terminate_str(tp->name);    /* Makes it empty */
                tp->value = NULL;
                tp++;
            }
        }
        else {              /* ...or check whether it is there */
            for (vnum = 0; vnum < BVALLOC; vnum++) {
                if (!strcmp(var+1, execbp->bv[vnum].name)) {
                    vtype = TKBVR;
                    break;
                }
            }
            if (vnum < BVALLOC) break;
        }
        if (!vcreate) break;
        for (vnum = 0; vnum < BVALLOC; vnum++)  /* Create a new one??? */
            if (execbp->bv[vnum].name[0] == '\0') {
                vtype = TKBVR;
                strcpy(execbp->bv[vnum].name, var+1);
                break;
            }
        break;

    case '&':               /* A function to generate the name? */
        var = getval(var);
        goto fvar;
    }

/* Return the results */
    vd->v_num = vnum;
    vd->v_type = vtype;
    return;
}

/* Set a variable.
 *
 * @var: variable to set.
 * @value: value to set to.
 */
static int svar(struct variable_description *var, const char *value) {
    int vnum;       /* ordinal number of var referenced */
    int vtype;      /* type of variable to set */
    int status;     /* status return */
    int c;          /* translated character */

/* simplify the vd structure (we are gonna look at it a lot) */
    vnum = var->v_num;
    vtype = var->v_type;

/* and set the appropriate value */
    status = TRUE;
    switch (vtype) {
    case TKVAR:             /* set a user variable */
        uv[vnum].value = Xrealloc(uv[vnum].value, strlen(value) + 1);
        strcpy(uv[vnum].value, value);
        break;

    case TKBVR:             /* set a buffer variable - findvar check BTPROC */
        execbp->bv[vnum].value =
             Xrealloc(execbp->bv[vnum].value, strlen(value) + 1);
        strcpy(execbp->bv[vnum].value, value);
        break;

    case TKENV:             /* set an environment variable */
        status = TRUE;  /* by default */
        switch (vnum) {

/* All of these are read-only */
        case EVVERSION:
        case EVPROGNAME:
        case EVSEARCH:
        case EVMATCH:
        case EVKILL:
        case EVPENDING:
        case EVLWIDTH:
        case EVRVAL:
        case EVINMB:
        case EVULPCOUNT:
        case EVULPTOTAL:
        case EVULPFORCED:
        case EVSYSTYPE:
        case EVFORCEMODEON:
        case EVFORCEMODEOFF:
            status = FALSE;
            break;

        case EVFILLCOL:
            fillcol = atoi(value);
            break;
        case EVPAGELEN:
            status = newsize(atoi(value));
            break;
        case EVCURCOL:
            srch_can_hunt = 0;
            status = setccol(atoi(value) - 1);
            break;
        case EVCURLINE:
            srch_can_hunt = 0;
            status = gotoline(TRUE, atoi(value));
            break;
        case EVCURWIDTH:
            status = newwidth(atoi(value));
            break;
        case EVCBUFNAME:
            if (set_buffer_name(value))
                upmode(curbp);
            else
                status = FALSE;
            break;
        case EVCFNAME:
            set_buffer_filenames(curbp, value);
            upmode(curbp);
            break;
        case EVDEBUG:
            macbug = ue_atoi(value);
            break;
        case EVSTATUS:
            cmdstatus = stol(value);
            break;
        case EVASAVE:
            gasave = atoi(value);
            break;
        case EVACOUNT:
            gacount = atoi(value);
            break;
        case EVLASTKEY:
            lastkey = atoi(value);
            break;
        case EVCURCHAR:
            srch_can_hunt = 0;
            ldelgrapheme(1, FALSE);     /* Delete 1 char-place */
            c = atoi(value);
            if (c == '\n') lnewline();
            else           linsert_uc(1, c);
            back_grapheme(1);
            break;
        case EVDISCMD:
            discmd = stol(value);
            break;
        case EVSEED:
            seed = atoi(value);
            break;
        case EVDISINP:
            disinp = stol(value);
            break;
        case EVWLINE:
            status = resize(TRUE, atoi(value));
            break;
        case EVCWLINE:
            srch_can_hunt = 0;
            status = forwline(TRUE, atoi(value) - getwpos());
            break;
        case EVTARGET:
            curgoal = atoi(value);
            com_flag |= CFCPCN; /* Set this flag */
            break;
        case EVREPLACE:
            db_set(rpat, value);
            new_prompt(value);  /* Let getstring() know, via the search code */
            break;
        case EVCMODE:
            srch_can_hunt = 0;
            curbp->b_mode = (int)ue_atoi(value);
            upmode(curbp);
            break;
        case EVGMODE:
            gmode = (int)ue_atoi(value);
            break;
        case EVLINE:
            srch_can_hunt = 0;
/* Just replace the current line's text with this text, and put dot at 0 */
            struct line *tlp = curwp->w.dotp;
            db_set(tlp->l_, value);
            curwp->w.doto = 0;      /* Has to go somewhere */
            break;
        case EVTAB:
            tabmask = atoi(value) - 1;
/* tab can only be set to 8 or 4 */
            if (tabmask != 0x07 && tabmask != 0x03) tabmask = 0x07;
            curwp->w_flag |= WFHARD;
            break;
        case EVOVERLAP:
            overlap = atoi(value);
            break;
        case EVSCROLLJUMP:
            scrolljump = atoi(value);
            break;
        case EVSCROLL:
            if (!stol(value)) term.t_scroll = NULL;
            break;
        case EVFCOL:
            curwp->w.fcol = atoi(value) - 1;
            if (curwp->w.fcol < 0) curwp->w.fcol = 0;
            curwp->w_flag |= WFHARD | WFMODE;
            break;
        case EVHJUMP:
            hjump = atoi(value);
            if (hjump < 1) hjump = 1;
            if (hjump > term.t_ncol - 1) hjump = term.t_ncol - 1;
            break;
        case EVHSCROLL:
            hscroll = stol(value);
            lbound = 0;
/* If we have switched it off, we need to undo any exisiting visible
 * hscroll.
 * Easiest done by temporarily switching to col0, updating then switching
 * back to where we were...
 */
            if (!hscroll) {
                int sdoto = curwp->w.doto;
                curwp->w.doto = 0;
                update(TRUE);
                curwp->w.doto = sdoto;
            }
            break;
        case EVYANKMODE:
            if (strcmp("old", value) == 0)
                yank_mode = Old;
            else if (strcmp("gnu", value) == 0)
                yank_mode = GNU;
            break;      /* For anything else, leave unset */
        case EVAUTOCLEAN:
           {int old_autoclean = autoclean;
            autoclean = atoi(value);
            if (autoclean >= 0 && autoclean < old_autoclean) dumpdir_tidy();
           }
            break;
        case EVREGLTEXT:    /* These two are... */
        case EVREGLNUM:     /* ...very similar */
            if (strlen(value) >= MAX_REGL_LEN) {
                mlforce("String too long - max %d", MAX_REGL_LEN - 1);
                status = FALSE;
                break;
            }
            strcpy((vnum == EVREGLTEXT)? regionlist_text: regionlist_number,
                  value);
            break;
        case EVAUTODOS:
            autodos = stol(value);
            break;
        case EVSDTKSKIP:
            showdir_tokskip = atoi(value);
            break;
        case EVUPROCOPTS:
            uproc_opts = ue_atoi(value);
            break;
/* Set the Equiv handler. Default is utf8proc_NFKC.
 * Just in case this could usefully produce different results...
 */
        case EVEQUIVTYPE: {
            static char choices[] = "NFC NFD NFKD";
            char *which = strstr(choices, value);
            equiv_handler = utf8proc_NFKC;  /* Default */
            if (which) {
                switch(which - choices) {
                case 0:  equiv_handler = utf8proc_NFC;  break;
                case 4:  equiv_handler = utf8proc_NFD;  break;
                case 8:  equiv_handler = utf8proc_NFKD; break;
                }
            }
            break;
        }
        case EVSRCHCANHUNT:
            srch_can_hunt = atoi(value);
            break;
/* There are only 5 (MAX_SD_OPTS) options, so any attempt to set more
 * is an error.
 */
        case EVSDOPTS:
            if (strlen(value) > MAX_SD_OPTS) status = FALSE;
            else strcpy(showdir_opts, value);
            break;
        case EVGGROPTS:
            ggr_opts = ue_atoi(value);
            break;
        case EVVISMAC:
            vismac = stol(value);
            break;
        case EVFILOCK:
            filock = stol(value);
            break;
        case EVCRYPT: {
            int new_mode = ue_atoi(value);
            int fail = 0;
/* There must no unexpected bits set and a valid value in the bottom bits. */

            do {
                if ((new_mode & ~(CRYPT_VALID))) {
                    fail = 1;
                    break;
                }
                int key_fill = (new_mode & CRYPT_MODEMASK);
                switch(key_fill) {  /* Check for a valid value */
                case CRYPT_RAW:
                case CRYPT_FILL63:
                    break;
                default:
                    fail = 1;
                }
            } while(0);     /* 1-pass block */
            if (fail) {
               mlforce("0x%x is an invalid $crypt-mode setting", new_mode);
               status = FALSE;
            }
            else {
/* If we a have an encryption key set we have to handle this by
 * encrypting it under the old key (which is a decrypt, as it's symmetric)
 * and then encrypt it again under the new mode.
 * For all buffers that have a key.
 * NOTE that in order to set an encryption key you must have first set
 * crypt_mode, so if crypt_mode is still unset there is no encryption key.
 */
                if (crypt_mode != 0) {
                    int old_mode = crypt_mode;
                    for (struct buffer *bp = bheadp;
                         bp != NULL; bp = bp->b_bufp) {
                        if (bp->b_keylen > 0) {
                            crypt_mode = old_mode;
                            myencrypt(NULL, 0);
                            myencrypt(bp->b_key, bp->b_keylen);
                            crypt_mode = new_mode;
                            myencrypt(NULL, 0);
                            myencrypt(bp->b_key, bp->b_keylen);
                        }
                    }
                }
                crypt_mode = new_mode;
            }
            break;
        }
        case EVBRKTMS: {
/* We deal in ms, so need to convert to the s + ns of timespec */
            int bracket_ms = atoi(value);
            if (bracket_ms >= 1000) {
                pause_time.tv_sec = bracket_ms/1000;
                bracket_ms -= pause_time.tv_sec*1000;
            }
            else pause_time.tv_sec = 0;
            pause_time.tv_nsec = bracket_ms*1000000;
            break;
        }
        }
        break;
    }
    return status;
}

/* Set a variable
 * The command front-end to svar
 * Also used by names.c
 *
 * int f;               default flag
 * int n;               numeric arg (can override prompted value)
 */
int setvar(int f, int n) {
    int status;                     /* status return */
    struct variable_description vd; /* variable num/type */

    db_strdef(var);         /* name of variable to set */
    db_strdef(varval);      /* value to set */

/* First get the variable to set.. */
    if (clexec == FALSE) {
        status = mlreply("Variable to set: ", &var, CMPLT_VAR);
        if (status != TRUE) return status;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, &var);
   }

/* Check the legality and find the var */
    findvar(db_val(var), &vd, TRUE);

/* If its not legal....bitch */
    if (vd.v_type == -1) {
        mlwrite("%%No such variable as '%s'", db_val(var));
        status = FALSE;
        goto exit;
    }

/* Get the value for that variable */
    if (f == TRUE) db_set(varval, ue_itoa(n));
    else {
        status = mlreply("Value: ", &varval, CMPLT_NONE);
        if (status != TRUE) goto exit;
    }

/* And set the appropriate value */
    status = svar(&vd, db_val(varval));

/* If $debug & 0x01, every assignment will be reported in the minibuffer.
 *      The user then needs to press a key to continue.
 *      If that key is abortc (ctl-G) the macro is aborted
 * If $debug & 0x02, every assignment will be reported in //Debug buffer
 *
 * NOTE that a user-macro can turn this off while running.
 *      This is used by the ones which set macbug and clear //Debug.
 */
    if (macbug && !macbug_off) {    /* More likely failure first */
        db_sprintf(glb_db, "(%s:%s:%s)", ltos(status),
             db_val(var), db_val(varval));

/* Write out the debug line to //Debug? */
        if (macbug & 0x2) {
            addline_to_anyb(db_val(glb_db), bdbgp);
        }
/* Write out the debug line to the message line? */
        if (macbug & 0x1) {
            mlforce_one(db_val(glb_db));
            update(TRUE);

/* And get the keystroke */
            if (get1key() == abortc) {
                mlforce_one(MLbkt("Macro aborted"));
                status = FALSE;
            }
        }
    }

/* And return it */
exit:
    db_free(var);
    db_free(varval);
    return status;
}

/* Delete a user (%xxx) or buffer (.xxx) var.
 * These have the same structure - so we just need to know the pointer
 * of the one to delete. we just need to know how many is in the list sent
 *
 * delvar has already successfully run findvar for us
 */
static void del_simple_var(struct variable_description *vd,
     struct simple_variable *op, int listlen) {

/* We know where it is, so just move the rest of the array down 1.
 * Since there are never gaps, we can stop as soon as we get to an
 * empty variable name.
 */
    struct simple_variable *np = op+1;
    Xfree(op->value);
    for (int vnum = vd->v_num; vnum < listlen; vnum++, op++, np++) {
        strcpy(op->name, np->name);
        op->value = np->value;
        if (op->name[0] == '\0') break;     /* All done */
    }
    terminate_str(np->name);                /* Makes it empty */
    np->value = NULL;
}

/* Delete a variable
 * Can only delete user and buffer variables.
 *
 * int f;               default flag
 * int n;               numeric arg (can override prompted value)
 */
int delvar(int f, int n) {
    UNUSED(f); UNUSED(n);
    int status;                     /* status return */
    struct variable_description vd; /* variable num/type */

    db_strdef(var);         /* Variable to delete */

/* First get the variable to delete.. */
    if (clexec == FALSE) {
        status = mlreply("Variable to delete: ", &var, CMPLT_VAR);
        if (status != TRUE) return status;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, &var);
   }

/* Check the legality and find the var */
    findvar(db_val(var), &vd, FALSE);

/* Delete by type, or complain about the type */
    switch(vd.v_type) {
    case TKVAR:
        del_simple_var(&vd, uv+(vd.v_num), MAXVARS);
        status = TRUE;
        goto exit;
    case TKBVR:
        del_simple_var(&vd, &(execbp->bv[vd.v_num]), BVALLOC);
        status = TRUE;
        goto exit;
    case -1:
        mlwrite("No such variable as '%s'", db_val(var));
        break;
    default:
        mlwrite("Cannot delete '%s' (not a user or buffer variable)",
             db_val(var));
        break;
    }
    status = FALSE;
exit:
    db_free(var);
    return status;
}

#ifdef DO_FREE
/* Add a call to allow free() of normally-unfreed items here for, e.g,
 * valgrind usage.
 */
void free_eval(void) {
/* Just the variable indexes.
 * Buffer vars are freed in free_buffer.
 */
    Xfree(envvar_index);
    Xfree(next_envvar_index);
    for (int i = 0; i < MAXVARS; i++) {
        if (uv[i].name[0] == '\0') break;   /* End of list */
        Xfree(uv[i].value);
    }
    db_free(kvalue);
    db_free(xlres);
    db_free(pttres);
    db_free(ue_buf);
    db_free(funres);
    db_free(valres);
    return;
}
#endif
