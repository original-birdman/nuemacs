/*      ESTRUCT.H
 *
 *      Structure and preprocessor defines
 *
 *      written by Dave G. Conroy
 *      modified by Steve Wilhite, George Jones
 *      substantially modified by Daniel Lawrence
 *      modified by Petri Kutvonen
 */
#ifndef ESTRUCT_H_
#define ESTRUCT_H_

#include "utf8.h"

#define MAXCOL  600
#define MAXROW  300

/* Various bits of code use chars as indices.
 * Ensure these are treated as unsigned.
 * Could use a compile option (-funsigned-char for gcc) but
 * that puts the logic into the build files rather than the actual code.
 * So use this macro to cast it when you want it as an int (0-255).
 */
#define ch_as_uc(bc) ((unsigned char)(bc))

/* The character with which to start/end message line info */
#define MLpre  "["
#define MLpost "]"
#define MLbkt(text) MLpre text MLpost

/* A macro so we can note which parameters are unused (happens a lot
 * for f and n in handler calls) while leaving the general compiler
 * warning on */
#define UNUSED(x) {(void)(x);}

/* Define an invalid unicode character to mark the end of lists */
#define UEM_NOCHAR 0x0FFFFFFF       /* GGR - NoChar. Top 4 bits special */

/*      Configuration options   */

#define CLRMSG  0  /* space clears the message line with no insert */
#define VISMAC  0  /* update display during keyboard macros        */
#define REVSTA  1  /* Status line appears in reverse video         */

#define COLOR   1

/* Make the file-locking machanism optional for any compilation */

#define FILOCK  0

#define XONXOFF 0  /* don't disable XON-XOFF flow control P.K.     */

/* Define some ability flags. */

#define MEMMAP  0

/* GGR - whether we want PATH to be searched before table lookup */
#if GGR_MODE
#define TABLE_THEN_PATH 1
#else
#define TABLE_THEN_PATH 0
#endif
#define PATH_THEN_TABLE !TABLE_THEN_PATH

/* Emacs global flag bit definitions (for gflags). */

#define GFREAD  1

/* GGR global option flag bits for ggr_opts */

/* Always swap 2 preceding chars */
#define GGR_TWIDDLE     0x0001
/* On next-word, go to end of next word, not start of next */
#define GGR_FORWWORD    0x0002
/* Do complete line wrapping in fill mode, not just the last word */
#define GGR_FULLWRAP    0x0004
/* Allow overlapping matches while searching */
#define GGR_SRCHOLAP    0x0008

/* Internal constants. */

/* GGR - Increase
 * NFILEN(80), NLINE(256) and NSTRING(128) to 513 each
 * NBUFN(16) to 32
 * Also, since result[NSTRING] in eval.c is used to get pathnames
 * NSTRING should be at least as large as NFILEN.
 */
#define NBINDS  256             /* max # of bound keys          */
#define NFILEN  513             /* # of bytes, file name        */
#define NBUFN   32              /* # of bytes, buffer name      */
#define NLINE   513             /* # of bytes, input line       */
#define NSTRING 513             /* # of bytes, string buffers   */
#define NKBDM   256             /* # of strokes, keyboard macro */
#define NPAT    128             /* # of bytes, pattern          */
#define NLOCKS  100             /* max # of file locks active   */
#define NCOLORS 8               /* number of supported colors   */
#define KBLOCK  250             /* sizeof kill buffer chunks    */

#define CONTROL 0x10000000      /* Control flag, or'ed in       */
#define META    0x20000000      /* Meta flag, or'ed in          */
#define CTLX    0x40000000      /* ^X flag, or'ed in            */
#define SPEC    0x80000000      /* special key (function keys)  */

#ifdef  FALSE
#undef  FALSE
#endif
#ifdef  TRUE
#undef  TRUE
#endif

#define FALSE   0               /* False, no, bad, etc.         */
#define TRUE    1               /* True, yes, good, etc.        */
#define ABORT   2               /* Death, ^G, abort, etc.       */
#define FAILED  3               /* not-quite fatal false return */

#define STOP    0               /* keyboard macro not in use    */
#define PLAY    1               /*                playing       */
#define RECORD  2               /*                recording     */

/* Types of buffer expanding in getstring() */

enum cmplt_type {   /* What is looked up and complete */
    CMPLT_NONE,     /* Nothing. Just take input */
    CMPLT_FILE,     /* File names */
    CMPLT_BUF,      /* Buffer names */
    CMPLT_PROC,     /* BTROC (user-procedure) buffer names */
    CMPLT_PHON,     /* BTPHON (translation table) buffer names */
    CMPLT_NAME,     /* Function names */
    CMPLT_VAR,      /* Variables (system $, user %, buffer .) */
    CMPLT_SRCH,     /* Nothing. Instead rotates search ring on <tab> */
};

/*      Directive definitions   */

#define DIF             0
#define DELSE           1
#define DENDIF          2
#define DGOTO           3
#define DRETURN         4
#define DENDM           5
#define DWHILE          6
#define DENDWHILE       7
#define DBREAK          8
#define DFORCE          9
#define DFINISH        10       /* GGR */

#define NUMDIRS        11       /* GGR */

/*
 * PTBEG, PTEND, FORWARD, and REVERSE are all toggle-able values for
 * the scan routines.
 */
#define PTBEG   0               /* Leave the point at the beginning on search   */
#define PTEND   1               /* Leave the point at the end on search         */
#define FORWARD 0               /* forward direction            */
#define REVERSE 1               /* backwards direction          */
#define REVERSE_NOSKIP 2        /* Special for scanmore -> step_scanner */

#define FIOSUC  0               /* File I/O, success.           */
#define FIOFNF  1               /* File I/O, file not found.    */
#define FIOEOF  2               /* File I/O, end of file.       */
#define FIOERR  3               /* File I/O, error.             */
#define FIOMEM  4               /* File I/O, out of memory      */
#define FIOFUN  5               /* File I/O, eod of file/bad line */

#define CFCPCN  0x0001          /* Last command was C-P, C-N    */
#define CFKILL  0x0002          /* Last command was a kill      */
#define CFYANK  0x0004          /* Last command was a yank      */

#define BELL    0x07            /* a bell character             */
#define TAB     0x09            /* a tab character              */

#define PATHCHR ':'

#define INTWIDTH        sizeof(int) * 3

/*      Macro argument token types                                      */

#define TKNUL   0               /* end-of-string                */
#define TKARG   1               /* interactive argument         */
#define TKBUF   2               /* buffer argument              */
#define TKVAR   3               /* user variables               */
#define TKENV   4               /* environment variables        */
#define TKFUN   5               /* function....                 */
#define TKDIR   6               /* directive                    */
#define TKLBL   7               /* line label                   */
#define TKLIT   8               /* numeric literal              */
#define TKSTR   9               /* quoted string literal        */
#define TKCMD   10              /* command name                 */
#define TKBVR   11              /* A buffer variable            */

/*      Internal defined functions                                      */

#define nextab(a)       (a & ~tabmask) + (tabmask+1)

/* DIFCASE represents the integer difference between upper
   and lower case letters.  It is an xor-able value, which is
   fortunate, since the relative positions of upper to lower
   case letters is the opposite of ascii in ebcdic.
*/
#define DIFCASE         0x20

/* Some data is kept on a per-window view when a file is displayed in a
 * window, and this is copied back to the file's buffer structure when
 * the window is closed.
 * Since this is common to struct window and struct buffer we'll make
 * it a struct as well, allowing a simple copy.
 */
struct locs {
    struct line *dotp;      /* Line containing "."          */
    struct line *markp;     /* Line containing "mark"       */
    int doto;               /* Byte offset for "."          */
    int marko;              /* Byte offset for "mark"       */
    int fcol;               /* first column displayed       */
};

/* There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay. Although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */
struct window {
    struct window *w_wndp;  /* Next window                  */
    struct buffer *w_bufp;  /* Buffer displayed in window   */
    struct line *w_linep;   /* Top line in the window       */
    struct locs w;
    int w_toprow;           /* Origin 0 top row of window   */
    int w_ntrows;           /* # of rows of text in window  */
    int w_force;            /* If NZ, forcing row.          */
    char w_flag;            /* Flags.                       */
#if COLOR
    char w_fcolor;          /* current forground color      */
    char w_bcolor;          /* current background color     */
#endif
};

#define WFFORCE 0x01            /* Window needs forced reframe  */
#define WFMOVE  0x02            /* Movement from line to line   */
#define WFEDIT  0x04            /* Editing within a line        */
#define WFHARD  0x08            /* Better to a full display     */
#define WFMODE  0x10            /* Update mode line.            */
#define WFCOLR  0x20            /* Needs a color change         */
#define WFKILLS 0x40            /* something was deleted        */
#define WFINS   0x80            /* something was inserted       */

/* Phonetic Translation tables.
 * Allocated in ptt_compile().
 * Freed (ptt_free) on buffer changes/removal:
 *  bclear(), ptt_compile() ifile(), readin(), lchange(), filter_buffer()
 *
 */
#define CASESET_LOWI_ALL -4
#define CASESET_LOWI_ONE -3
#define CASESET_CAPI_ALL -2
#define CASESET_CAPI_ONE -1
#define CASESET_OFF       0
#define CASESET_ON        1
struct ptt_ent {
    struct ptt_ent *nextp;
    char *from;
    char *to;
    unicode_t final_uc;         /* Last char in from, as unicode */
    int from_len;               /* in bytes */
    int from_len_uc;            /* in unicode */
    int to_len_uc;              /* in unicode */
/* The following fields are only set for the head */
    int bow_only;               /* Only match at beginning of word */
    int caseset;                /* Casing for replacement */
    char display_code[32];      /* Only 2 graphemes, though */
};


/* Text is kept in buffers. A buffer header, described below, exists for every
 * buffer in the system. The buffers are kept in a big list, so that commands
 * that search for a buffer by name can find the buffer header. There is a
 * safe store for the dot and mark in the header, but this is only valid if
 * the buffer is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with a pointer to
 * the header line in "b_linep".
 *      Buffers may be "Inactive" which means the files associated with them
 * have not been read in yet. These get read in at "use buffer" time.
 */

/* Max #chars in a var name (user or buffer) */
#define NVSIZE  32

/* Structure to hold user and buffer variables and their definitions.
 * MAXVARS(64) user vars are defined globally
 * BVALLOC(32) are allocated to each BTPROC buffer.
 */
#define BVALLOC 32
struct simple_variable {
    char *value;            /* value (string) */
    char name[NVSIZE + 1];  /* name of buffer variable */
};

/* Structure for function/buffer-proc options */
struct func_opts {
    unsigned int skip_in_macro :1;
    unsigned int not_mb :1;
    unsigned int not_interactive :1;
    unsigned int search_ok :1;      /* *hunt() can run */
    unsigned int one_pass :1;       /* ignore any repeat arg (user-proc) */
};

/* These are allocated in bfind()  and freed in zotbuf() */
struct buffer {
    struct buffer *b_bufp;  /* Link to next struct buffer   */
    struct line *b_linep;   /* Link to the header struct line */
    struct line *b_topline; /* Link to narrowed top text    */
    struct line *b_botline; /* Link to narrowed bottom text */
    struct ptt_ent *ptt_headp;
    struct simple_variable *bv; /* Only for b_type = BTPROC */
    struct locs b;
    struct func_opts btp_opt;   /* Only for b_type = BTPROC */
    int b_type;             /* Type of buffer */
    int b_exec_level;       /* Recursion level */
    int b_mode;             /* editor mode of this buffer   */
    int b_EOLmissing;       /* When read in... */
    int b_keylen;           /* encrypted key len            */
    char b_active;          /* window activated flag        */
    char b_nwnd;            /* Count of windows on buffer   */
    char b_flag;            /* Flags                        */
    char b_fname[NFILEN];   /* File name                    */
    char b_bname[NBUFN];    /* Buffer name                  */
    char b_key[NPAT];       /* current encrypted key        */
};

#define BTNORM  0               /* A "normal" buffer            */
#define BTSPEC  1               /* Generic special buffer       */
#define BTPROC  2               /* Buffer from store-procedure  */
#define BTPHON  3               /* Buffer from store-pttable    */

#define BFINVS  0x01            /* Internal invisable buffer    */
#define BFCHG   0x02            /* Changed since last write     */
#define BFTRUNC 0x04            /* buffer was truncated when read */
#define BFNAROW 0x08            /* buffer has been narrowed - GGR */

/*      mode flags      */

#define MDWRAP  0x0001          /* word wrap                    */
#define MDCMOD  0x0002          /* C indentation and fence match */
#define MDPHON  0x0004          /* Phonetic input handling      */
#define MDEXACT 0x0008          /* Exact matching for searches  */
#define MDVIEW  0x0010          /* read-only buffer             */
#define MDOVER  0x0020          /* overwrite mode               */
#define MDMAGIC 0x0040          /* regular expresions in search */
#define MDCRYPT 0x0080          /* encryption mode active       */
#define MDASAVE 0x0100          /* auto-save mode               */
#define MDEQUIV 0x0200          /* Equivalent unicode searching */
#define MDDOSLE 0x0400          /* DOS line endings             */
#define MDRPTMG 0x0800          /* Report match in Magic mode   */
/* Equiv mode only applies in Magic mode, so this is useful */
#define MD_MAGEQV (MDMAGIC | MDEQUIV)

#define NUMMODES    12          /* # of defined modes           */

/* The starting position of a region, and the size of the region in
 * characters, is kept in a region structure.  Used by the region commands.
 */
struct region {
    struct line *r_linep;       /* Origin struct line address.  */
    int r_offset;               /* Origin struct line offset.   */
    long r_bytes;               /* Length in bytes.             */
    struct line *r_endp;        /* line address at end.         */
};

/* Used by inword() to override current point usage. */

struct inwbuf {
    struct line *lp;            /* Line pointer */
    int offs;                   /* offset within line */
};

/* The editor communicates with the display using a high level interface. A
 * "TERM" structure holds useful variables, and indirect pointers to routines
 * that do useful operations. The low level get and put routines are here too.
 * This lets a terminal, in addition to having non standard commands, have
 * funny get and put character code too. The calls might get changed to
 * "termp->t_field" style in the future, to make it possible to run more than
 * one terminal type.
 */
struct terminal {
    void (*t_open)(void);       /* Open terminal at the start.   */
    void (*t_close)(void);      /* Close terminal at end.        */
    void (*t_kopen)(void);      /* Open keyboard                 */
    void (*t_kclose)(void);     /* close keyboard                */
    int (*t_getchar)(void);     /* Get character from keyboard.  */
    int (*t_putchar)(int);      /* Put character to display.     */
    void (*t_flush) (void);     /* Flush output buffers.         */
    void (*t_move)(int, int);   /* Move the cursor, origin 0.    */
    void (*t_eeol)(void);       /* Erase to end of line.         */
    void (*t_eeop)(void);       /* Erase to end of page.         */
    void (*t_beep)(void);       /* Beep.                         */
    void (*t_rev)(int);         /* set reverse video state       */
    int (*t_rez)(char *);       /* change screen resolution      */
#if COLOR
    void (*t_setfor) (int);     /* set foreground color          */
    void (*t_setback) (int);    /* set background color          */
#endif
    void (*t_scroll)(int, int,int); /* scroll a region of the screen */
    int t_mrow;                 /* max rows (allocated)          */
    int t_nrow;                 /* current number of rows used   */
/* Next two are derived from t_nrow (-1, -2 resp), but it makes
 * things easier to read/process if we calcutae them one.
 * USE the SET_t_nrow to set t_nrow - it sets the others too.
 */
    int t_mbline;               /* 0-based minibuffer-line index */
    int t_vscreen;              /* 0-based vscreen max index     */
    int t_mcol;                 /* max columns (allocated)       */
    int t_ncol;                 /* current Number of columns.    */
    int t_margin;               /* min margin for extended lines */
    int t_scrsiz;               /* size of scroll region "       */
    int t_pause;                /* # times thru update to pause  */
};

/* TEMPORARY macros for terminal I/O  (to be placed in a machine
                                            dependant place later)      */

#define TTopen      (*term.t_open)
#define TTclose     (*term.t_close)
#define TTkopen     (*term.t_kopen)
#define TTkclose    (*term.t_kclose)
#define TTgetc      (*term.t_getchar)
#define TTputc      (*term.t_putchar)
#define TTflush     (*term.t_flush)
#define TTmove      (*term.t_move)
#define TTeeol      (*term.t_eeol)
#define TTeeop      (*term.t_eeop)
#define TTbeep      (*term.t_beep)
#define TTrev       (*term.t_rev)
#define TTrez       (*term.t_rez)
#if COLOR
#define TTforg      (*term.t_setfor)
#define TTbacg      (*term.t_setback)
#endif

/* Structures for the table of initial key bindings.
 * NOTE: pbp buffer names are allocated in buffertokey()
 * and freed in unbindkey() and, possibly, bindtokey().
 */
#define ENDS_KMAP -1
#define ENDL_KMAP 0
#define FUNC_KMAP 1
#define PROC_KMAP 2
typedef int (*fn_t)(int, int);  /* Bound function prototype*/
struct key_tab {
    int k_type;             /* function or procedure buffer */
    int k_code;             /* Key code */
    union {
        fn_t k_fp;          /* Routine to handle it, or... */
        char *pbp;          /* ...procedure buffer name */
    } hndlr;
    struct name_bind *fi;   /* Function info */
    int bk_multiplier;      /* Binding multiplier */
};
struct key_tab_init {       /* Initializing data */
    int k_code;             /* Key code */
    fn_t k_fp;              /* Routine to handle it */
};

/* Structure for the name binding table. */
struct name_bind {
    char *n_name;            /* name of function */
    fn_t n_func;             /* function the name is bound to */
    struct func_opts opt;
};

/* The editor holds deleted text chunks in the struct kill buffer. The
 * kill buffer is logically a stream of ascii characters, however
 * due to its unpredictable size, it gets implemented as a linked
 * list of chunks. (The d_ prefix is for "deleted" text, as k_
 * was taken up by the keycode structure).
 */
struct kill {
    struct kill *d_next;   /* Link to next chunk, NULL if last. */
    char d_chunk[KBLOCK];  /* Deleted text. */
};

/* When emacs' command interpreter needs to get a variable's name,
 * rather than it's value, it is passed back as a variable description
 * structure. The v_num field is a index into the appropriate variable table.
 */
struct variable_description {
    int v_type;  /* Type of variable. */
    int v_num;   /* Ordinal pointer to variable in list. */
};

/* The !WHILE directive in the execution language needs to
 * stack references to pending whiles. These are stored linked
 * to each currently open procedure via a linked list of
 * the following structure.
*/
struct while_block {
    struct line *w_begin;        /* ptr to !while statement */
    struct line *w_end;          /* ptr to the !endwhile statement */
    int w_type;                  /* block type */
    struct while_block *w_next;  /* next while */
};

#define BTWHILE         1
#define BTBREAK         2

/* Let the user decide which functions should re-use their args when
 * reexecing.
 * These keys are tagged with the name of the function handler.
 */
#define RXARG_forwsearch    0x00000001
#define RXARG_backsearch    0x00000002
#define RXARG_namedcmd      0x00000004
#define RXARG_execcmd       0x00000008
#define RXARG_execproc      0x00000010
#define RXARG_execbuf       0x00000020
#define RXARG_execfile      0x00000040
#define RXARG_quote         0x00000080
#define RXARG_spawn         0x00000100
#define RXARG_execprg       0x00000200
#define RXARG_pipecmd       0x00000400
#define RXARG_filter_buffer 0x00000800

#define RX_ON  1
#define RX_OFF 0

/* Macro to allow, e.g.,  RXARG(forwsearch) in coding */

#define RXARG(func) (rxargs & RXARG_ ## func)

/* Used for mapping rexec args on/off */
struct rx_mask {
    char *entry;
    int mask;
};

/* The tags for environment variable - used in struct evlist so needed
 * here.
 */
enum ev_val {
    EVFILLCOL,  EVPAGELEN,  EVCURCOL,   EVCURLINE,  EVFLICKER,
    EVCURWIDTH, EVCBUFNAME, EVCFNAME,   EVSRES,     EVDEBUG,
    EVSTATUS,   EVPALETTE,  EVASAVE,    EVACOUNT,   EVLASTKEY,
    EVCURCHAR,  EVDISCMD,   EVVERSION,  EVPROGNAME, EVSEED,
    EVDISINP,   EVWLINE,    EVCWLINE,   EVTARGET,   EVSEARCH,
    EVREPLACE,  EVMATCH,    EVKILL,     EVCMODE,    EVGMODE,
    EVTPAUSE,   EVPENDING,  EVLWIDTH,   EVLINE,     EVGFLAGS,
    EVRVAL,     EVTAB,      EVOVERLAP,  EVSCROLLJUMP,
    EVSCROLL,   EVINMB,     EVFCOL,     EVHJUMP,    EVHSCROLL,
/* GGR */
    EVYANKMODE, EVAUTOCLEAN, EVREGLTEXT, EVREGLNUM, EVAUTODOS,
    EVSDTKSKIP, EVUPROCOPTS, EVFORCESTAT, EVEQUIVTYPE, EVSRCHCANHUNT,
    EVULPCOUNT, EVULPTOTAL, EVULPFORCED,
    EVSDOPTS,   EVGGROPTS,  EVSYSTYPE,
    EVFORCEMODEON,  EVFORCEMODEOFF,     EVPTTMODE,
};

struct evlist {
    char *var;
    enum ev_val tag;
};

/* Settings for userproc_opt */

#define UPROC_FIXUP 0x0000001

#endif
