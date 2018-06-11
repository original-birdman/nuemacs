/*      EVAR.H
 *
 *      Environment and user variable definitions
 *
 *      written 1986 by Daniel Lawrence
 *      modified by Petri Kutvonen
 */
#ifndef EVAR_H_
#define EVAR_H_

/* Max #chars in a var name. */
#define NVSIZE  32

/* Structure to hold user variables and their definitions. */
struct user_variable {
        char u_name[NVSIZE + 1]; /* name of user variable */
        char *u_value;           /* value (string) */
};

/* The tags for environment variable - used in struct evlist */
enum ev_val {
    EVFILLCOL,  EVPAGELEN,  EVCURCOL,   EVCURLINE,  EVRAM,
    EVFLICKER,  EVCURWIDTH, EVCBUFNAME, EVCFNAME,   EVSRES,
    EVDEBUG,    EVSTATUS,   EVPALETTE,  EVASAVE,    EVACOUNT,
    EVLASTKEY,  EVCURCHAR,  EVDISCMD,   EVVERSION,  EVPROGNAME,
    EVSEED,     EVDISINP,   EVWLINE,    EVCWLINE,   EVTARGET,
    EVSEARCH,   EVREPLACE,  EVMATCH,    EVKILL,     EVCMODE,
    EVGMODE,    EVTPAUSE,   EVPENDING,  EVLWIDTH,   EVLINE,
    EVGFLAGS,   EVRVAL,     EVTAB,      EVOVERLAP,  EVSCROLLJUMP,
    EVSCROLL,   EVINMB,     EVFCOL,     EVHJUMP,    EVHSCROLL,
/* GGR */
    EVYANKMODE, EVAUTOCLEAN, EVREGLTEXT, EVREGLNUM,
};

/* List of recognized environment variables. */

static struct evlist {
    char *var;
    enum ev_val tag;
} evl[] = {
 { "fillcol",   EVFILLCOL },    /* current fill column */
 { "pagelen",   EVPAGELEN },    /* number of lines used by editor */
 { "curcol",    EVCURCOL },     /* current column pos of cursor */
 { "curline",   EVCURLINE },    /* current line in file */
 { "ram",       EVRAM },        /* ram in use by malloc */
 { "flicker",   EVFLICKER },    /* flicker supression */
 { "curwidth",  EVCURWIDTH },   /* current screen width */
 { "cbufname",  EVCBUFNAME },   /* current buffer name */
 { "cfname",    EVCFNAME },     /* current file name */
 { "sres",      EVSRES },       /* current screen resolution */
 { "debug",     EVDEBUG },      /* macro debugging */
 { "status",    EVSTATUS },     /* returns the status of the last command */
 { "palette",   EVPALETTE },    /* current palette string */
 { "asave",     EVASAVE },      /* # of chars between auto-saves */
 { "acount",    EVACOUNT },     /* # of chars until next auto-save */
 { "lastkey",   EVLASTKEY },    /* last keyboard char struck */
 { "curchar",   EVCURCHAR },    /* current character under the cursor */
 { "discmd",    EVDISCMD },     /* display commands on command line */
 { "version",   EVVERSION },    /* current version number */
 { "progname",  EVPROGNAME },   /* returns current prog name - "MicroEMACS" */
 { "seed",      EVSEED },       /* current random number seed */
 { "disinp",    EVDISINP },     /* display command line input characters */
 { "wline",     EVWLINE },      /* # of lines in current window */
 { "cwline",    EVCWLINE },     /* current screen line in window */
 { "target",    EVTARGET },     /* target for line moves */
 { "search",    EVSEARCH },     /* search pattern */
 { "replace",   EVREPLACE },    /* replacement pattern */
 { "match",     EVMATCH },      /* last matched magic pattern */
 { "kill",      EVKILL },       /* kill buffer (read only) */
 { "cmode",     EVCMODE },      /* mode of current buffer */
 { "gmode",     EVGMODE },      /* global modes */
 { "tpause",    EVTPAUSE },     /* length to pause for paren matching */
 { "pending",   EVPENDING },    /* type ahead pending flag */
 { "lwidth",    EVLWIDTH },     /* width of current line */
 { "line",      EVLINE },       /* text of current line */
 { "gflags",    EVGFLAGS },     /* global internal emacs flags */
 { "rval",      EVRVAL },       /* child process return value */
 { "tab",       EVTAB },        /* tab 4 or 8 */
 { "overlap",   EVOVERLAP },    /* Overlap on next/prev page */
 { "jump",      EVSCROLLJUMP }, /* Number of lines jump on scroll */
 { "scroll",    EVSCROLL },     /* scroll enabled */
 { "inmb",      EVINMB },       /* In mini-buffer (read only) */

 { "fcol",      EVFCOL },       /* first displayed column in curent window */
 { "hjump",     EVHJUMP },      /* horizontal screen jump size */
 { "hscroll",   EVHSCROLL },    /* horizontal scrolling flag */
/* GGR */
 { "yankmode",  EVYANKMODE },   /* mode for yank (old/gnu) */
 { "autoclean", EVAUTOCLEAN },  /* Age at which dumped files are removed */
 { "regionlist_text", EVREGLTEXT },     /* makelist_region() indent */
 { "regionlist_number", EVREGLNUM },    /* numberlist_region() indent */
};

/* The tags for user functions - used in struct evlist */

enum uf_val {
    UFADD,      UFSUB,      UFTIMES,    UFDIV,      UFMOD,
    UFNEG,      UFCAT,      UFLEFT,     UFRIGHT,    UFMID,
    UFNOT,      UFEQUAL,    UFLESS,     UFGREATER,  UFSEQUAL,
    UFSLESS,    UFSGREAT,   UFIND,      UFAND,      UFOR,
    UFLENGTH,   UFUPPER,    UFLOWER,    UFTRUTH,    UFASCII,
    UFCHR,      UFGTKEY,    UFRND,      UFABS,      UFSINDEX,
    UFENV,      UFBIND,     UFEXIST,    UFFIND,     UFBAND,
    UFBOR,      UFBXOR,     UFBNOT,     UFXLATE,
};

enum function_type {
        NILNAMIC = 0,
        MONAMIC,
        DYNAMIC,
        TRINAMIC,
};

/* List of recognized user functions. */
struct user_function {
    char *f_name;
    enum function_type f_type;
    enum uf_val tag;
};

static struct user_function funcs[] = {
 { "add", DYNAMIC,  UFADD },    /* add two numbers together */
 { "sub", DYNAMIC,  UFSUB },    /* subtraction */
 { "tim", DYNAMIC,  UFTIMES },  /* multiplication */
 { "div", DYNAMIC,  UFDIV },    /* division */
 { "mod", DYNAMIC,  UFMOD },    /* mod */
 { "neg", MONAMIC,  UFNEG },    /* negate */
 { "cat", DYNAMIC,  UFCAT },    /* concatinate string */
 { "lef", DYNAMIC,  UFLEFT },   /* left string(string, len) */
 { "rig", DYNAMIC,  UFRIGHT },  /* right string(string, pos) */
 { "mid", TRINAMIC, UFMID },    /* mid string(string, pos, len) */
 { "not", MONAMIC,  UFNOT },    /* logical not */
 { "equ", DYNAMIC,  UFEQUAL },  /* logical equality check */
 { "les", DYNAMIC,  UFLESS },   /* logical less than */
 { "gre", DYNAMIC,  UFGREATER },/* logical greater than */
 { "seq", DYNAMIC,  UFSEQUAL }, /* string logical equality check */
 { "sle", DYNAMIC,  UFSLESS },  /* string logical less than */
 { "sgr", DYNAMIC,  UFSGREAT }, /* string logical greater than */
 { "ind", MONAMIC,  UFIND },    /* evaluate indirect value */
 { "and", DYNAMIC,  UFAND },    /* logical and */
 { "or",  DYNAMIC,  UFOR },     /* logical or */
 { "len", MONAMIC,  UFLENGTH }, /* string length */
 { "upp", MONAMIC,  UFUPPER },  /* uppercase string */
 { "low", MONAMIC,  UFLOWER },  /* lower case string */
 { "tru", MONAMIC,  UFTRUTH },  /* Truth of the universe logical test */
 { "asc", MONAMIC,  UFASCII },  /* char to integer conversion */
 { "chr", MONAMIC,  UFCHR },    /* integer to char conversion */
 { "gtk", NILNAMIC, UFGTKEY },  /* get 1 charater */
 { "rnd", MONAMIC,  UFRND },    /* get a random number */
 { "abs", MONAMIC,  UFABS },    /* absolute value of a number */
 { "sin", DYNAMIC,  UFSINDEX }, /* find the index of one string in another */
 { "env", MONAMIC,  UFENV },    /* retrieve a system environment var */
 { "bin", MONAMIC,  UFBIND },   /* loopup what function name is bound to a key */
 { "exi", MONAMIC,  UFEXIST },  /* check if a file exists */
 { "fin", MONAMIC,  UFFIND },   /* look for a file on the path... */
 { "ban", DYNAMIC,  UFBAND },   /* bitwise and   9-10-87  jwm */
 { "bor", DYNAMIC,  UFBOR },    /* bitwise or    9-10-87  jwm */
 { "bxo", DYNAMIC,  UFBXOR },   /* bitwise xor   9-10-87  jwm */
 { "bno", MONAMIC,  UFBNOT },   /* bitwise not */
 { "xla", TRINAMIC, UFXLATE },  /* XLATE character string translation */
};

#endif  /* EVAR_H_ */
