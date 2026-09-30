#include <utf8proc.h>

#define GLOBALS_C

#include "estruct.h"
#include "edef.h"

/* initialized global definitions */

int fillcol = 72;               /* Current fill column          */
dbp_dcl(execstr) = NULL;        /* pointer to string to execute */
const char *mode2name[] = {     /* Display name of modes        */
                                /* Also text when checking them */
        "Wrap",  "Cmode", "Phon",  "Exact", "View", "Over",
        "Magic", "Crypt", "Asave", "eQuiv", "Dos", "Report"
};
int ml_text_offset = 0;         /* n chars in message line buffer */
char modecode[] = "WCPEVOMYAQDR";   /* letters to represent modes   */
int gmode = 0;                  /* global editor mode           */
int force_mode_on = 0;          /* modes forced on              */
int force_mode_off = 0;         /* modes forced off             */
db_bufdef(glfcolor);            /* global foreground            */
db_bufdef(hifcolor);            /* highlight foreground         */
db_bufdef(hibcolor);            /* highlight background         */
int gasave = 256;               /* global ASAVE size            */
int gacount = 256;              /* count until next ASAVE       */
int sgarbf = TRUE;              /* TRUE if screen is garbage    */
int clexec = FALSE;             /* command line execution flag  */
int discmd = TRUE;              /* display command flag         */
int disinp = TRUE;              /* display input characters     */
int vismac = FALSE;             /* update display during keyboard macros? */
int filock = FALSE;             /* Do we want file-locking */
int crypt_mode = 0;             /* Crypt mode - default is NONE */
char gl_enc_key[NKEY];          /* Global encryption key */
int gl_enc_len = 0;             /* Global encryption key length. 0 == unset */
int ttrow = -1;                 /* Row location of HW cursor */
int ttcol = -1;                 /* Column location of HW cursor */
int lbound = 0;                 /* leftmost column of current line
                                   being displayed */
int metac = CONTROL | '[';      /* current meta character */
int ctlxc = CONTROL | 'X';      /* current control X prefix char */
int reptc = CONTROL | 'U';      /* current universal repeat char */
int abortc = CONTROL | 'G';     /* current abort command char   */

int tabmask = 0x07;             /* tabulator mask */
const char *cname[] = {         /* names of colors              */
        "BLACK", "RED", "GREEN", "YELLOW", "BLUE",
        "MAGENTA", "CYAN", "WHITE"
};
struct kill *kbufp = NULL;      /* current kill buffer chunk pointer    */
struct kill *kbufh[] = {[0 ... KRING_SIZE-1] = NULL};
                                /* kill buffer header pointers          */
int kused[KRING_SIZE] = {[0 ... KRING_SIZE-1] = KBLOCK};
                                /* # of bytes used in kill buffer       */
struct window *swindow = NULL;  /* saved window pointer                 */
int cryptflag = FALSE;          /* currently encrypting?                */
int *kbdptr;                    /* current position in keyboard buf */
int kbdmode = STOP;             /* current keyboard macro mode  */
int kbdrep = 0;                 /* number of repetitions        */
int restflag = FALSE;           /* restricted use?              */
struct {
    unicode_t last;             /* Last key entered */
    int count;                  /* Total key entered count */
} inkey = { 0, 0 };
int macbug = 0;                 /* macro debuging flag          */
int macbug_off = 0;             /* macro debug global-off flag  */
char errorm[] = "ERROR";        /* error literal                */
char truem[] = "TRUE";          /* true literal                 */
char falsem[] = "FALSE";        /* false litereal               */
int cmdstatus = TRUE;           /* last command status          */
int rval = 0;                   /* return value of a subprocess */
int overlap = 0;                /* line overlap in forw/back page */
int scrolljump = 0;             /* no. lines to scroll (0 == centre screen) */

struct window *wheadp = NULL;   /* vtinit() needs to check this */

/* uninitialized global definitions */

int *kbdm;                      /* Macro buffer                 */
int n_kbdm;                     /* Allocated size of kbdm       */
int *kbdend;                    /* ptr to end of the keyboard   */

int currow;                     /* Cursor row                   */
int curcol;                     /* Cursor column                */
int com_flag;                   /* Command flags                */
int curgoal;                    /* Display column goal for C-P, C-N */
struct window *curwp;           /* Current window               */
struct buffer *curbp;           /* Current buffer               */
struct buffer *bheadp;          /* Head of list of buffers      */
struct buffer *blistp = NULL;   /* Buffer for C-X C-B           */
struct buffer *bdbgp;           /* Buffer for macro debug info  */

/* GGR - Add one to these three to allow for trailing NULs      */
db_bufdef(pat);                    /* Search pattern               */
db_bufdef(tap);                    /* Reversed pattern array.      */
db_bufdef(rpat);                   /* replacement pattern          */

struct line *fline;             /* dynamic return line */

/* The variable srch_patlen holds the length of the search pattern
 */
int srch_patlen = 0;

/* directive name table:
 * This holds the names of all the directives....
 * It MUST correspond to the Directive definitions defines in estruct.h
 */
const char *dname[] = {
        "if", "else", "endif",
        "goto", "return", "endm",
        "while", "endwhile", "break",
        "force"
        , "finish"              /* GGR */
};

/* GGR - Additional initializations */
int  inreex          = FALSE;

int  allow_current   = 0;
unicode_t *eos_list  = NULL;
int  inmb            = FALSE;
int  pathexpand      = TRUE;
db_bufdef(savnam);
int do_savnam        = 1;

int  silent          = FALSE;

char *input_waiting  = NULL;

int keytab_alloc_ents = 0;

struct buffer *ptt = NULL;
int no_newline_in_pttex = 0;

int hscroll = FALSE;
int hjump = 1;
int autodos = TRUE;         /* Default is to do the check */
/* Set the default to the expected GNU/Linux case
 * but allow it to be overriden by a compile-time define.
 */
#ifndef SDIR_SKIP
#define SDIR_SKIP 8
#endif
int showdir_tokskip = SDIR_SKIP;
int uproc_opts = 0;

const char kbdmacro_buffer[] = "//kbd_macro";
struct buffer *kbdmac_bp = NULL;

int run_filehooks = 0;

mb_info_st mb_info = { NULL, NULL, NULL, 0 };

not_in_mb_st not_in_mb = { NULL, 0 };
const char *not_interactive_fname = NULL;

int pause_key_index_update = 0;

/* Contains a db string struct */
prmpt_buf_st prmpt_buf = { db_buf_initval, 0, 0 };

enum yank_type last_yank = None;

enum yank_style yank_mode = Old;

int autoclean = 7;

char regionlist_text[MAX_REGL_LEN] = " o ";
char regionlist_number[MAX_REGL_LEN] = " %2d. ";

db_bufdef(readin_mesg);

int running_function = 0;
const char *current_command = NULL;

func_arg f_arg = { NULL, { META|SPEC|'C', TRUE, 1 } }, p_arg;

/* reexecute arg mappings... */
struct rx_mask rx_mask[] = {
    { "search-forward",         RXARG_forwsearch },
    { "search-reverse",         RXARG_backsearch },
    { "execute-named-command",  RXARG_namedcmd },
    { "execute-command-line",   RXARG_execcmd },
    { "execute-procedure",      RXARG_execproc },
    { "run",                    RXARG_execproc },
    { "execute-buffer",         RXARG_execbuf },
    { "execute-file",           RXARG_execfile },
    { "quote-character",        RXARG_quote },
    { "shell-command",          RXARG_spawn },
    { "execute-program",        RXARG_execprg },
    { "pipe-command",           RXARG_pipecmd },
    { "filter-buffer",          RXARG_filter_buffer },
    { NULL, 0 },
};
/* ...and the current setting */
int rxargs = ~0;            /* Everything on by default */

const char *userproc_arg = NULL;

int comline_processing = 1;

const char *force_status = "UNSET";

utf8proc_uint8_t *(*equiv_handler)(const utf8proc_uint8_t *) = utf8proc_NFKC;

struct buffer *group_match_buffer = NULL;

int no_macrobuf_record = 0;

int mline_persist = FALSE;

struct buffer *execbp = NULL;

int srch_can_hunt = 0;

int uproc_lpcount = 0;
int uproc_lptotal = 0;
int uproc_lpforced = 0;

/* Markers for META|SPEC handler being active. */

meta_spec_flags_t meta_spec_active = { 0, 0, 0, 0 };

int ggr_opts = 0;

int pretend_size = FALSE;

char *dump_message = NULL;

/* A global db, for use in localized code.
 * Must NOT be used in calls to a function which might use it itself!!!
 * Must be db_free()d in quit() in main.c. when FREE is set.
 */
db_bufdef(glb_db);

/* Index info for binary chop indexes */

struct bc_info key_info;    /* Lookup key bind(s) for a function */
struct bc_info evl_info;    /* Lookup environment variables */
struct bc_info ufc_info;    /* Lookup user functions (&...) */

struct bc_info nfc_info;    /* Lookup command functions by name */
struct bc_info fcn_info;    /* Lookup command functions by func */

/* A system-wide mark for temporarily saving the current location.
 * p MUST be reset to NULL after every restore!!!
 */
sysmark_t sysmark = { NULL, 0 };

/* Stored as s + ns, but $brkt_ms works in ms */
struct timespec pause_time = { 0, 200000000 };

/* Where macro pins hang out */

linked_items *macro_pin_headp = NULL;

/* Three locations worked out once at the start/
 * All malloc'ed.
 */

udir_t udir;

/* Info for deferred SIGWINCH handling */

volatile struct sigdefer sigwin_dfr = {TRUE, FALSE};

/* A flag set when we don't want a NUL */

int no_quoted_NUL = FALSE;

/* Whether to remap NUL to UEM_NOCHAR in tgetc */

int ret_nochar = 0;

/* Set when we are handling SIGWINCH */

volatile int handling_sigwinch = 0;

/* Pending (unprocessed) chars in the read input buffer */

int pending_rch = 0;

/* Pretend screen is this much narrower, to avoid autowrap */

int fake_narrow = 0;

/* The overflow indicator and nodisplay method */

unicode_t ovflw = 0x22EF;   /* Ellipsis */
unicode_t nodisplay = 0;    /* Display glyphs */

/* Ignored prefixes  - need to limit assignment... */

const char *path_pfx_map = NULL;
const char *path_pfx_map_from[MAX_PFX_MAP];
const char *path_pfx_map_to[MAX_PFX_MAP];
int path_pfx_map_from_len[MAX_PFX_MAP];
int path_pfx_map_to_len[MAX_PFX_MAP];
int path_pfx_map_valid = 0;
