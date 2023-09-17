/*      edef.h
 *
 *      Global variable definitions
 *
 *      written by Dave G. Conroy
 *      modified by Steve Wilhite, George Jones
 *      greatly modified by Daniel Lawrence
 *      modified by Petri Kutvonen
 */
#ifndef EDEF_H_
#define EDEF_H_

#include <stdlib.h>
#include <string.h>
#include <utf8proc.h>
#include "utf8.h"

/* Initialized global external declarations. */

extern int fillcol;             /* Fill column                  */
extern int kbdm[];              /* Holds kayboard macro data    */
extern char *execstr;           /* pointer to string to execute */
extern int eolexist;            /* does clear to EOL exist?     */
extern int revexist;            /* does reverse video exist?    */
extern int flickcode;           /* do flicker supression?       */
extern char *mode2name[];       /* text names of modes          */
extern char modecode[];         /* letters to represent modes   */
extern struct key_tab *keytab;  /* key bind to functions table  */
extern int key_index_valid;     /* Whether idx index for keytab is valid */
extern struct name_bind names[];/* name to function table */
extern int gmode;               /* global editor mode           */
extern int force_mode_on;       /* modes forced to be on        */
extern int force_mode_off;      /* modes forced to be off       */
extern int gflags;              /* global control flag          */
extern int gfcolor;             /* global forgrnd color (white) */
extern int gbcolor;             /* global backgrnd color (black) */
extern int gasave;              /* global ASAVE size            */
extern int gacount;             /* count until next ASAVE       */
extern int sgarbf;              /* State of screen unknown      */
extern int mpresf;              /* Stuff in message line        */
extern int clexec;              /* command line execution flag  */
extern int mstore;              /* storing text to macro flag   */
extern int discmd;              /* display command flag         */
extern int disinp;              /* display input characters     */
extern int vismac;              /* update display during keyboard macros? */
extern int filock;              /* Do we want file-locking */
extern struct buffer *bstore;   /* buffer to store macro text to */
extern int ttrow;               /* Row location of HW cursor */
extern int ttcol;               /* Column location of HW cursor */
extern int lbound;              /* leftmost column of current line
                                   being displayed */
extern int metac;               /* current meta character */
extern int ctlxc;               /* current control X prefix char */
extern int reptc;               /* current universal repeat char */
extern int abortc;              /* current abort command char   */

extern int tabmask;
extern char *cname[];           /* names of colors              */
extern struct kill *kbufp;      /* current kill buffer chunk pointer */
#define KRING_SIZE 10
extern struct kill *kbufh[KRING_SIZE];
                                /* kill buffer header pointers  */
extern int kused[KRING_SIZE];   /* # of bytes used in this kill buffer */
extern struct window *swindow;  /* saved window pointer         */
extern int cryptflag;           /* currently encrypting?        */
extern int *kbdptr;             /* current position in keyboard buf */
extern int *kbdend;             /* ptr to end of the keyboard */
extern int kbdmode;             /* current keyboard macro mode  */
extern int kbdrep;              /* number of repetitions        */
extern int restflag;            /* restricted use?              */
extern int lastkey;             /* last keystoke                */
extern int macbug;              /* macro debugging flag         */
extern int macbug_off;          /* macro debug global-off flag  */
extern char errorm[];           /* error literal                */
extern char truem[];            /* true literal                 */
extern char falsem[];           /* false litereal               */
extern int cmdstatus;           /* last command status          */
extern char palstr[];           /* palette string               */
extern int saveflag;            /* Flags, saved with the $target var */
extern struct line *fline;      /* dynamic return line */
extern int rval;                /* return value of a subprocess */
extern int nullflag;
extern int overlap;             /* line overlap in forw/back page */
extern int scrolljump;          /* number of lines to jump on scroll */

/* Uninitialized global external declarations. */

extern int currow;              /* Cursor row                   */
extern int curcol;              /* Cursor column                */
extern int thisflag;            /* Flags, this command          */
extern int lastflag;            /* Flags, last command          */
extern int curgoal;             /* Goal for C-P, C-N            */
extern struct window *curwp;    /* Current window               */
extern struct buffer *curbp;    /* Current buffer               */
extern struct window *wheadp;   /* Head of list of windows      */
extern struct buffer *bheadp;   /* Head of list of buffers      */
extern struct buffer *blistp;   /* Buffer for C-X C-B           */
extern struct buffer *bdbgp;    /* Buffer for macro debug info  */

extern char sres[NBUFN];        /* Current screen resolution.   */
extern char pat[];              /* Search pattern.              */
extern char tap[];              /* Reversed pattern array.      */
extern char rpat[];             /* Replacement pattern.         */

extern unsigned int srch_patlen;

extern char *dname[];           /* Directive name table.        */

/* Terminal table defined only in term.c */
extern struct terminal term;
/* Macros for setting t_nrow and derivatives */
#define SET_t_nrow(h) \
    term.t_nrow = h; term.t_mbline = h-1; term.t_vscreen = h-2;

/* GGR - Additional declarations */
extern int inreex;              /* Set when re-executing */

extern unicode_t *eos_list;     /* List of end-of-sentence characters */
extern int inmb;                /* Set when in minibuffer */
extern int pathexpand;          /* Whether to expand paths */
extern int silent;              /* Set for "no message line info" */
extern char savnam[];           /* Saved buffer name */
extern int do_savnam;           /* Whether to save buffer name */

extern int allow_current;       /* Look in current dir for startup? */

extern int chg_width;           /* Changed width on SIGWINCH */
extern int chg_height;          /* Changed height on SIGWINCH */

extern char *input_waiting;     /* Input ready (for execproc) */

extern int keytab_alloc_ents;   /* Allocated number of keytab entries */

extern struct buffer *ptt;      /* Global pt table */

extern int hscroll;             /* TRUE when we are scrolling horizontally */
extern int hjump;               /* How much to jump on horizontal scroll */
extern int autodos;             /* Auto-detect DOS file on read if set */
extern int showdir_tokskip;     /* Tokens to skip in showdir parsing */
extern int uproc_opts;          /* Flags for &arg handling */

extern const char kbdmacro_buffer[];    /* Name of the keyboard macro buffer */
extern struct buffer *kbdmac_bp;    /* keyboard macro buffer */

extern int run_filehooks;       /* Set when we want them run */

typedef struct {
    struct buffer *main_bp;     /* curbp of arriving window */
    struct window *main_wp;     /* curwp of arriving window */
    struct window *wheadp;      /* wheadp of arriving window */
    int mbdepth;                /* Current depth of minibuffer */
} mb_info_st;
extern mb_info_st mb_info;

typedef struct {
    char *funcname;             /* Name of function */
    int keystroke;              /* Keystroke(s) use to reach it */
}  not_in_mb_st;
extern not_in_mb_st not_in_mb;
extern char *not_interactive_fname;

extern int pause_key_index_update;  /* If set, don't update keytab index */

typedef struct {
    char *preload;              /* text to preload into getstring() result */
    int update;                 /* Set to make getstring() update its prompt */
    char prompt[NSTRING];       /* The new prompt to use */
} prmpt_buf_st;
extern prmpt_buf_st prmpt_buf;

enum yank_type { None, NormalYank, MiniBufferYank };
extern enum yank_type last_yank;

enum yank_style { Old, GNU };
extern enum yank_style yank_mode;

extern int autoclean;

#define MAX_REGL_LEN 16
extern char regionlist_text[MAX_REGL_LEN];
extern char regionlist_number[MAX_REGL_LEN];

extern char readin_mesg[NSTRING];

extern int running_function;
extern char *current_command;

typedef struct {
    int c;              /* Last command char (reexecute) */
    int f;              /* Last flag (-> n valid) */
    int n;              /* Last n */
} com_arg;
typedef struct {
    fn_t func;          /* Function that would be called */
    com_arg ca;
} func_arg;
extern func_arg f_arg, p_arg;
extern struct rx_mask rx_mask[];
extern int rxargs;

/* To allow macro procedures to get args from C-code */

extern char *userproc_arg;

/* Are we still processing the command line? */
extern int comline_processing;

/* The real status of the last command !force'd in a buffer macro */
extern char *force_status;

/* The default Equiv function handler */
extern utf8proc_uint8_t *(*equiv_handler)(const utf8proc_uint8_t *);

/* The buffer used for the last search */

extern struct buffer *group_match_buffer;

/* Set over a mlreply() call if we don't want getstring to record
 * it's result into the keyboard macro buffer (//kbd_macro).
 */
extern int no_macrobuf_record;

/* So mlforce() messages in key macro re-runs (ctxle) remain visible */

extern int mline_persist;

/* The buffer pointer to any current buffer running through dobuf() */

extern struct buffer *execbp;

/* Set to -1, 0, 1 according to whether a search string is OK for
 * back search, not ok, forward search
 */
extern int srch_can_hunt;

/* Counters (current, total) for repeating a user-procedure */

extern int uproc_lpcount, uproc_lptotal, uproc_lpforced;

/* Markers for META|SPEC handler being active. */

typedef struct {
    int C;
    int R;
    int W;
    int X;
} meta_spec_flags_t;
extern meta_spec_flags_t meta_spec_active;

/* GGR option flag bits */
extern int ggr_opts;

/* A system-wide mark for temporarily saving the current locaiton. */
typedef struct {
    struct line *p;
    int o;
} sysmark_t;
extern sysmark_t sysmark;

/* Crypt bits */

extern int crypt_mode;          /* Type of crypt to use */

/* Crypt code modes */
#define CRYPT_MOD95     0x1000  /* Use the mod95 code */
#define CRYPT_ONLYP     0x2000  /* Only work on printing chars */

/* Key setup mode */
#define CRYPT_RAW         1
#define CRYPT_FILL63      2
#define CRYPT_MODEMASK 0x03     /* Cover all bits used in *setup* modes */

#endif  /* EDEF_H_ */
