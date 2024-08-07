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

#include <string.h>
#include <utf8proc.h>
#include <time.h>

#include "dyn_buf.h"

/* FreeBSD (cclang) doesn't have strdupa(), so make our own....
 * ...or rather copy the GCC definition from string.h.
 * Hence this is here, as we've elready included <string.h>
 * If anything has strndupa as something other than a #define
 * this logic will need to be changed.
 */
#ifndef strdupa
# define strdupa(s)                                     \
  (__extension__                                        \
    ({                                                  \
      const char *__old = (s);                          \
      size_t __len = strlen (__old) + 1;                \
      char *__new = (char *) __builtin_alloca (__len);  \
      (char *) memcpy (__new, __old, __len);            \
    }))
#endif

/* Define allocation sizes and types here, so that globals.c gets them */

#define KRING_SIZE 10
#define MAX_REGL_LEN 16

typedef struct {
    struct buffer *main_bp;     /* curbp of arriving window */
    struct window *main_wp;     /* curwp of arriving window */
    struct window *wheadp;      /* wheadp of arriving window */
    int mbdepth;                /* Current depth of minibuffer */
} mb_info_st;

typedef struct {
    char *funcname;             /* Name of function */
    int keystroke;              /* Keystroke(s) use to reach it */
}  not_in_mb_st;

typedef struct {
    char *preload;              /* text to preload into getstring() result */
    int update;                 /* Set to make getstring() update its prompt */
    db_dcl(prompt);             /* The new prompt to use */
} prmpt_buf_st;

typedef struct {
    int c;              /* Last command char (reexecute) */
    int f;              /* Last flag (-> n valid) */
    int n;              /* Last n */
} com_arg;

typedef struct {
    fn_t func;          /* Function that would be called */
    com_arg ca;
} func_arg;

typedef struct {
    int C;
    int R;
    int W;
    int X;
} meta_spec_flags_t;

typedef struct {
    struct line *p;
    int o;
} sysmark_t;

typedef struct {
    char *current;
    char *parent;
    char *home;
    int clen;
    int plen;
    int hlen;
} udir_t;

/* For einit and whlist data stashed away around docmd() calls we need
 * to allow for the fact the the command might execute a buffer and hence
 * come back here.
 * So we need to save these items in a linked list and allocate/free it
 * as we go.
 * We use the same linked_items structure for the macro pins, so
 * define it all here.
 */
struct _li {
    struct _li *next;
    void *item;
};
typedef struct _li linked_items;

/* We keep a linked list of per-macro-level pins and also
 * need to keep a level marker for them.
 */
struct mac_pin {
    struct buffer *bp;
    struct line *lp;
    int offset;
    int mac_level;
    int col_offset;     /* Temporary use by p-m-l_get_columns/set_offsets */
};

/* This make the macro pin code easier to follow */

#define mmi(lip, what) (((struct mac_pin *)((lip)->item))->what)

enum yank_type { None, NormalYank, MiniBufferYank };
enum yank_style { Old, GNU };

#ifndef GLOBALS_C

extern linked_items *macro_pin_headp;

/* Initialized global external declarations. */

extern int fillcol;             /* Fill column                  */
extern int *kbdm;               /* Holds keyboard macro data    */
extern int n_kbdm;              /* Allocated size of kbdm       */
extern const char *execstr;     /* pointer to string to execute */
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
extern int gfcolor;             /* global forgrnd color (white) */
extern int gbcolor;             /* global backgrnd color (black) */
extern int gasave;              /* global ASAVE size            */
extern int gacount;             /* count until next ASAVE       */
extern int sgarbf;              /* State of screen unknown      */
extern int mpresf;              /* Stuff in message line        */
extern int clexec;              /* command line execution flag  */
extern int discmd;              /* display command flag         */
extern int disinp;              /* display input characters     */
extern int vismac;              /* update display during keyboard macros? */
extern int filock;              /* Do we want file-locking */
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
extern struct line *fline;      /* dynamic return line */
extern int rval;                /* return value of a subprocess */
extern int overlap;             /* line overlap in forw/back page */
extern int scrolljump;          /* number of lines to jump on scroll */

/* Uninitialized global external declarations. */

extern int currow;              /* Cursor row                   */
extern int curcol;              /* Cursor column                */
extern int com_flag;            /* Command flags                */
extern int curgoal;             /* Goal for C-P, C-N            */
extern struct window *curwp;    /* Current window               */
extern struct buffer *curbp;    /* Current buffer               */
extern struct window *wheadp;   /* Head of list of windows      */
extern struct buffer *bheadp;   /* Head of list of buffers      */
extern struct buffer *blistp;   /* Buffer for C-X C-B           */
extern struct buffer *bdbgp;    /* Buffer for macro debug info  */

extern db_dcl(pat);             /* Search pattern.              */
extern db_dcl(tap);             /* Reversed pattern array.      */
extern db_dcl(rpat);            /* Replacement pattern.         */

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
extern db_dcl(savnam);          /* Saved buffer name */
extern int do_savnam;           /* Whether to save buffer name */

extern int allow_current;       /* Look in current dir for startup? */

extern int chg_width;           /* Changed width on SIGWINCH */
extern int chg_height;          /* Changed height on SIGWINCH */

extern char *input_waiting;     /* Input ready (for execproc) */

extern int keytab_alloc_ents;   /* Allocated number of keytab entries */

extern struct buffer *ptt;      /* Global pt table */
extern int no_newline_in_pttex; /* Does replacement end in newline? */

extern int hscroll;             /* TRUE when we are scrolling horizontally */
extern int hjump;               /* How much to jump on horizontal scroll */
extern int autodos;             /* Auto-detect DOS file on read if set */
extern int showdir_tokskip;     /* Tokens to skip in showdir parsing */
extern int uproc_opts;          /* Flags for &arg handling */

extern const char kbdmacro_buffer[];    /* Name of the keyboard macro buffer */
extern struct buffer *kbdmac_bp;    /* keyboard macro buffer */

extern int run_filehooks;       /* Set when we want them run */

extern mb_info_st mb_info;
extern not_in_mb_st not_in_mb;
extern char *not_interactive_fname;

extern int pause_key_index_update;  /* If set, don't update keytab index */

extern prmpt_buf_st prmpt_buf;

extern enum yank_type last_yank;
extern enum yank_style yank_mode;

extern int autoclean;

extern char regionlist_text[MAX_REGL_LEN];
extern char regionlist_number[MAX_REGL_LEN];

extern db_dcl(readin_mesg);

extern int running_function;
extern char *current_command;

extern func_arg f_arg, p_arg;
extern struct rx_mask rx_mask[];
extern int rxargs;

/* To allow macro procedures to get args from C-code */

extern const char *userproc_arg;

/* Are we still processing the command line? */
extern int comline_processing;

/* The real status of the last command !force'd in a buffer macro */
extern char *force_status;

/* The default Equiv function handler */
extern utf8proc_uint8_t *(*equiv_handler)(const utf8proc_uint8_t *);

/* The buffer used for the last search */

extern struct buffer *group_match_buffer;

/* Set over a mlreply() call if we don't want getstring to record
 * its result into the keyboard macro buffer (//kbd_macro).
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

extern meta_spec_flags_t meta_spec_active;

/* GGR option flag bits */
extern int ggr_opts;

extern struct timespec pause_time;  /* Bracket match pause time */

/* A system-wide mark for temporarily saving the current locaiton. */

extern sysmark_t sysmark;

/* Three locations worked out once at the start/
 * All malloc'ed.
 */
extern udir_t udir;

extern int pretend_size;

/* A global db, for use in localized code.
 * Must NOT be used in calls to a function which might use it itself!!!
 */
extern db_dcl(glb_db);

/* Crypt bits */

extern int crypt_mode;          /* Type of crypt to use */
extern char gl_enc_key[];       /* Global key */
extern int gl_enc_len;          /* Global key length */

/* Crypt code modes */
#define CRYPT_MOD95     0x1000  /* Use the mod95 code */
#define CRYPT_ONLYP     0x2000  /* Only work on printing chars */
#define CRYPT_GLOBAL    0x4000  /* Use a global encryption key */

/* Key setup mode */
#define CRYPT_RAW         1
#define CRYPT_FILL63      2
#define CRYPT_MODEMASK 0x03     /* Cover all bits used in *setup* modes */

/* Which bits may be set in crypt_mode */

#define CRYPT_VALID (CRYPT_MOD95|CRYPT_ONLYP|CRYPT_GLOBAL|CRYPT_MODEMASK)

#endif  /* GLOBALS_C */

#endif  /* EDEF_H_ */
