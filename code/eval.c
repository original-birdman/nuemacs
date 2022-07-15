/*      eval.c
 *
 *      Expression evaluation functions
 *
 *      written 1986 by Daniel Lawrence
 *      modified by Petri Kutvonen
 */

#include <stdio.h>

#include "estruct.h"
#include "edef.h"
#include "efunc.h"
#include "evar.h"
#include "line.h"
#include "util.h"
#include "version.h"

#include <stddef.h>
#include "idxsorter.h"

/* Return some of the contents of the kill buffer
 */
static char *getkill(void) {
    int size;                       /* max number of chars to return */
    static char value[NSTRING];     /* fixed buffer for value */

    if (kbufh[0] == NULL)
                /* no kill buffer....just a null string */
        value[0] = 0;
    else {      /* copy in the contents...allow for a trailing NUL */
        if (kused[0] < NSTRING) size = kused[0];
        else                    size = NSTRING - 1;
        memcpy(value, kbufh[0]->d_chunk, size);
        *(value+size) = '\0';
    }
    return value;       /* Return the constructed value */
}

/* A user-settaable environment variable allowing the user to define
 * the initial sorting for a directory display.
 * The user may give up to 5 settings, 1-char for each.
 */
#define MAX_SD_OPTS 5
static char showdir_opts[MAX_SD_OPTS+1] = "";

/* User variables. External as used by completion code in input.c */

#define MAXVARS 64

struct user_variable uv[MAXVARS + 1];

/* Initialize the user variable list. */
void varinit(void) {
    int i;
    for (i = 0; i < MAXVARS; i++) {
        uv[i].u_name[0] = 0;
        uv[i].u_value = NULL;
    }
}

/* SORT ROUTINES
 * Some parts are external for use by completion code in input.c
 */

int *envvar_index = NULL;
int *next_envvar_index;
static int evl_size = sizeof(evl)/sizeof(struct evlist);

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

char *uvnames[MAXVARS + 1];
static int n_uvn;

void sort_user_var(void) {
    n_uvn = 0;
    for (int i = 0; i < MAXVARS; i++) {

/* There is currently no mechanism for removing a user variable
 * once it has been created, so as soon as we reach an empty entry
 * we've found all active ones.
 * If a "del_user_var" functionality ever existed the break below would
 * (probably) need to be a continue.
 */
        if (uv[i].u_name[0] == '\0') break;

/* Need to add this one into uvnames in alphabetic order and push any
 * followers down.
 */
        char *toadd = uv[i].u_name;
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
int stol(char *val) {
/* check for logical values */
    if (val[0] == 'F') return FALSE;
    if (val[0] == 'T') return TRUE;

/* check for numeric truth (!= 0) */
    return (atoi(val) != 0);
}

/* Numeric logical to string logical
 *
 * int val;             value to translate
 */
static char *ltos(int val) {
    if (val) return truem;
    else     return falsem;
}

/* Make a string lower case
 * Internal to this routine - only needs to handle ASCII as it is only
 * used for the function names defined for this code.
 *
 * char *str;           string to lower case
 */
static char *mklower(char *str) {
    char *sp;

    sp = str;
    while (*sp) {
        if ('A' <= *sp && *sp <= 'Z') *sp += 'a' - 'A';
        ++sp;
    }
    return str;
}

/* Returns a random integer
 */
static int ernd(void) {
    seed = abs(seed * 1721 + 10007);
    return seed;
}

/* Find pattern within source
 *
 * char *source;        source string to search
 * char *pattern;       string to look for
 */
static int sindex(char *source, char *pattern) {
    char *sp;               /* ptr to current position to scan */
    char *csp;              /* ptr to source string during comparison */
    char *cp;               /* ptr to place to check for equality */

/* Scanning through the source string. Assumes ASCII.... */
    sp = source;
    while (*sp) {           /* Scan through the pattern */
        cp = pattern;
        csp = sp;
        while (*cp) {
            if (!asceq(*cp, *csp)) break;
            ++cp;
            ++csp;
        }

/* Was it a match? */
        if (*cp == 0) return (int) (sp - source) + 1;
        ++sp;
    }

/* No match at all.. */
    return 0;
}

/* Filter a string through a translation table
 *
 * char *source;        string to filter
 * char *lookup;        characters to translate
 * char *trans;         resulting translated characters
 */
static char *xlat(char *source, char *lookup, char *trans) {
    char *sp;       /* pointer into source table */
    char *lp;       /* pointer into lookup table */
    char *rp;       /* pointer into result */
    static char result[NSTRING];    /* temporary result */

/* Scan source string */
    sp = source;
    rp = result;
    while (*sp) {   /* scan lookup table for a match */
        lp = lookup;
        while (*lp) {
            if (*sp == *lp) {
                *rp++ = trans[lp - lookup];
                goto xnext;
            }
            ++lp;
        }
/* No match, copy in the source char untranslated */
        *rp++ = *sp;
        xnext:++sp;
    }

/* Terminate and return the result */
    *rp = 0;
    return result;
}

/* ue_printf
 *
 * printf-like handling for multiple args together...
 * Since all values are strings, and trying to start a template with %
 * produces an error (it's taken to be the start of a user variable).
 * The place-holder is %ws with the w being a stated width (may be omitted).
 *
 * NOTE!!!!
 * This *might* require the use of a buffer-per-call, but
 *    write-message &cat &ptf "fc: %s " $fillcol &ptf "et: %s" $equiv_type
 * works, so perhaps not...
 */

/* The returned value - overflow NOT checked.... */
static char ue_buf[NSTRING];

#define PHCHAR '%'
#define TMCHAR 's'
static char *ue_printf(char *fmt) {
    unicode_t c;
    char nexttok[NSTRING];

    char *op = ue_buf;      /* Output pointer */
/* GGR - loop through the bytes getting any utf8 sequence as unicode */
    int bytes_togo = strlen(fmt);
    if (bytes_togo == 0) goto finalize; /* Nothing else...clear line only */

    while (bytes_togo > 0) {
        int used = utf8_to_unicode((char *)fmt, 0, bytes_togo, &c);
        bytes_togo -= used;
        if (c != PHCHAR) {      /* So copy in the *bytes*! */
            memcpy(op, fmt, used);
            op += used;
            fmt += used;
            continue;
        }
        fmt += used;

/* We have a PHCHAR, so get the next char */

        if (*(fmt) == PHCHAR) { /* %% is a literal '%' */
            *op++ = PHCHAR;
            fmt++;
            continue;
        }
        int min_width = 0;
        while ((*fmt >= '0') && (*fmt <= '9')) {
            min_width *= 10;
            min_width += (*fmt - '0');
            fmt++;
            bytes_togo--;
        }
/* The next char MUST be a TMCHAR - anything else is an error! */
        if (*fmt != TMCHAR) {
            strcpy(op, errorm);
            goto finalize;
        }
        fmt++;                  /* Skip the TMCHAR */
        bytes_togo--;
        if (macarg(nexttok) != TRUE) break;
        int slen = sprintf(op, "%*s", min_width, nexttok);
        op +=slen;
    }

/* Finalize the result with a NUL char */

finalize:
    *op = '\0';
    return ue_buf;
}

/* ue_atoi
 * This allows the integer to be expressed in hex ("0x"), octal (0..)
 * or decimal.
 */
static int ue_atoi(char *ustr) {
    return (int)strtol(ustr, NULL, 0);
}

/* Evaluate a function.
 *
 * @fname: name of function to evaluate.
 */
static char *gtfun(char *fname) {
    unsigned int fnum;      /* index to function to eval */
    int status;             /* return status */
    char *tsp;              /* temporary string pointer */
    char arg1[NSTRING];     /* value of first argument */
    char arg2[NSTRING];     /* value of second argument */
    char arg3[NSTRING];     /* value of third argument */
    static char result[2 * NSTRING];        /* string result */
    int nb;                 /* Number of bytes in string */
    struct mstr csinfo;     /* Casing info structure */

/* Look the function up in the function table */
    fname[3] = 0;           /* only first 3 chars significant */
    mklower(fname);         /* and let it be upper or lower case */
    for (fnum = 0; fnum < ARRAY_SIZE(funcs); fnum++)
        if (strcmp(fname, funcs[fnum].f_name) == 0) break;

/* Return errorm on a bad reference */
    if (fnum == ARRAY_SIZE(funcs)) return errorm;

/* If needed, retrieve the first argument */
    if (funcs[fnum].f_type >= MONAMIC) {
        if ((status = macarg(arg1)) != TRUE) return errorm;

/* If needed, retrieve the second argument */
        if (funcs[fnum].f_type >= DINAMIC) {
            if ((status = macarg(arg2)) != TRUE) return errorm;

/* If needed, retrieve the third argument */
            if (funcs[fnum].f_type >= TRINAMIC)
                if ((status = macarg(arg3)) != TRUE) return errorm;
        }
    }


/* And now evaluate it! */
    switch (funcs[fnum].tag) {
    case UFADD:         return ue_itoa(atoi(arg1) + atoi(arg2));
    case UFSUB:         return ue_itoa(atoi(arg1) - atoi(arg2));
    case UFTIMES:       return ue_itoa(atoi(arg1) * atoi(arg2));
    case UFDIV:         return ue_itoa(atoi(arg1) / atoi(arg2));
    case UFMOD:         return ue_itoa(atoi(arg1) % atoi(arg2));
    case UFNEG:         return ue_itoa(-atoi(arg1));
    case UFCAT:
        strcpy(result, arg1);
        return strcat(result, arg2);
    case UFLEFT:
                       {int reslen = atoi(arg2);
                        strncpy(result, arg1, reslen);
                        result[reslen] = '\0';
                        return result;
                       }
    case UFRIGHT:
        return (strcpy(result, &arg1[(strlen(arg1) - atoi(arg2))]));
    case UFMID:
                       {int reslen = atoi(arg3);
                        strncpy(result, &arg1[atoi(arg2) - 1], atoi(arg3));
                        result[reslen] = '\0';
                        return result;
                       }
    case UFNOT:         return ltos(stol(arg1) == FALSE);
    case UFEQUAL:       return ltos(atoi(arg1) == atoi(arg2));
    case UFLESS:        return ltos(atoi(arg1) < atoi(arg2));
    case UFGREATER:     return ltos(atoi(arg1) > atoi(arg2));
    case UFSEQUAL:      return ltos(strcmp(arg1, arg2) == 0);
    case UFSLESS:       return ltos(strcmp(arg1, arg2) < 0);
    case UFSGREAT:      return ltos(strcmp(arg1, arg2) > 0);
    case UFIND:         return strcpy(result, getval(arg1));
    case UFAND:         return ltos(stol(arg1) && stol(arg2));
    case UFOR:          return ltos(stol(arg1) || stol(arg2));
    case UFLENGTH:      return ue_itoa(strlen(arg1));
    case UFUPPER:
    case UFLOWER:
        utf8_recase(funcs[fnum].tag == UFUPPER? UTF8_UPPER: UTF8_LOWER,
             arg1, -1, &csinfo);
        strcpy(result, csinfo.str);
        Xfree(csinfo.str);
        return(result);
    case UFESCAPE:              /* Only need to escape ASCII chars */
       {char *ip = arg1;        /* This is SHELL escaping... */
        char *op = result;
        while (*ip) {
            if (*ip == '\t') {  /* tab - word separator */
                *op++ = '\\';
                *op++ = 't';
                ip++;
                continue;
            }
            if (*ip == '\n') {  /* newline - word separator */
                *op++ = '\\';
                *op++ = 'n';
                ip++;
                continue;
            }
            switch(*ip) {       /* Check for the chars we need to escape */
            case ' ':           /* Spaces */
            case '"':           /* Double quote */
            case '$':           /* Environment vars */
            case '&':           /* Backgrounding */
            case '\'':          /* Single quote */
            case '(':           /* Word separator */
            case ')':           /* Word separator */
            case '*':           /* multi-Wildcard */
            case ';':           /* Command separator */
            case '<':           /* Input redirection */
            case '>':           /* Output redirection */
            case '?':           /* single-Wildcard */
            case '\\':          /* Escaper itself */
            case '`':           /* Command interpolation */
            case '{':           /* Brace expansion */
            case '|':           /* Pipeline */
                *op++ = '\\';   /* Escape it */
            }
            *op++ = *ip++;
        }
        *op = '\0';
        return result;
       }
    case UFTRUTH:       return ltos(atoi(arg1) == 42);
    case UFASCII: {     /* Return base unicode char - but keep name... */
        struct grapheme gc;
        (void)build_next_grapheme(arg1, 0, -1, &gc);
        if (gc.ex) Xfree(gc.ex);
        return ue_itoa(gc.uc);
    }
/* Allow for unicode as:
 *      decimal codepoint
 *      hex codepoint   (0x...)
 *      U+hex           [0x must be chars 1 and 2]
 */
    case UFCHR:
        if (arg1[0] == 'U' && arg1[1] == '+') {
            static char targ[20] = "0x";    /* Fudge to 0x instead */
            strcpy(targ+2, arg1+2);         /* strtol then handles it */
            tsp = targ;
        }
        else {
            tsp = arg1;
        }
        nb = unicode_to_utf8(strtol(tsp, NULL, 0), result);
        result[nb] = 0;
        return result;
    case UFGTKEY:       /* Allow for unicode input. -> utf-8 */
        nb = unicode_to_utf8(tgetc(), result);
        result[nb] = 0;
        return result;
    case UFRND:         return ue_itoa((ernd() % abs(atoi(arg1))) + 1);
    case UFABS:         return ue_itoa(abs(atoi(arg1)));
    case UFSINDEX:      return ue_itoa(sindex(arg1, arg2));
    case UFENV:
#if     ENVFUNC
        tsp = getenv(arg1);
        return tsp == NULL ? "" : tsp;
#else
        return "";
#endif
    case UFBIND:        return transbind(arg1);
    case UFEXIST:       return ltos(fexist(arg1));
    case UFBXIST:       return ltos(bfind(arg1, 0, 0) != NULL);
    case UFFIND:
        tsp = flook(arg1, TRUE, ONPATH);
        return tsp == NULL ? "" : tsp;
    case UFBAND:        return ue_itoa(ue_atoi(arg1) & ue_atoi(arg2));
    case UFBOR:         return ue_itoa(ue_atoi(arg1) | ue_atoi(arg2));
    case UFBXOR:        return ue_itoa(ue_atoi(arg1) ^ ue_atoi(arg2));
    case UFBNOT:        return ue_itoa(~ue_atoi(arg1));
    case UFBLIT:        return ue_itoa(ue_atoi(arg1));
    case UFXLATE:       return xlat(arg1, arg2, arg3);
    case UFGRPTEXT:
        return group_match(atoi(arg1));
    case UFPRINTF:
        return ue_printf(arg1);
    }

    exit(-11);              /* never should get here */
}

/* Look up a user var's value
 *
 * char *vname;                 name of user variable to fetch
 */
static char *gtusr(char *vname) {
    int vnum;       /* Ordinal number of user var */

/* Scan the list looking for the user var name */
    for (vnum = 0; vnum < MAXVARS; vnum++) {
        if (uv[vnum].u_name[0] == 0) return errorm;
/* If a user var is being used in the same statement as it is being set
 *      set %test &add %test 1
 * then we can up with the name existing, but no value set...
 * We must check for this to avoid a crash!
 */
        if (strcmp(vname, uv[vnum].u_name) == 0)
             return uv[vnum].u_value? uv[vnum].u_value: errorm;
    }

/* Return errorm if we run off the end */
    return errorm;
}

/* Look up a buffer var's value
 *
 * char *vname;                 name of user variable to fetch
 */
static char *gtbvr(char *vname) {
    int vnum;       /* Ordinal number of user var */

    if (!execbp || !execbp->bv) return errorm;
/* Scan the list looking for the user var name */
    struct buffer_variable *tp = execbp->bv;
    for (vnum = 0; vnum < BVALLOC; vnum++, tp++) {
        if (!strcmp(vname, tp->name)) return tp->value? tp->value: errorm;
    }

/* Return errorm if we run off the end */
    return errorm;
}

/*
 * gtenv()
 *
 * char *vname;                 name of environment variable to retrieve
 */
static char *gtenv(char *vname) {
    unsigned int vnum;  /* ordinal number of var referenced */

/* Scan the list, looking for the referenced name */
    for (vnum = 0; vnum < ARRAY_SIZE(evl); vnum++)
        if (strcmp(vname, evl[vnum].var) == 0) break;

/* Return errorm on a bad reference */
    if (vnum == ARRAY_SIZE(evl))
#if     ENVFUNC
    {
        char *ename = getenv(vname);
        if (ename != NULL)
            return ename;
        else
            return errorm;
    }
#else
    return errorm;
#endif

/* Otherwise, fetch the appropriate value */
    switch (evl[vnum].tag) {
    case EVFILLCOL:         return ue_itoa(fillcol);
    case EVPAGELEN:         return ue_itoa(term.t_nrow + 1);
    case EVCURCOL:          return ue_itoa(getccol());
    case EVCURLINE:         return ue_itoa(getcline());
    case EVFLICKER:         return ltos(flickcode);
    case EVCURWIDTH:        return ue_itoa(term.t_ncol);
    case EVCBUFNAME:        return curbp->b_bname;
    case EVCFNAME:          return curbp->b_fname;
    case EVSRES:            return sres;
    case EVDEBUG:           return ltos(macbug);
    case EVSTATUS:          return ltos(cmdstatus);
    case EVPALETTE:         return palstr;
    case EVASAVE:           return ue_itoa(gasave);
    case EVACOUNT:          return ue_itoa(gacount);
    case EVLASTKEY:         return ue_itoa(lastkey);
    case EVCURCHAR:     /* Make this return the current Unicode char */
        if (curwp->w.dotp->l_used == curwp->w.doto) return ue_itoa('\n');
        struct grapheme gc;
        (void)build_next_grapheme(curwp->w.dotp->l_text,
             curwp->w.doto, curwp->w.dotp->l_used, &gc);
        if (gc.ex) Xfree(gc.ex);
        return ue_itoa(gc.uc);
    case EVDISCMD:          return ltos(discmd);
    case EVVERSION:         return VERSION;
    case EVPROGNAME:        return PROGRAM_NAME_LONG;
    case EVSEED:            return ue_itoa(seed);
    case EVDISINP:          return ltos(disinp);
    case EVWLINE:           return ue_itoa(curwp->w_ntrows);
    case EVCWLINE:          return ue_itoa(getwpos());
    case EVTARGET:
        saveflag = lastflag;
        return ue_itoa(curgoal);
    case EVSEARCH:          return pat;
    case EVREPLACE:         return rpat;
    case EVMATCH:           return group_match(0);
    case EVKILL:            return getkill();
    case EVCMODE:           return ue_itoa(curbp->b_mode);
    case EVGMODE:           return ue_itoa(gmode);
    case EVTPAUSE:          return ue_itoa(term.t_pause);
    case EVPENDING:         return ltos(typahead());
    case EVLWIDTH:          return ue_itoa(llength(curwp->w.dotp));
    case EVLINE:            return getctext();
    case EVGFLAGS:          return ue_itoa(gflags);
    case EVRVAL:            return ue_itoa(rval);
    case EVTAB:             return ue_itoa(tabmask + 1);
    case EVOVERLAP:         return ue_itoa(overlap);
    case EVSCROLLJUMP:      return ue_itoa(scrolljump);
    case EVSCROLL:          return ltos(term.t_scroll != NULL);
    case EVINMB:            return ue_itoa(inmb);
    case EVFCOL:            return(ue_itoa(curwp->w.fcol));
    case EVHSCROLL:         return(ltos(hscroll));
    case EVHJUMP:           return(ue_itoa(hjump));
    case EVYANKMODE:        switch (yank_mode) {
                            case Old:
                                return "old";
                                break;
                            case GNU:
                                return "gnu";
                                break;
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
            if (equiv_handler == utf8proc_NFC)  return "NFC";
            if (equiv_handler == utf8proc_NFD)  return "NFD";
            if (equiv_handler == utf8proc_NFKD) return "NFKD";
            else                                return "NFKC";
            break;
    case EVSRCHCANHUNT:     return ue_itoa(srch_can_hunt);
    case EVSRCHOLAP:        return ue_itoa(srch_overlap);
    case EVUPLCOUNT:        return ue_itoa(uproc_lpcount);
    case EVUPLTOTAL:        return ue_itoa(uproc_lptotal);
    case EVSDOPTS:          return showdir_opts;
    case EVGGROPTS:         return ue_itoa(ggr_opts);
    }

    exit(-12);              /* again, we should never get here */
}

/* Find a variables type and name.
 *
 * @var: name of variable to get.
 * @vd: structure to hold type and pointer.
 * @create: only add new entry for user and buffer vars if this is set
 *   (use to be @size: size of variable array., but that was always NVSIZE+1)
 */
static void findvar(char *var, struct variable_description *vd, int vcreate) {
    unsigned int vnum;  /* subscript in variable arrays */
    int vtype;          /* type to return */

    vnum = -1;
fvar:
    vtype = -1;
    switch (var[0]) {

    case '$':               /* Check for legal enviromnent var */
        for (vnum = 0; vnum < ARRAY_SIZE(evl); vnum++)
            if (strcmp(var+1, evl[vnum].var) == 0) {
                vtype = TKENV;
                break;
            }
        break;

    case '%':               /* Check for existing legal user variable */
        for (vnum = 0; vnum < MAXVARS; vnum++)
            if (strcmp(var+1, uv[vnum].u_name) == 0) {
                vtype = TKVAR;
                break;
            }
        if (vnum < MAXVARS || !vcreate) break;
        for (vnum = 0; vnum < MAXVARS; vnum++)  /* Create a new one??? */
            if (uv[vnum].u_name[0] == 0) {
                vtype = TKVAR;
                strcpy(uv[vnum].u_name, var+1);
                break;
            }
        break;

    case '.':               /* A buffer variable - only for execbp! */
        if (!execbp) break;
        if (!execbp->bv) {  /* Need to create a set...free()d in bclear() */
            execbp->bv = Xmalloc(BVALLOC*sizeof(struct buffer_variable));
            struct buffer_variable *tp = execbp->bv;
            int count = BVALLOC;
            while(count--) {
                tp->name[0] = '\0';
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

    case '&':               /* Indirect operator? */
        var[4] = 0;
        if (strcmp(var+1, "ind") == 0) {  /* Grab token, and eval it */
            execstr = token(execstr, var, NVSIZE+1);
            strcpy(var, getval(var));
            goto fvar;
        }
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
static int svar(struct variable_description *var, char *value) {
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
        uv[vnum].u_value = Xrealloc(uv[vnum].u_value, strlen(value) + 1);
        strcpy(uv[vnum].u_value, value);
        break;

    case TKBVR:             /* set a buffer variable - findvar check BTPROC */
        execbp->bv[vnum].value =
             Xrealloc(execbp->bv[vnum].value, strlen(value) + 1);
        strcpy(execbp->bv[vnum].value, value);
        break;

    case TKENV:             /* set an environment variable */
        status = TRUE;  /* by default */
        switch (vnum) {
        case EVFILLCOL:
            fillcol = atoi(value);
            break;
        case EVPAGELEN:
            status = newsize(TRUE, atoi(value));
            break;
        case EVCURCOL:
            srch_can_hunt = 0;
            status = setccol(atoi(value));
            break;
        case EVCURLINE:
            srch_can_hunt = 0;
            status = gotoline(TRUE, atoi(value));
            break;
        case EVFLICKER:
            flickcode = stol(value);
            break;
        case EVCURWIDTH:
            status = newwidth(TRUE, atoi(value));
            break;
        case EVCBUFNAME:
            strcpy(curbp->b_bname, value);
            curwp->w_flag |= WFMODE;
            break;
        case EVCFNAME:
            strcpy(curbp->b_fname, value);
            curwp->w_flag |= WFMODE;
            break;
        case EVSRES:
            status = TTrez(value);
            break;
        case EVDEBUG:
            macbug = stol(value);
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
            ldelgrapheme(1, FALSE);     /* delete 1 char-place */
            c = ue_atoi(value);
            if (c == '\n') lnewline();
            else           linsert_uc(1, c);
            back_grapheme(1);
            break;
        case EVDISCMD:
            discmd = stol(value);
            break;
        case EVVERSION:
            break;
        case EVPROGNAME:
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
            thisflag = saveflag;
            break;
        case EVREPLACE:
            strcpy(rpat, value);
            new_prompt(value);  /* Let gestring() know, via the search code */
            break;
        case EVMATCH:
            break;
        case EVKILL:
            break;
        case EVCMODE:
            srch_can_hunt = 0;
            curbp->b_mode = ue_atoi(value);
            curwp->w_flag |= WFMODE;
            break;
        case EVGMODE:
            gmode = ue_atoi(value);
            break;
        case EVTPAUSE:
            term.t_pause = atoi(value);
            break;
        case EVPENDING:
            break;
        case EVLWIDTH:
            break;
        case EVLINE:
            srch_can_hunt = 0;
            putctext(value);
            break;
        case EVGFLAGS:
            gflags = ue_atoi(value);
            break;
        case EVRVAL:
            break;
        case EVTAB:
            tabmask = atoi(value) - 1;
            if (tabmask != 0x07 && tabmask != 0x03)
            tabmask = 0x07;
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
        case EVINMB:
            break;
        case EVFCOL:
            curwp->w.fcol = atoi(value);
            if (curwp->w.fcol < 0) curwp->w.fcol = 0;
            curwp->w_flag |= WFHARD | WFMODE;
            break;
        case EVHSCROLL:
            hscroll = stol(value);
            lbound = 0;
            break;
        case EVHJUMP:
            hjump = atoi(value);
            if (hjump < 1) hjump = 1;
            if (hjump > term.t_ncol - 1) hjump = term.t_ncol - 1;
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
            uproc_opts = strtol(value, NULL, 0);    /* Allow hex input */
            break;
/* Set the Equiv handler. Default is utf8proc_NFKC.
 * Just in case this could usefully produce different results...
 */
        case EVEQUIVTYPE:
            if (!strcasecmp("NFC", value))       equiv_handler = utf8proc_NFC;
            else if (!strcasecmp("NFD", value))  equiv_handler = utf8proc_NFD;
            else if (!strcasecmp("NFKD", value)) equiv_handler = utf8proc_NFKD;
            else                                 equiv_handler = utf8proc_NFKC;
            break;
        case EVSRCHCANHUNT:
            srch_can_hunt = atoi(value);
            break;
        case EVSRCHOLAP:
            srch_overlap = atoi(value);
            break;
        case EVUPLCOUNT:        /* Both read-only */
        case EVUPLTOTAL:
            status = FALSE;
            break;

/* There are only 5 (MAX_SD_OPTS) options, so any attempt to set more
 * is an error.
 */
        case EVSDOPTS:
            if (strlen(value) > MAX_SD_OPTS) status = FALSE;
            else            strcpy(showdir_opts, value);
            break;
        case EVGGROPTS:
            ggr_opts = ue_atoi(value);
            break;
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
    char var[NVSIZE + 1];           /* name of variable to fetch */
    char value[NSTRING];            /* value to set variable to */

/* First get the variable to set.. */
    if (clexec == FALSE) {
        status = mlreply("Variable to set: ", var, NVSIZE, CMPLT_VAR);
        if (status != TRUE) return status;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, var, NVSIZE + 1);
   }

/* Check the legality and find the var */
    findvar(var, &vd, TRUE);

/* If its not legal....bitch */
    if (vd.v_type == -1) {
        mlwrite("%%No such variable as '%s'", var);
        return FALSE;
    }

/* Get the value for that variable */
    if (f == TRUE) strcpy(value, ue_itoa(n));
    else {
        status = mlreply("Value: ", value, NSTRING, CMPLT_NONE);
        if (status != TRUE) return status;
    }

/* And set the appropriate value */
    status = svar(&vd, value);

#if DEBUGM
/* If $debug == TRUE, every assignment will echo a statement to
 * that effect here.
 */
    if (macbug) {
        strcpy(outline, "(((");

/* Assignment status */
        strcat(outline, ltos(status));
        strcat(outline, ":");

/* Variable name */
        strcat(outline, var);
        strcat(outline, ":");

/* And lastly the value we tried to assign */
        strcat(outline, value);
        strcat(outline, ")))");

/* Write out the debug line */
        mlforce(outline);
        update(TRUE);

/* And get the keystroke to hold the output */
        if (get1key() == abortc) {
            mlforce(MLbkt("Macro aborted"));
            status = FALSE;
        }
    }
#endif

/* And return it */
    return status;
}

/* Delete a user var.
 * delvar has already successfully run findvar for us
 */
static void delusr(struct variable_description *vd) {

/* We know where it is, so just move the rest of the array down 1.
 * Since there are never gaps, we can stop as soon as we get to an
 * empty variable name.
 */
    struct user_variable *op = uv+(vd->v_num);
    struct user_variable *np = op+1;
    Xfree(op->u_value);
    for (int vnum = vd->v_num; vnum < BVALLOC; vnum++, op++, np++) {
        strcpy(op->u_name, np->u_name);
        op->u_value = np->u_value;
        if (op->u_name[0] == '\0') break;     /* All done */
    }
    np->u_name[0] = '\0';
    np->u_value = NULL;
}

/* Delete a buffer var.
 * delvar has already successfully run findvar for us
 */
static void delbvr(struct variable_description *vd) {

/* We know where it is, so just move the rest of the array down 1.
 * Since there are never gaps, we can stop as soon as we get to an
 * empty variable name.
 */
    struct buffer_variable *op = (execbp+vd->v_num)->bv;
    struct buffer_variable *np = op+1;
    Xfree(op->value);
    for (int vnum = vd->v_num; vnum < BVALLOC; vnum++, op++, np++) {
        strcpy(op->name, np->name);
        op->value = np->value;
        if (op->name[0] == '\0') break;     /* All done */
    }
    np->name[0] = '\0';
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
    char var[NVSIZE + 1];           /* name of variable to fetch */

/* First get the variable to delete.. */
    if (clexec == FALSE) {
        status = mlreply("Variable to delete: ", var, NVSIZE, CMPLT_VAR);
        if (status != TRUE) return status;
    }
    else {      /* macro line argument - grab token and skip it */
        execstr = token(execstr, var, NVSIZE + 1);
   }

/* Check the legality and find the var */
    findvar(var, &vd, FALSE);

/* Delete by type, or complain about the type */
    switch(vd.v_type) {
    case TKVAR:
        delusr(&vd);
        return TRUE;
    case TKBVR:
        delbvr(&vd);
        return TRUE;
    case -1:
        mlwrite("No such variable as '%s'", var);
        break;
    default:
        mlwrite("Cannot delete '%s' (not a user or buffer variable)", var);
        break;
    }
    return FALSE;
}

/*
 * ue_itoa:
 *      integer to ascii string.......... This is too
 *      inconsistent to use the system's
 *
 * int i;               integer to translate to a string
 */
char *ue_itoa(int i) {
    int digit;      /* current digit being used */
    char *sp;       /* pointer into result */
    int sign;       /* sign of resulting number */
    static char result[INTWIDTH + 1];       /* resulting string */

/* Record the sign... */
    sign = 1;
    if (i < 0) {
        sign = -1;
        i = -i;
    }

/* And build the string (backwards!) */
    sp = result + INTWIDTH;
    *sp = 0;
    do {
        digit = i % 10;
        *(--sp) = '0' + digit;  /* And install the new digit */
        i = i / 10;
    } while (i);

/* And fix the sign */
    if (sign == -1) {
        *(--sp) = '-';          /* And install the minus sign */
    }

    return sp;
}

/*
 * find the type of a passed token
 *
 * char *token;         token to analyze
 */
int gettyp(char *token) {
    char c;         /* first char in token */

    c = *token;     /* grab the first char (all we check) */

    if (c == 0) return TKNUL;      /* no blanks!!! */

/* A numeric literal? *GGR* allow for -ve ones too */

    if (c >= '0' && c <= '9') return TKLIT;
    if (c == '-') {
        char c2 = token[1];
        if (c2 >= '0' && c2 <= '9') return TKLIT;
    }

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
    default:    return TKCMD;
    }
}

/*
 * find the value of a token
 *
 * char *token;         token to evaluate
 */
char *getval(char *token) {
    struct buffer *bp;          /* temp buffer pointer */
    int blen;                   /* length of buffer argument */
    static char buf[NSTRING];   /* string buffer for some returns */
    char tbuf[NSTRING];         /* string buffer for some workings */

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
            strcpy(buf, userproc_arg);
        }
        else {
/* GGR - There is the possibility (actually, certainty) of an illegal
 * overlap of args here. So it must be done to a temporary buffer.
 *              strcpy(token, getval(token+1));
 */
            strcpy(tbuf, getval(token+1));
            int distmp = discmd;    /* Remember initial state */
            discmd = TRUE;
            int status = getstring(tbuf, buf, NSTRING, CMPLT_NONE);
            discmd = distmp;
            if (status == ABORT) return errorm;
        }
        if (do_fixup) fixup_full(buf);
        return buf;
    }
    case TKBUF:                 /* buffer contents fetch */
/* Grab the right buffer
 * GGR - There is the possibility of an illegal overlap of args here.
 *       So it must be done via a temporary buffer.
 *              strcpy(token, getval(token+1));
 */
        strcpy(tbuf, getval(token+1));
        bp = bfind(tbuf, FALSE, 0);
        if (bp == NULL) return errorm;

/* If the buffer is displayed, get the window vars instead of the buffer vars */
        if (bp->b_nwnd > 0) {
            curbp->b.dotp = curwp->w.dotp;
            curbp->b.doto = curwp->w.doto;
        }

/* Make sure we are not at the end */
        if (bp->b_linep == bp->b.dotp) return errorm;

/* Grab the line as an argument */
        blen = bp->b.dotp->l_used - bp->b.doto;
        if (blen >= NSTRING)        /* GGR >= to allow for NUL */
            blen = NSTRING - 1;
        memcpy(buf, bp->b.dotp->l_text + bp->b.doto, blen);
        buf[blen] = 0;

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
        return buf;

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
