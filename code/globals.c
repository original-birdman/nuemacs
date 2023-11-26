#include <utf8proc.h>

#define GLOBALS_C

#include "estruct.h"
#include "edef.h"

/* initialized global definitions */

int fillcol = 72;               /* Current fill column          */
char *execstr = NULL;           /* pointer to string to execute */
char *mode2name[] = {           /* Display name of modes        */
                                /* Also text when checking them */
        "Wrap",  "Cmode", "Phon",  "Exact", "View", "Over",
        "Magic", "Crypt", "Asave", "eQuiv", "Dos", "Report"
};
char modecode[] = "WCPEVOMYAQDR";   /* letters to represent modes   */
int gmode = 0;                  /* global editor mode           */
int force_mode_on = 0;          /* modes forced on              */
int force_mode_off = 0;         /* modes forced off             */
int gfcolor = 7;                /* global forgrnd color (white) */
int gbcolor = 0;                /* global backgrnd color (black) */
int gasave = 256;               /* global ASAVE size            */
int gacount = 256;              /* count until next ASAVE       */
int sgarbf = TRUE;              /* TRUE if screen is garbage    */
int mpresf = FALSE;             /* TRUE if message in last line */
int clexec = FALSE;             /* command line execution flag  */
int discmd = TRUE;              /* display command flag         */
int disinp = TRUE;              /* display input characters     */
int vismac = FALSE;             /* update display during keyboard macros? */
int filock = FALSE;             /* Do we want file-locking */
int crypt_mode = 0;             /* Crypt mode - default is NONE */
char gl_enc_key[NPAT];          /* Global encryption key */
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
char *cname[] = {               /* names of colors              */
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
int lastkey = 0;                /* last keystoke                */
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

int kbdm[NKBDM];                /* Macro                        */
int *kbdend = kbdm;             /* ptr to end of the keyboard */

int eolexist;                   /* does clear to EOL exist      */
int revexist;                   /* does reverse video exist?    */

int currow;                     /* Cursor row                   */
int curcol;                     /* Cursor column                */
int com_flag;                   /* Command flags                */
int curgoal;                    /* Display column goal for C-P, C-N */
struct window *curwp;           /* Current window               */
struct buffer *curbp;           /* Current buffer               */
struct buffer *bheadp;          /* Head of list of buffers      */
struct buffer *blistp;          /* Buffer for C-X C-B           */
struct buffer *bdbgp;           /* Buffer for macro debug info  */

/* GGR - Add one to these three to allow for trailing NULs      */
char pat[NPAT+1];               /* Search pattern               */
char tap[NPAT+1];               /* Reversed pattern array.      */
char rpat[NPAT+1];              /* replacement pattern          */

struct line *fline;             /* dynamic return line */

/* The variable srch_patlen holds the length of the search pattern
 */
unsigned int srch_patlen = 0;

/* directive name table:
        This holds the names of all the directives....  */

char *dname[] = {
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
char savnam[NBUFN]   = "main";
int do_savnam        = 1;

int  silent          = FALSE;

int chg_width        = 0;
int chg_height       = 0;

char *input_waiting  = NULL;

int keytab_alloc_ents = 0;

struct buffer *ptt = NULL;

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
char *not_interactive_fname = NULL;

int pause_key_index_update = 0;

prmpt_buf_st prmpt_buf = { NULL, 0, "" };

enum yank_type last_yank = None;

enum yank_style yank_mode = Old;

int autoclean = 7;

char regionlist_text[MAX_REGL_LEN] = " o ";
char regionlist_number[MAX_REGL_LEN] = " %2d. ";

char readin_mesg[NSTRING];

int running_function = 0;
char *current_command = NULL;

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

char *userproc_arg = NULL;

int comline_processing = 1;

char *force_status = "UNSET";

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

/* A system-wide mark for temporarily saving the current location.
 * p MUST be reset to NULL after every restore!!!
 */
sysmark_t sysmark = { NULL, 0 };

/* Stored as s + ns, but $brkt_ms works in ms */
struct timespec pause_time = { 0, 200000000 };

/* Where macro pins hang out */

linked_items *macro_pin_headp = NULL;
