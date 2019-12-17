#include "estruct.h"
#include "edef.h"

/* initialized global definitions */

int fillcol = 72;               /* Current fill column          */
int kbdm[NKBDM];                /* Macro                        */
char *execstr = NULL;           /* pointer to string to execute */
char golabel[NPAT] = "";        /* current line to go to        */
int execlevel = 0;              /* execution IF level           */
int eolexist = TRUE;            /* does clear to EOL exist      */
int revexist = FALSE;           /* does reverse video exist?    */
int flickcode = FALSE;          /* do flicker supression?       */
char *mode2name[] = {           /* Display name of modes        */
                                /* Also text when checking them */
        "Wrap",  "Cmode", "Phon",  "Exact", "View",
        "Over",  "Magic", "Crypt", "Asave", "eQuiv", "Dos",
};
char modecode[] = "WCPEVOMYAQD";/* letters to represent modes   */
int gmode = 0;                  /* global editor mode           */
int gflags = GFREAD;            /* global control flag          */
#if IBMPC
int gfcolor = 8;                /* global forgrnd color (white) */
#else
int gfcolor = 7;                /* global forgrnd color (white) */
#endif
int gbcolor = 0;                /* global backgrnd color (black) */
int gasave = 256;               /* global ASAVE size            */
int gacount = 256;              /* count until next ASAVE       */
int sgarbf = TRUE;              /* TRUE if screen is garbage    */
int mpresf = FALSE;             /* TRUE if message in last line */
int clexec = FALSE;             /* command line execution flag  */
int mstore = FALSE;             /* storing text to macro flag   */
int discmd = TRUE;              /* display command flag         */
int disinp = TRUE;              /* display input characters     */
struct buffer *bstore = NULL;   /* buffer to store macro text to */
int ttrow = HUGE;               /* Row location of HW cursor */
int ttcol = HUGE;               /* Column location of HW cursor */
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
#if IBMPC
            , "HIGH"
#endif
};
struct kill *kbufp = NULL;      /* current kill buffer chunk pointer    */
struct kill *kbufh[] = {[0 ... KRING_SIZE-1] = NULL};
                                /* kill buffer header pointers          */
int kused[] = {[0 ... KRING_SIZE-1] = KBLOCK};
                                /* # of bytes used in kill buffer       */
struct window *swindow = NULL;  /* saved window pointer                 */
int cryptflag = FALSE;          /* currently encrypting?                */
int *kbdptr;                    /* current position in keyboard buf */
int *kbdend = kbdm;             /* ptr to end of the keyboard */
int kbdmode = STOP;             /* current keyboard macro mode  */
int kbdrep = 0;                 /* number of repetitions        */
int restflag = FALSE;           /* restricted use?              */
int lastkey = 0;                /* last keystoke                */
int seed = 0;                   /* random number seed           */
int macbug = FALSE;             /* macro debuging flag          */
char errorm[] = "ERROR";        /* error literal                */
char truem[] = "TRUE";          /* true literal                 */
char falsem[] = "FALSE";        /* false litereal               */
int cmdstatus = TRUE;           /* last command status          */
char palstr[49] = "";           /* palette string               */
int saveflag = 0;               /* Flags, saved with the $target var */
int rval = 0;                   /* return value of a subprocess */
int nullflag = FALSE;           /* accept null characters */
int overlap = 0;                /* line overlap in forw/back page */
int scrolljump = 0;             /* no. lines to scroll (0 == centre screen) */

/* uninitialized global definitions */

int currow;                     /* Cursor row                   */
int curcol;                     /* Cursor column                */
int thisflag;                   /* Flags, this command          */
int lastflag;                   /* Flags, last command          */
int curgoal;                    /* Goal for C-P, C-N            */
struct window *curwp;           /* Current window               */
struct buffer *curbp;           /* Current buffer               */
struct window *wheadp;          /* Head of list of windows      */
struct buffer *bheadp;          /* Head of list of buffers      */
struct buffer *blistp;          /* Buffer for C-X C-B           */

char sres[NBUFN];               /* current screen resolution    */
/* GGR - Add one to these three to allow for trailing NULs      */
char pat[NPAT+1];               /* Search pattern               */
char tap[NPAT+1];               /* Reversed pattern array.      */
char rpat[NPAT+1];              /* replacement pattern          */

struct line *fline;             /* dynamic return line */

/* The variable matchlen holds the length of the matched
 * string - used by the replace functions.
 * The variable patmatch holds the string that satisfies
 * the search command.
 * The variables matchline and matchoff hold the line and
 * offset position of the *start* of match.
 */
unsigned int matchlen = 0;
unsigned int mlenold = 0;
char *patmatch = NULL;
struct line *matchline = NULL;
int matchoff = 0;

/* directive name table:
        This holds the names of all the directives....  */

char *dname[] = {
        "if", "else", "endif",
        "goto", "return", "endm",
        "while", "endwhile", "break",
        "force"
        , "finish"              /* GGR */
};

#if     DEBUGM
/*      vars needed for macro debugging output  */
char outline[NSTRING];          /* global string to hold debug line text */
#endif

/* GGR - Additional initializations */
int  inreex          = FALSE;

int  allow_current   = 0;
unicode_t *eos_list  = NULL;
int  inmb            = FALSE;
int  pathexpand      = TRUE;
char savnam[NBUFN]   = "main";
int do_savnam        = 1;

int  silent          = FALSE;
int  using_ggr_style = 0;

int chg_width        = 0;
int chg_height       = 0;

char *input_waiting  = NULL;

int keytab_alloc_ents = 0;

struct buffer *ptt = NULL;

int hscroll = FALSE;
int hjump = 1;
int autodos = TRUE;     /* Default is to do the check */
int showdir_tokskip = -1;

const char kbdmacro_buffer[] = "//kbd_macro";
struct buffer *kbdmac_bp = NULL;

int run_filehooks = 0;

mb_info_st mb_info = { NULL, NULL, NULL, 0 };

not_in_mb_st not_in_mb = { NULL, 0 };

int pause_key_index_update = 0;

int in_search_prompt = 0;

prmpt_buf_st prmpt_buf = { NULL, 0, "" };

enum yank_type last_yank = None;

enum yank_style yank_mode = Old;

int autoclean = 7;

char regionlist_text[] = " o ";
char regionlist_number[] = " %2d. ";

char readin_mesg[];

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
