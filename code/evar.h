/*      EVAR.H
 *
 *      Environment and user variable definitions
 *
 *      written 1986 by Daniel Lawrence
 *      modified by Petri Kutvonen
 */
#ifndef EVAR_H_
#define EVAR_H_

/* List of recognized environment variables. */

struct evlist evl[] = {
 { "fillcol",   EVFILLCOL },    /* current fill column */
 { "pagelen",   EVPAGELEN },    /* number of lines used by editor */
 { "curcol",    EVCURCOL },     /* current column pos of cursor */
 { "curline",   EVCURLINE },    /* current line in file */
 { "curwidth",  EVCURWIDTH },   /* current screen width */
 { "cbufname",  EVCBUFNAME },   /* current buffer name */
 { "cfname",    EVCFNAME },     /* current file name */
 { "debug",     EVDEBUG },      /* macro debugging */
 { "status",    EVSTATUS },     /* returns the status of the last command */
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
 { "search",    EVSEARCH },     /* search pattern (read-only) */
 { "replace",   EVREPLACE },    /* replacement pattern */
 { "match",     EVMATCH },      /* last matched magic pattern */
 { "kill",      EVKILL },       /* kill buffer (read only) */
 { "cmode",     EVCMODE },      /* mode of current buffer */
 { "gmode",     EVGMODE },      /* global modes */
 { "tpause",    EVTPAUSE },     /* length to pause for paren matching */
 { "pending",   EVPENDING },    /* type ahead pending flag */
 { "lwidth",    EVLWIDTH },     /* width of current line */
 { "line",      EVLINE },       /* text of current line */
 { "rval",      EVRVAL },       /* child process return value */
 { "tab",       EVTAB },        /* tab 4 or 8 */
 { "overlap",   EVOVERLAP },    /* Overlap on next/prev page */
 { "jump",      EVSCROLLJUMP }, /* Number of lines jump on scroll */
 { "scroll",    EVSCROLL },     /* scroll enabled */
 { "inmb",      EVINMB },       /* In mini-buffer (read only) */
 { "fcol",      EVFCOL },       /* first displayed column in current window */
 { "hjump",     EVHJUMP },      /* horizontal screen jump size */
 { "hscroll",   EVHSCROLL },    /* horizontal scrolling flag */
/* GGR */
 { "yankmode",  EVYANKMODE },   /* mode for yank (old/gnu) */
 { "autoclean", EVAUTOCLEAN },  /* Age at which dumped files are removed */
 { "regionlist_text", EVREGLTEXT },     /* makelist_region() indent */
 { "regionlist_number", EVREGLNUM },    /* numberlist_region() indent */
 { "autodos",   EVAUTODOS },    /* Check for DOS on read-in? */
 { "showdir_tokskip", EVSDTKSKIP }, /* Tokens to skip in showdir */
 { "uproc_opts",    EVUPROCOPTS },  /* Flags when processing &arg */
 { "force_status",  EVFORCESTAT },  /* Actual status when forced to OK */
 { "equiv_type",    EVEQUIVTYPE },  /* Function to use for Equiv mode */
 { "srch_can_hunt", EVSRCHCANHUNT },/* To allow forw/backhunt to work
                                       in test macros */
 { "uproc_lpcount", EVULPCOUNT },   /* Current loop for Esc<n>func */
 { "uproc_lptotal", EVULPTOTAL },   /* <n> for Esc<n>func */
 { "uproc_lpforced", EVULPFORCED }, /* Did user set <n>? */
 { "showdir_opts", EVSDOPTS },  /* Start-up state (for user to set) */
 { "ggr_opts",  EVGGROPTS },    /* Optional-style difference */
 { "sys_type",  EVSYSTYPE },    /* System type (== uname -s) */
 { "force_mode_on", EVFORCEMODEON },    /* Modes to force on (read-only) */
 { "force_mode_off", EVFORCEMODEOFF },  /* Modes to force off (read-only) */
 { "pttmode",   EVPTTMODE },    /* Current ptt mode, or "" (read-only)  */
 { "vismac", EVVISMAC },        /* update display during keyboard macros? */
 { "filock", EVFILOCK },        /* Do we want file locking? */
 { "crypt_mode", EVCRYPT },     /* Crypt mode to use (default NONE) */
};

/* The tags for user functions - used in struct evlist */

enum uf_val {
    UFADD,      UFSUB,      UFTIMES,    UFDIV,      UFMOD,  UFNEG,  UFABS,
    UFEQUAL,    UFLESS,     UFGREATER,  UFNOT,      UFAND,  UFOR,
    UFBAND,     UFBOR,      UFBXOR,     UFBNOT,     UFBLIT,
    UFCAT,      UFLEFT,     UFRIGHT,    UFMID,      UFSEQUAL,   UFSLESS,
    UFSGREAT,   UFLENGTH,   UFUPPER,    UFLOWER,    UFESCAPE,
    UFSINDEX,   UFRINDEX,
    UFIND,      UFTRUTH,    UFASCII,    UFCHR,      UFGTKEY,    UFRND,
    UFENV,      UFBIND,     UFEXIST,    UFBXIST,    UFFIND,     UFXLATE,
    UFGRPTEXT,  UFPRINTF,
    UFRADD,     UFRSUB,     UFRTIMES,   UFRDIV,     UFRPOW,
    UFRLESS,    UFRGREAT,   UFR2I,
};

enum function_type {
        NILNAMIC = 0,
        MONAMIC,
        DINAMIC,
        TRINAMIC,
};

/* List of recognized user functions. */
struct user_function {
    char *f_name;
    enum function_type f_type;
    enum uf_val tag;
};

static struct user_function funcs[] = {
/* Integer arithmetic*/
 { "add", DINAMIC,  UFADD },    /* add two numbers together */
 { "sub", DINAMIC,  UFSUB },    /* subtraction */
 { "tim", DINAMIC,  UFTIMES },  /* multiplication */
 { "div", DINAMIC,  UFDIV },    /* integer division */
 { "mod", DINAMIC,  UFMOD },    /* mod */
 { "neg", MONAMIC,  UFNEG },    /* negate */
 { "abs", MONAMIC,  UFABS },    /* absolute value of a number */

/* Logical operators */
 { "equ", DINAMIC,  UFEQUAL },  /* logical equality check */
 { "les", DINAMIC,  UFLESS },   /* logical less than */
 { "gre", DINAMIC,  UFGREATER },/* logical greater than */
 { "not", MONAMIC,  UFNOT },    /* logical not */
 { "and", DINAMIC,  UFAND },    /* logical and */
 { "or",  DINAMIC,  UFOR },     /* logical or */

/* Bitwise functions */
 { "ban", DINAMIC,  UFBAND },   /* bitwise and   9-10-87  jwm */
 { "bor", DINAMIC,  UFBOR },    /* bitwise or    9-10-87  jwm */
 { "bxo", DINAMIC,  UFBXOR },   /* bitwise xor   9-10-87  jwm */
 { "bno", MONAMIC,  UFBNOT },   /* bitwise not */
 { "bli", MONAMIC,  UFBLIT },   /* bit literal (for hex, dec, oct input) */

/* String functions */
 { "cat", DINAMIC,  UFCAT },    /* concatenate string */
 { "lef", DINAMIC,  UFLEFT },   /* left string(string, len) */
 { "rig", DINAMIC,  UFRIGHT },  /* right string(string, pos) */
 { "mid", TRINAMIC, UFMID },    /* mid string(string, pos, len) */
 { "seq", DINAMIC,  UFSEQUAL }, /* string logical equality check */
 { "sle", DINAMIC,  UFSLESS },  /* string logical less than */
 { "sgr", DINAMIC,  UFSGREAT }, /* string logical greater than */
 { "len", MONAMIC,  UFLENGTH }, /* string length */
 { "upp", MONAMIC,  UFUPPER },  /* uppercase string */
 { "low", MONAMIC,  UFLOWER },  /* lower case string */
 { "esc", MONAMIC,  UFESCAPE }, /* SHELL escape string */
 { "sin", DINAMIC,  UFSINDEX }, /* find the index of one string in another */
 { "rin", DINAMIC,  UFRINDEX }, /* reverse index of one string in another */

/* Miscellaneous functions */
 { "ind", MONAMIC,  UFIND },    /* evaluate indirect value */
 { "tru", MONAMIC,  UFTRUTH },  /* Truth of the universe logical test */
 { "asc", MONAMIC,  UFASCII },  /* char to integer conversion */
 { "chr", MONAMIC,  UFCHR },    /* integer to char conversion */
 { "gtk", NILNAMIC, UFGTKEY },  /* get 1 Unicode character */
 { "rnd", MONAMIC,  UFRND },    /* get a random number */
 { "env", MONAMIC,  UFENV },    /* retrieve an OS system environment var */
 { "bin", MONAMIC,  UFBIND },   /* lookup name of function bound to a key */
 { "exi", MONAMIC,  UFEXIST },  /* check whether a file exists */
 { "exb", MONAMIC,  UFBXIST },  /* check whether a buffer exists */
 { "fin", MONAMIC,  UFFIND },   /* look for a file on the path... */
 { "xla", TRINAMIC, UFXLATE },  /* XLATE character string translation */
 { "grp", MONAMIC,  UFGRPTEXT}, /* Text in group for last match */
 { "ptf", MONAMIC,  UFPRINTF},  /* printf-style string creator */

/* Real artihmetic */
 { "rad", DINAMIC,  UFRADD },   /* add */
 { "rsu", DINAMIC,  UFRSUB },   /* subtract */
 { "rti", DINAMIC,  UFRTIMES }, /* multiply */
 { "rdv", DINAMIC,  UFRDIV },   /* division */
 { "rpw", DINAMIC,  UFRPOW },   /* power (exp) */
 { "rlt", DINAMIC,  UFRLESS },  /* less than */
 { "rgt", DINAMIC,  UFRGREAT }, /* greater than */
 { "r2i", MONAMIC,  UFR2I },    /* round real to int */
};

#endif  /* EVAR_H_ */
