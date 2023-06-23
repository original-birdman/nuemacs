/*
 *      main.c

 *      uEmacs/PK 4.0
 *
 *      Based on:
 *
 *      MicroEMACS 3.9
 *      Written by Dave G. Conroy.
 *      Substantially modified by Daniel M. Lawrence
 *      Modified by Petri Kutvonen
 *
 *      MicroEMACS 3.9 (c) Copyright 1987 by Daniel M. Lawrence
 *
 *      Original statement of copying policy:
 *
 *      MicroEMACS 3.9 can be copied and distributed freely for any
 *      non-commercial purposes. MicroEMACS 3.9 can only be incorporated
 *      into commercial software with the permission of the current author.
 *
 *      No copyright claimed for modifications made by Petri Kutvonen.
 *
 *      This file contains the main driving routine, and some keyboard
 *      processing code.
 *
 * REVISION HISTORY:
 *
 * 1.0  Steve Wilhite, 30-Nov-85
 *
 * 2.0  George Jones, 12-Dec-85
 *
 * 3.0  Daniel Lawrence, 29-Dec-85
 *
 * 3.2-3.6 Daniel Lawrence, Feb...Apr-86
 *
 * 3.7  Daniel Lawrence, 14-May-86
 *
 * 3.8  Daniel Lawrence, 18-Jan-87
 *
 * 3.9  Daniel Lawrence, 16-Jul-87
 *
 * 3.9e Daniel Lawrence, 16-Nov-87
 *
 * After that versions 3.X and Daniel Lawrence went their own ways.
 * A modified 3.9e/PK was heavily used at the University of Helsinki
 * for several years on different UNIX, VMS, and MSDOS platforms.
 *
 * This modified version is now called eEmacs/PK.
 *
 * 4.0  Petri Kutvonen, 1-Sep-91
 *
 * 4.1  Ian Dunkin, Mike Arnautov and Gordon Lack ~1988->2017
 *      GGR tags...
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <stdarg.h>

#include "estruct.h" /* Global structures and defines. */
#include "edef.h"    /* Global definitions. */
#include "efunc.h"   /* Function declarations and name table. */
#include "ebind.h"   /* Default key bindings. */
#include "version.h"

#ifndef GOOD
#define GOOD    0
#endif

#if UNIX
#include <signal.h>
static void emergencyexit(int);
#endif

/* ======================================================================
 * GGR - list all options actually available!
 */
void usage(int status) {
#define NL "\n"

printf( \
"Usage: %s [options] [filename(s)]"                       NL \
"   Options:"                                             NL \
"      +            start at the end of file"             NL \
"      +<n>         start at line <n>"                    NL \
"      -a           process error file"                   NL \
"      -c<filepath> replacement for default rc file"      NL \
"      @<filepath>  deprecated legacy synonym for -c"     NL \
"      -d<dir>      directory holding rc and hlp files"   NL \
"      -e           edit file (default)"                  NL \
"      -f<modes>    force mode(s) on (uppercase) or off"  NL \
"                   valid for WCEVOMQDR modes"            NL \
"      -g<n>        go to line <n> (same as +<n>)"        NL \
"      -i           insecure mode - look in current dir"  NL \
"      -k<key>      encryption key"                       NL \
"      -m           message for mini-buffer at start-up"  NL \
"      -n           accept null chars (now always true)"  NL \
"      -r           restrictive use"                      NL \
"      -s<str>      initial search string"                NL \
"      -v           view only (no edit)"                  NL \
"      -x<filepath> an additional rc file"                NL \
"      -h,--help    display this help and exit"           NL \
"      -V,--version output version information and exit"  NL \
        , PROGRAM_NAME);
  exit(status);
}

/* KBD Macro in buffer code */

static char kbd_text[1024];
static int kbd_idx;
static int must_quote;
static int ctlxe_togo = 0;  /* This is for the kbdm[] version */

enum KBDM_direction { GetTo_KBDM, OutOf_KBDM };

/* ======================================================================
 * Internal routine to get to/from the keyboard macro buffer.
 * Remembers whence it came.
 */
static int kbdmac_buffer_toggle(enum KBDM_direction mode, char *who) {
    static enum KBDM_direction last_mode = OutOf_KBDM;  /* This must toggle */
    static struct buffer *obp;
    static int saved_nw;
    static struct window wsave;

    do_savnam = 0;
    switch(mode) {
    case GetTo_KBDM:
        if (last_mode == GetTo_KBDM) goto out_of_phase;
        obp = curbp;                    /* Save whence we came */
        wsave = *curwp;                 /* Structure copy - save */
        saved_nw = kbdmac_bp->b_nwnd;   /* Mark as "not viewed" */
        kbdmac_bp->b_nwnd = 0;
/* This code has to be able to reach the macro buffer even when
 * we are collecting macros...
 */
        if (!swbuffer(kbdmac_bp, 1)) {
            mlwrite("%s: cannot reach keyboard macro buffer!", who);
            kbdmac_bp->b_nwnd = saved_nw;
            kbdmode = STOP;
            ctlxe_togo = 0;     /* Just in case... */
            do_savnam = 1;
            return FALSE;
        }
        last_mode = GetTo_KBDM;
        return TRUE;
    case OutOf_KBDM:
        if (last_mode == OutOf_KBDM) goto out_of_phase;
        if (!swbuffer(obp, 0)) {
            mlwrite("%s: cannot leave keyboard macro buffer!", who);
            return FALSE;   /* and probably panic! */
        }
        *curwp = wsave;             /* Structure copy - restore */
        kbdmac_bp->b_nwnd = saved_nw;
        do_savnam = 1;
        last_mode = OutOf_KBDM;
        return TRUE;
    }
out_of_phase:                       /* Can only get here on error */
    mlforce("Keyboard macro collection out of phase - aborted.");
    do_savnam = 1;
    kbdmode = STOP;
    ctlxe_togo = 0;             /* Just in case... */
    return FALSE;
}

/* ======================================================================
 * Internal routine to create the keyboard-macro buffer.
 */
static int create_kbdmacro_buffer(void) {
    if ((kbdmac_bp = bfind(kbdmacro_buffer, TRUE, BFINVS)) == NULL) {
        mlwrite_one("Cannot create keyboard macro buffer!");
        return FALSE;
    }
    kbdmac_bp->b_type = BTPROC;     /* Mark the buffer type */
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "init")) return FALSE;
    linstr("write-message \"No keyboard macro yet defined\"");
    kbdmac_bp->b_flag &= ~BFCHG;    /* Mark as unchanged */
    return kbdmac_buffer_toggle(OutOf_KBDM, "init");
}

/* ======================================================================
 * Internal routine to set-up the keyboard macro buffer to collect a new
 * set of macro commands.
 */
static int start_kbdmacro(void) {
    if (!kbdmac_bp) {
        mlwrite_one("start: no keyboard macro buffer!");
        return FALSE;
    }

    bclear(kbdmac_bp);
    kbd_idx = must_quote = 0;
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "start")) return FALSE;
    linstr("; keyboard macro\n");
    return kbdmac_buffer_toggle(OutOf_KBDM, "start");
}

/* ======================================================================
 * Record a character to the keyboard macro buffer.
 * Needs to handle spaces and token()'s special characters.
 *
 * Would be nice to be able to count consecutive calls to certain
 * functions (forward-character, next-line, etc.) and just use
 * one call with a numeric arg.
 */
void addchar_kbdmacro(char addch) {

    char cc = addch & 0xff;
    char xc = 0;
    switch(cc) {        /* Handle what token in exec.c handles */
    case 8:   xc = 'b'; break;
    case 9:   xc = 't'; break;
    case 10:  xc = 'n'; break;
    case 12:  xc = 'f'; break;
    case 13:  xc = 'r'; break;
    case '~': xc = '~'; break;      /* It *does* handle this */
    case '"': xc = '"';             /* Falls through */
    case ' ': must_quote = 1; break;
    }
    if (xc != 0) {
        kbd_text[kbd_idx++] = '~';
        cc = xc;
    }
    kbd_text[kbd_idx++] = cc;
    return;
}

/* ======================================================================
 * Internal routine to flush any pending text so as to be insert raw.
 * Needs to handle spaces, token()'s special characters and NULs.
 */
    static void flush_kbd_text(void) {
    lnewline();
    linstr("insert-string ");
    if (must_quote) linsert_byte(1, '"');
    kbd_text[kbd_idx] = '\0';
/* This loop may look odd, but if we have NUL bytes to insert it means
 * that it works as if we haven't yet added enough it must be because
 * we've hit a NUL, so process that and continue on from there.
 */
    int added = 0;
    while(1) {
        linstr(kbd_text+added);
        added += strlen(kbd_text+added);
        if (added >= kbd_idx) break;
        if (must_quote) linsert_byte(1, '"');
        lnewline();
        linstr("macro-helper 0");
        lnewline();
        linstr("insert-string ");
        added++;
        if (must_quote) linsert_byte(1, '"');
    }
    if (must_quote) linsert_byte(1, '"');
    kbd_idx = must_quote = 0;
    return;
}

/* ======================================================================
 * Internal routine to record any numeric arg given to a macro function.
 */
static struct {
    int cnt;
    int valid;
} func_rpt = { 0, 0 };
static void set_narg_kbdmacro(int n) {
    func_rpt.cnt = n;
    func_rpt.valid = 1;
    return;
}

/* ======================================================================
 * Add a string to the keyboard macro buffer.
 */
int addto_kbdmacro(char *text, int new_command, int do_quote) {
    if (!kbdmac_bp) {
        mlwrite_one("addto: no keyboard macro buffer!");
        return FALSE;
    }
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "addto")) return FALSE;

/* If there is any pending text we need to flush it first */
    if (kbd_idx) flush_kbd_text();
    if (new_command) {
        lnewline();
        if (func_rpt.valid) {
            linstr(ue_itoa(func_rpt.cnt));
            linsert_byte(1, ' ');
            func_rpt.valid = 0;
        }
    }
    else linsert_byte(1, ' ');
    if (!do_quote) linstr(text);
    else {
        int qreq = 0;
/* We need to quote if the first char is an "active" character
 * or if the text contain any spaces or "s.
 */
        char *tp = text;
        switch(*tp) {
        case '"':
        case '!':
        case '@':
        case '#':
        case '$':
        case '&':
        case '*':
        case '.':
            qreq = 1;
        }
        if (!qreq) while (*tp) {
            if ((*tp == ' ') || (*tp == '"')) {
                qreq = 1;
                break;
            }
            tp++;
        }
        if (qreq) linsert_byte(1, '"');
        for (char *tp = text; *tp; tp++) {
            char cc = *tp & 0xff;
            char xc = 0;
            switch(cc) {
            case 8:   xc = 'b'; break;
            case 9:   xc = 't'; break;
            case 10:  xc = 'n'; break;
            case 12:  xc = 'f'; break;
            case 13:  xc = 'r'; break;
            case '"':                   /* It *does* handle these two! */
            case '~': xc = cc; break;
            }
            if (xc != 0) {
                linsert_byte(1, '~');
                cc = xc;
            }
            linsert_byte(1, cc);
	}
        if (qreq) linsert_byte(1, '"');
    }
    return kbdmac_buffer_toggle(OutOf_KBDM, "addto");
}

/* ======================================================================
 * Internal routine to finalize the keyboard macro buffer.
 * Also used to terminate running...
 */
static int end_kbdmacro(void) {
    if (!kbdmac_bp) {
        mlwrite_one("end: no keyboard macro buffer!");
        return FALSE;
    }
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "end")) return FALSE;

/* If there is any pending text we need to flush it first */
    if (kbd_idx) flush_kbd_text();
    lnewline();

/* If we were in PLAY mode, restore the current c/f/n-last.
 * Otherwise it gets lost if macro has aborted.
 * If we weren't in PLAY mode, report ending the macro.
 */
    if (kbdmode == PLAY) f_arg = p_arg;
    else                 mlwrite_one(MLbkt("End macro"));

    kbdmode = STOP;
/* Reset ctlxe_togo regardless of current state */
    ctlxe_togo = 0;
    return kbdmac_buffer_toggle(OutOf_KBDM, "end");
}

/* ======================================================================
 * Used to get the action for certain active characters into macros,
 * rather than just getting the raw character inserted.
 */
int macro_helper(int f, int n) {
    UNUSED(f);
    char tag[2];                        /* Just char + NULL needed */
    int status = mlreply("helper:", tag, 1, CMPLT_NONE);
    if (status != TRUE) return status;  /* Only act on +ve response */
    switch(tag[0]) {
    case '}':
    case ']':
    case ')':
        return insbrace(n, tag[0]);
    case '#':   return inspound();
    case '0':   return linsert_byte(n, 0);  /* Insert a NUL */
    }
    return FALSE;
}

/* ====================================================================== */
static char* called_as;


/* ===================== START OF BUFFER SAVING CODE ==================== */

#define Dumpdir_Name "uemacs-dumps"
#define Dump_Index "INDEX"
#define AutoClean_Buffer "//autoclean"

/* ======================================================================
 * Generate the time-tag string.
 * We assume that we don't get multiple dumps in the same second.
 */
static int ts_len = 0;
static char time_stamp[20]; /* There is only one time_stamp at a time... */
static int set_time_stamp(int days_back) {
    time_t t = time(NULL) - days_back*86400;;
    return strftime(time_stamp, 20, "%Y%m%d-%H%M%S.", localtime(&t));
}
static FILE *index_fp = NULL;  /* Avoid "may be used uninitialized" */
static int can_dump_files = 0;

/* ===================== START OF NUTRACE ONLY CODE ===================== */
/* We need NUTRACE to be able to handle stack dumps */
#ifdef NUTRACE

/* Implement a stack dump compile time option using libbacktrace. */

#include "backtrace.h"
#include "backtrace-supported.h"

struct bt_ctx {
    struct backtrace_state *state;
    int error;
};

FILE *stkdmp_fp = NULL;

/* ======================================================================
 * Print stack trace to stdout and the dump file, if open.
 */
void stk_printf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (stkdmp_fp) {
        va_start(ap, fmt);
        vfprintf(stkdmp_fp, fmt, ap);
        va_end(ap);
    }
    return;
}

/* ======================================================================
 * A set of callback routines to provide the stack-trace.
 */
static void syminfo_callback (void *data, uintptr_t pc,
     const char *symname, uintptr_t symval, uintptr_t symsize) {
    UNUSED(data); UNUSED(symval); UNUSED(symsize);
    if (symname) {
        stk_printf("0x%lx %s ??:0\n", (unsigned long)pc, symname);
    }
    else {
        stk_printf("0x%lx ?? ??:0\n", (unsigned long)pc);
    }
}
static void error_callback(void *data, const char *msg, int errnum) {
    struct bt_ctx *ctx = data;
    stk_printf("ERROR: %s (%d)", msg, errnum);
    ctx->error = 1;
}
static int full_callback(void *data, uintptr_t pc, const char *filename,
     int lineno, const char *function) {

    struct bt_ctx *ctx = data;
    if (function) {
        stk_printf("0x%lx %s %s:%d\n", (unsigned long)pc, function,
             filename? filename: "??", lineno);
    }
    else {
        backtrace_syminfo(ctx->state, pc, syminfo_callback,
             error_callback, data);
    }
    return 0;
}
/* ======================================================================
 * The entry point to the above callbacks.
 */
static int simple_callback(void *data, uintptr_t pc) {
    struct bt_ctx *ctx = data;
    backtrace_pcinfo(ctx->state, pc, full_callback, error_callback, data);
    return 0;
}

/* ======================================================================
 * Print the trace back info.
 * Tries to open a file in uemacs-dumps to contan it too and, if it does,
 * puts an entry for it into INDEX so that it will get autocleaned.
 */
static void do_stackdump(void) {

    char stack_dump_name[40];
    if (can_dump_files) {
        strcpy(stack_dump_name, time_stamp);
        strcat(stack_dump_name, "-StackDump");
        stkdmp_fp = fopen(stack_dump_name, "w");
        if (!stkdmp_fp) perror("Failed to open -StackDump");
    }
    if (index_fp) fprintf(index_fp, "%s <= //StackDump\n", stack_dump_name);

/* Even if the INDEX isn't open we still try the stack dump */

    stk_printf("Traceback:\n");

    struct backtrace_state *state =
        backtrace_create_state (called_as, BACKTRACE_SUPPORTS_THREADS,
             error_callback, NULL);
    struct bt_ctx ctx = {state, 0};
/* Start the stacktrace 3 levels back (ignore this routine,
 * the signal handler that called us and the signal thrower code).
 */
    backtrace_simple(state, 2, simple_callback, error_callback, &ctx);
    stk_printf("\n");
}
#endif
/* ===================== END OF NUTRACE ONLY CODE ======================= */

static char cwd[1024];
/* ======================================================================
 * get_to_dumpdir
 * get to the Dump_Dir - the return value is whether it makes it
 * Also open the INDEX file,if possible.
 */
int get_to_dumpdir(void) {

/* Get the current dir name - possibly needed for the index */

    char *dnc = getcwd(cwd, 1024);
    UNUSED(dnc);    /* To avoid "warn_unused_result" message */

/* Get to the dump directory */

    char *p;
    if ((p = getenv("HOME")) == NULL) {
        perror("Can't find HOME - no buffer saving:");
        return 0;
    }
    int status = chdir(p);
    if (status !=0) {
        perror("Can't get to HOME - no buffer saving:");
        return 0;
    }
    status = mkdir(Dumpdir_Name, 0700); /* Private by default */
    if (status != 0 && errno != EEXIST) {
        perror("Can't make HOME/" Dumpdir_Name " - no buffer saving: ");
        return 0;
    }
    status = chdir(Dumpdir_Name);
    if (status !=0) {
        perror("Can't get to HOME/" Dumpdir_Name " - no buffer saving:");
        return 0;
    }
    index_fp = fopen(Dump_Index, "a");
    if (!index_fp) perror("No " Dump_Index " update");
    return 1;   /* We *are* in DumpDir... */
}

/* ======================================================================
 * Scan through the buffers and look for modified ones associated
 * with files. For all it finds it tries to save them in ~/uemacs-dumps.
 * If any buffers are narrowed it widens them before dumping.
 * Since we are dying we don't preserve the current working dir.
 */
static void dump_modified_buffers(void) {

    int status;

    printf("Trying to save modified buffers\n");

/* Generate a time-tag for the dumped files.
 * We assume that we don't get multiple dumps in the same second to
 * the same user's HOME.
 */
    char tagged_name[NFILEN], orig_name[NFILEN];

/* Scan the buffers */

    int index_open = 0;
    int do_index = 0;
    int add_cwd;

    for(struct buffer *bp = bheadp; bp != NULL; bp = bp->b_bufp) {
        if ((bp->b_flag & BFCHG) == 0     /* Not changed...*/
         || (bp->b_flag & BFINVS) != 0    /* ...not real... */
         || (bp->b_mode & MDVIEW) != 0    /* ...read-only... */
         || (bp->b_fname[0] == '\0')) {   /* ...has no filename */

/* Not interested ... but we *do* want to save any keyboard-macro.
 * which is marked as invisible.
 */
            if (bp == kbdmac_bp && (bp->b_flag & BFCHG) != 0) {
                strcpy(bp->b_fname, kbdmacro_buffer); /* Give it a value */
            }
            else {
                continue;
            }
        }

        curbp = bp;     /* Needed for functionality calls */
        if ((bp->b_flag & BFTRUNC) != 0) {
            printf("Skipping %s - truncated\n", bp->b_fname);
            continue;
        }
        if ((bp->b_flag & BFNAROW) != 0) {
            printf("Widening %s...\n", bp->b_bname);
            curwp->w_bufp = bp;         /* widen() widens this one */
            if (widen(0, 0) != TRUE) {
                printf(" - failed - skipping\n");
                continue;
            }
            else
                printf("\n");
        }
        printf("Saving %s...\n", bp->b_bname);
/* Get the filename from any pathname */
        char *fn = strrchr(bp->b_fname, '/');
        if (fn == NULL) {       /* Just a filename */
            fn = bp->b_fname;
            add_cwd = 1;
        }
        else {
            fn++;               /* Skip over the / */
            add_cwd = 0;
        }
        strcpy(tagged_name, time_stamp);
        strncpy(tagged_name+ts_len, fn, NFILEN-ts_len-1);
        tagged_name[NFILEN-1] = '\0';   /* Ensure a terminating NUL */
        strcpy(orig_name, bp->b_fname); /* For INDEX */
/* Just in case the user has opened a file "kbd_macro", we alter the
 * tagged name to have a ! for this special case, to avoid any
 * name clash.
 */
        if (curbp == kbdmac_bp) tagged_name[ts_len-1] = '!';
        strcpy(bp->b_fname, tagged_name);
        if ((status = filesave(0, 0)) != TRUE) {
            printf("  failed - skipping\n");
            continue;
        }
        else
            printf("\n");
        if (!index_open) {
            index_open = 1;                 /* Don't try again */
            index_fp = fopen(Dump_Index, "a");
            if (index_fp == NULL) {
                perror("No " Dump_Index " update");
            }
            else {
                do_index = 1;
            }
        }
        if (do_index) {
            char *dir, *sep;
            if (add_cwd) {
                dir = cwd;
                sep = "/";
            }
            else {
                dir = "";
                sep = "";
            }
            fprintf(index_fp, "%s <= %s%s%s\n", tagged_name, dir, sep,
                  orig_name);
        }
    }
    if (do_index) fclose(index_fp);
    return;
}

/* ======================================================================
 * Auto-clean the directory where modified buffers are dumped on
 * receiving a signal.
 * Run at start-up.
 * Note that we MUST NOT change our current directory!
 * Coded using fchdir, as openat and unlinkat are not available on
 * some old Linux systems (Oleg's mips distro).
 */
void dumpdir_tidy(void) {
    int status;

    struct buffer *saved_bp = curbp;
    struct buffer *auto_bp = bfind(AutoClean_Buffer, TRUE, BFINVS);
    if (auto_bp == NULL) {  /* !?! */
        mlwrite("Failed to find %s!\n", AutoClean_Buffer);
        return;
    }
    if (!swbuffer(auto_bp, 0)) {
        mlwrite("Failed to get to %s!\n", AutoClean_Buffer);
        goto revert_buffer;
    }
/* A -ve days_allowed means "never". */
    if (autoclean < 0) {
        mlwrite("autoclean -ve (%d). No cleaning done\n");
        goto revert_buffer;
    }

    char info_message[4096]; /* Hopefully large enough */

/* Open the current directory on a file-descriptor for ease of return */
    int start_fd = open(".", O_DIRECTORY);
    if (start_fd < 0) {
        snprintf(info_message, 4096,
              "Can't open current location: %s\n", strerror(errno));
        linstr(info_message);
        goto revert_buffer;
    }

/* Now get HOME and go to the dumpdir there */
    char *p;
    if ((p = getenv("HOME")) == NULL) {
        linstr("Can't find HOME - no auto-tidy:\n");
        goto revert_buffer;
    }

    char dd_name[2048]; /* Hopefully large enough */
    strcpy(dd_name, p);
    strcat(dd_name, "/");
    strcat(dd_name, Dumpdir_Name);
    if (chdir(dd_name) < 0) {
        snprintf(info_message, 4096,
              "Can't get to ~/%s: %s\n", Dumpdir_Name, strerror(errno));
        linstr(info_message);
        goto close_start_fd;
    }

/* Open file read/write using "r+". "a+" always writes at the end!! */

    FILE *index_fp = fopen(Dump_Index, "r+");
    if (index_fp == NULL) {
        snprintf(info_message, 4096,
              "Can't open ~/%s/" Dump_Index ": %s\n",
              Dumpdir_Name, strerror(errno));
        linstr(info_message);
        goto revert_to_start_fd;
    }

/* We now have the index open...
 * Read through it until a line is greater than the set time_stamp
 * deleting files as we go.
 */
    char *lp = NULL;
    size_t blen = 0;
    ts_len = set_time_stamp(autoclean);
    long rewrite_from = 0;             /* Only if there is a deletion */
    while (getline(&lp, &blen, index_fp) >= 0) {
        if (strncmp(lp, time_stamp, ts_len - 1) < 0) { /* Too old... */
/* Original filename is for reporting ONLY */
            char *orig_fn = strstr(lp, " <= ");
            if (!orig_fn) continue;     /* Just in case... */
            int dfn_len = orig_fn - lp; /* Length of filename */
            *(lp+dfn_len) = '\0';       /* Null terminate it */
            orig_fn += 4;               /* Step over " <= " for original */
            status = unlink(lp);
            if (status) {
                snprintf(info_message, 4096,
                      "Delete of %s (<= %s)failed: %s\n", lp, orig_fn,
                      strerror(errno));
                linstr(info_message);
            }
            else {
                snprintf(info_message, 4096,
                      "Deleted %s (<= %s)\n", lp, orig_fn);
                linstr(info_message);
            }
            rewrite_from = ftell(index_fp);
        }
        else
            break;
    }

/* We need to copy the unused end of the INDEX to the start */
    if (rewrite_from) {
        long new_offs = 0;
        int done = 0;
        while (!done && (fseek(index_fp, rewrite_from, SEEK_SET) == 0)) {
            char cbuf[4096];
            size_t rc = fread(cbuf, sizeof(char), sizeof(cbuf), index_fp);
            if (feof(index_fp)) {
                done = 1;
                clearerr(index_fp);
            }
            else {                  /* Update where we read from */
                rewrite_from = ftell(index_fp);
            }
            if (rc) {
                status = fseek(index_fp, new_offs, SEEK_SET);
                size_t wc = fwrite(cbuf, sizeof(char), rc, index_fp);
                new_offs += wc;
                if (wc != rc) break;    /* We have a problem... */
                status = fflush(index_fp);       /* See man fopen */
            }
        }
        status = ftruncate(fileno(index_fp), new_offs);
    }

/* Close files and free buffers */

    if (fclose(index_fp)) {
        snprintf(info_message, 4096,
              Dump_Index " rewrite error: %s\n", strerror(errno));
        linstr(info_message);
    }
    if (lp) Xfree(lp);

revert_to_start_fd:
    status = fchdir(start_fd);
    if (status < 0) mlforce("Stuck in dump dir!!!");
close_start_fd:
    close(start_fd);
revert_buffer:
    swbuffer(saved_bp, 0);  /* Assume it succeeds... */
    zotbuf(auto_bp);
    return;
}

/* ======================================================================
 * Signal handler which prints a stack trace (if configured at compile time)
 * and also attempts to save all modified buffers.
 */
void exit_via_signal(int signr) {

/* We have to go back to the normal screen for the printing, otherwise
 * it gets cleared at exit.
 * Also set up stdout to autoflush, so that it interlaces correctly
 * with stderr.
 * NOTE: some uemacs functions (widen(), filesave()) will still
 * be printing mini-buffer message (via mlwrite()). This seems to be OK
 * (it uses screen positioning, but always to the bottom row), so
 * it has been left running. There are newline prints after such calls to
 * switch to the next output line.
 */
    TTclose();
    fflush(stdout);
    setvbuf(stdout, NULL, _IONBF, 0);

/* Let's go through the steps order...various globals get set here... */

    ts_len = set_time_stamp(0);             /* Set it for "now" */
    can_dump_files = get_to_dumpdir();      /* Also opens INDEX */

/* Possibly a stack dump */
#if defined(NUTRACE)
    do_stackdump();
#endif

/* Try to save any modified files. */
    if (can_dump_files) dump_modified_buffers();

#if !defined(NUTRACE)
#define stk_printf printf
#endif
    stk_printf("%s: signal %s seen.\n", called_as, strsignal(signr));

#if defined(NUTRACE)
    if (stkdmp_fp) fclose(stkdmp_fp);
#endif
    if (index_fp) fclose(index_fp);
    exit(signr);
}
/* ===================== END OF BUFFER SAVING CODE ====================== */

/* ====================================================================== */
/* multiplier_check
 * Handler for Esc-nnn sequences denoting command multiplier
 * After this has run:
 *  If the user did not gave any numeric arg, f will be FALSE and n = 1
 *  If the user gave a numeric arg, f will be TRUE and n = result.
 */
com_arg *multiplier_check(int c) {

    static com_arg ca;  /* Caller will copy out, so we only need one */
    int mflag;          /* negative flag on repeat */
    int basec;          /* c stripped of meta character */

    ca.c = c;           /* Initialize... */
    ca.f = FALSE;       /* ...these... */
    ca.n = 1;           /* ...3 now */

/* Do META-# processing if needed */

    basec = ca.c & ~META;       /* strip meta char off if there */
    if ((ca.c & META) && ((basec >= '0' && basec <= '9') || basec == '-')) {
        ca.f = TRUE;            /* there is a # arg */
        ca.n = 0;               /* start with a zero default */
        mflag = 1;              /* current minus flag */
        ca.c = basec;           /* strip the META */
        while ((ca.c >= '0' && ca.c <= '9') || (ca.c == '-')) {
            if (ca.c == '-') {  /* already hit a minus or digit? */
                if ((mflag == -1) || (ca.n != 0)) break;
                mflag = -1;
            }
            else {
                ca.n = ca.n * 10 + (ca.c - '0');
            }
            if ((ca.n == 0) && (mflag == -1))  /* lonely - */
                mlwrite_one("Arg:");
            else
                mlwrite("Arg: %d", ca.n * mflag);

            ca.c = getcmd();    /* get the next key */
        }
        ca.n = ca.n * mflag;    /* figure in the sign */
    }

/* Do ^U repeat argument processing */

    if (ca.c == reptc) {    /* ^U, start argument   */
        ca.f = TRUE;
        ca.n = 4;           /* with argument of 4 */
        mflag = 0;          /* that can be discarded. */
        mlwrite_one("Arg: 4");
        while (((ca.c = getcmd()) >= '0' && ca.c <= '9') ||
                 ca.c == reptc || ca.c == '-') {
            if (ca.c == reptc)
/* This odd-looking statement is checking for integer overflow by testing
 * that the sign of the result would be the same sign as the starting value.
 */
                if ((ca.n > 0) == ((ca.n * 4) > 0))
                    ca.n = ca.n * 4;
                else
                    ca.n = 1;
/* If dash, and start of argument string, set arg.
 * to -1.  Otherwise, insert it.
 */
            else if (ca.c == '-') {
                if (mflag) break;
                ca.n = 0;
                mflag = -1;
            }
/* If first digit entered, replace previous argument
 * with digit and set sign.  Otherwise, append to arg.
 */
            else {
                if (!mflag) {
                    ca.n = 0;
                    mflag = 1;
                }
                ca.n = 10 * ca.n + ca.c - '0';
            }
            mlwrite("Arg: %d", (mflag >= 0) ? ca.n : (ca.n ? -ca.n : -1));
        }
/* Make arguments preceded by a minus sign negative and change
 * the special argument "^U -" to an effective "^U -1".
 */
        if (mflag == -1) {
            if (ca.n == 0) ca.n++;
            ca.n = -ca.n;
        }
    }
    return &ca;             /* Give back the result */
}

static char *rcfile = NULL;     /* GGR non-default rc file */
int set_rcfile(char *fname) {
    if (rcfile) {
        fprintf(stderr, "You cannot have two startup files (-c/@)!\n");
        return FALSE;
    }
    rcfile = Xmalloc(NFILEN);
    strcpy(rcfile, fname);
    return TRUE;
}

int main(int argc, char **argv) {
    int c = -1;             /* command character */
    struct buffer *bp;      /* temp buffer pointer */
    int firstfile;          /* first file flag */
    struct buffer *firstbp = NULL;  /* ptr to first buffer in cmd line */
    int viewflag;           /* are we starting in view mode? */
    int gotoflag;           /* do we need to goto a line at start? */
    int gline = 0;          /* if so, what line? */
    int searchflag;         /* Do we need to search at start? */
    int saveflag;           /* temp store for lastflag */
    int errflag;            /* C error processing? */
    char bname[NBUFN];      /* buffer name of file to read */
    int cryptflag;          /* encrypting on the way in? */
    char ekey[NPAT];        /* startup encryption key */
    int newc;
    int verflag = 0;        /* GGR Flags -v/-V presence on command line */
    char *rcextra[10];      /* GGR additional rc files */
    unsigned int rcnum = 0; /* GGR number of extra files to process */

#if BSD
        sleep(1); /* Time for window manager. */
#endif

#if UNIX
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);

    sigact.sa_handler = sizesignal;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sigact, NULL);

/* Add a handler for other signals, which will exit */
    sigact.sa_handler = exit_via_signal;
    sigact.sa_flags = SA_RESETHAND; /* So we can't loop into our handler */
/* The SIGTERM is there so you can trace a loop by sending one */
    int siglist[] = { SIGBUS, SIGFPE, SIGSEGV, SIGTERM, SIGABRT };
    for (unsigned int si = 0; si < sizeof(siglist)/sizeof(siglist[0]); si++)
        sigaction(siglist[si], &sigact, NULL);
    called_as = argv[0];
/* The old one to get you out... */
    sigact.sa_handler = emergencyexit;
    sigaction(SIGHUP, &sigact, NULL);
#endif

/* GGR The rest of initialization is done after processing optional args */
    varinit();              /* initialise user variables */

    viewflag = FALSE;       /* view mode defaults off in command line */
    gotoflag = FALSE;       /* set to off to begin with */
    searchflag = FALSE;     /* set to off to begin with */
    firstfile = TRUE;       /* no file to edit yet */
    errflag = FALSE;        /* not doing C error parsing */
    cryptflag = FALSE;      /* no encryption by default */

/* Set up the initial keybindings.  Must be done early, before any
 * command line processing.
 * NOTE that we must initialize the namelookup indexing first, i.e. before
 * calling extend_keytab().
 * We need to allow for the additional ENDL_KMAP and ENDS_KMAP entries,
 * which mark the End-of-List and End-of-Structure, and round up to the
 * next KEYTAB_INCR boundary.
 */
    init_namelookup();
    int n_init_keys = sizeof(init_keytab)/sizeof(typeof(init_keytab[0]));
    int init_ents = n_init_keys + 2 + KEYTAB_INCR;
    init_ents /= KEYTAB_INCR;
    init_ents *= KEYTAB_INCR;
    extend_keytab(init_ents);

/* Set up the search ring buffers */

    init_search_ringbuffers();

/* GGR Command line parsing substantially reorganised. It now consists of two
 * separate loops. The first loop processes all optional arguments (command
 * keywords and associated options if any) and stops on reaching the first
 * specification of a file to edit. Along the way it collects specs of
 * initialisation files to process -- these are processed in the correct order
 * after this first loop. The second loop then deals with files nominated
 * by the user.
 */
    char *mbuf_mess = NULL;
    while (--argc) {
        argv++;         /* Point at the next token */
                        /* Process Switches */
        if (**argv == '+') {
            gotoflag = TRUE;
            gline = atoi(*argv + 1); /* skip the '+' */
        }
        else if (**argv == '-') {
            char *opt = NULL;
            char key1;
            char *arg = *argv + 1;
            if (*arg == '-') arg++; /* Treat -- as - */
/* Check for specials: help and version requests */
            if (*arg == '?' || strncmp("help", arg, strlen(arg)) == 0) {
                 usage(EXIT_FAILURE);
            }
            if (strncmp("version", arg, strlen(arg)) == 0)
                 verflag = strlen(arg);
            key1 = *arg;
/* Allow options to be given as separate tokens */
            if (strchr("CDFGKMSX", key1 & (char)~0x20)) {
                opt = *argv + 2;
                if (*opt == '\0' && argc > 0 && !strchr("-@", *(*argv + 1))) {
                    if (--argc <= 0) {
                        fprintf(stderr, "parameter missing for -%c\n", key1);
                        usage(EXIT_FAILURE);
                    }
                    opt = *(++argv);
                }
            }

 /* Process Startup macros - quick Uppercase */
            switch (key1 & (char)~0x20) {
            case 'A':       /* process error file */
                errflag = TRUE;
                break;
            case 'C':       /* GGR -c replacement of default rc */
/* ffropen() will expand any relative/~ pathname *IN PLACE* so
 * we need a copy of the command line option!
 */
                if (!set_rcfile(opt)) exit(1);
                break;
            case 'D':       /* GGR -d for config/help directory */
                set_pathname(opt);
                break;
            case 'E':       /* -e for Edit file */
                viewflag = FALSE;
                break;
            case 'F':       /* -f for mode forcing */
/* Braces to allow a local declaration */
               {char err_char = do_force_mode(opt);
                if (err_char) {
                    fprintf(stderr, "Invalid force mode: %c\n", err_char);
                    usage(EXIT_FAILURE);
                }
                break;
               }
            case 'G':       /* -g for initial goto */
                gotoflag = TRUE;
                gline = atoi(opt);
                break;
            case 'I':       /* -i for insecure mode */
                allow_current = 1;
                break;
            case 'K':       /* -k<key> for code key */
                /* GGR only if given a key.. */
                /* The leading ; *is* needed! */
                ;int olen = strlen(opt);
                if (olen > 0 && olen < NPAT) {
                    cryptflag = TRUE;
                     strcpy(ekey, opt);
                }
                break;
            case 'M':       /* -m message for mini-buffer */
                mbuf_mess = opt;
                break;
            case 'N':       /* -n accept null chars */
                nullflag = TRUE;
                break;
            case 'R':       /* -r restrictive use */
                restflag = TRUE;
                break;
            case 'S':       /* -s for initial search string */
                searchflag = TRUE;
                strncpy(pat, opt, NPAT);
/* GGR - set-up some more things for the FAST search algorithm */
                rvstrcpy(tap, pat);
                srch_patlen = strlen(pat);
                break;
            case 'V':       /* -v for View File */
                if (!verflag) verflag = 1;    /* could be version or */
                viewflag = TRUE;    /* view request */
                break;
            case 'X':       /* GGR: -x for eXtra rc file */
                if (rcnum < sizeof(rcextra)/sizeof(rcextra[0]))
/* ffropen() will expand any relative/~ pathname *IN PLACE* so
 * we need a copy of the command line option!
 */
                rcextra[rcnum] = Xmalloc(NFILEN);
                strcpy(rcextra[rcnum], opt);
                rcnum++;
                break;
            default:        /* unknown switch - ignore this for now */
                break;
            }

        }
        else if (**argv == '@') {   /* Same as -c. But not as as well as... */
            if (!set_rcfile(*argv + 1)) exit(1);
        }
        else break;
    }
    if ((verflag && argc == 0) || verflag > 1) {
        version();
        exit(EXIT_SUCCESS);
    }

/* Initialize the editor. */
    vtinit();               /* Display */
    edinit("main");         /* Buffers, windows */

/* Set this up before running init files */

    if (!create_kbdmacro_buffer()) {    /* Set this up */
        sleep(2);
        vttidy();
        exit(1);
    }

/* GGR - Now process initialisation files before processing rest of comline */
    silent = TRUE;
    if (!rcfile || !startup(rcfile)) startup("");
    if (rcfile) {
        Xfree(rcfile);
        rcfile = NULL;
    }
    if (rcnum) {
        for (unsigned int n = 0; n < rcnum; n++) {
            startup(rcextra[n]);
            Xfree(rcextra[n]);
        }
    }
    silent = FALSE;

/* If we are C error parsing... run it! */
    if (errflag) startup("error.cmd");

/* Process rest of comline, which is a list of files to edit */
    while (argc--) {

/* You can use -v/-e to toggle view mode on/off as you go through the
 * files on the command line.
 */
        if (strcmp(*argv, "-e") == 0 || strcmp(*argv, "-v") == 0) {
            viewflag = *(*(argv++) + 1) == 'v';
            continue;
        }
        if (strlen(*argv) >= NFILEN) {  /* Sanity check */
            fprintf(stderr, "filename too long!!\n");
            sleep(2);
            vttidy();
            exit(1);
        }

/* We need to check here whether this is a directory, as we don't want
 * to set up a buffer if it is.
 * This looks a bit like duplication of the getfile() code, but the
 * buffer-pointer handling is different.
 */
        if (showdir_handled(*argv) == FALSE) {

/* Set-up a (unique) buffer for this file */
            makename(bname, *argv, TRUE);
            bp = bfind(bname, TRUE, 0);     /* set this to inactive */
            strcpy(bp->b_fname, *argv);
            bp->b_active = FALSE;
            if (firstfile) {
                firstbp = bp;
                firstfile = FALSE;
            }
/* Set the modes appropriately */

            if (viewflag) bp->b_mode |= MDVIEW;
            if (cryptflag) {
                bp->b_mode |= MDCRYPT;
                bp->b_keylen = strlen(ekey);
                myencrypt((char *) NULL, 0);
                myencrypt(ekey, bp->b_keylen);
                memcpy(bp->b_key, ekey, bp->b_keylen);
            }
        }
        argv++;
    }

/* Run the autocleaner...MUST come after processing start-up files, as they
 * may set $autoclean.
 */
    dumpdir_tidy();

/* Done with processing command line */

    comline_processing = 0;
    discmd = TRUE;          /* P.K. */

/* If there are any files to read, read the first one! */
    int display_readin_msg = 0;
    bp = bfind("main", FALSE, 0);
    if (firstfile == FALSE && (gflags & GFREAD)) {
        swbuffer(firstbp, 0);   /* Assume it succeeds */
        zotbuf(bp);
        display_readin_msg = 1;
    }

/* Deal with startup gotos and searches */
    if (gotoflag && searchflag) {
        update(FALSE);
        mlwrite_one(MLbkt("Cannot search and goto at the same time!"));
    } else if (gotoflag) {
        if (gotoline(TRUE, gline) == FALSE) {
            update(FALSE);
            mlwrite_one(MLbkt("Bogus goto argument"));
        }
    } else if (searchflag) {
        setpattern(pat, tap);   /* Need a valid curwp for this */
        srch_can_hunt = 1;
        if (forwhunt(FALSE, 0) == FALSE) update(FALSE);
    }

/* Setup to process commands. */
    lastflag = 0;  /* Fake last flags. */

loop:
/* Execute the "command" macro...normally null. */
    saveflag = lastflag;  /* Preserve lastflag through this. */
/* Don't start the handler when it is already running as that might
 * just get into a loop...
 */
    if (!meta_spec_active.C) {
        meta_spec_active.C = 1;
        execute(META|SPEC|'C', FALSE, 1);
        meta_spec_active.C = 0;
    }
    lastflag = saveflag;

    if (typahead()) {
        newc = getcmd();
        update(FALSE);
        do {
            if (c == newc) {
                struct key_tab *ktp = getbind(c);
                if (ktp) {
                    fn_t execfunc = ktp->hndlr.k_fp;
                    if (execfunc != insert_newline && execfunc != insert_tab)
                         newc = getcmd();
                }
            }
            else break;
        } while (typahead());
        c = newc;
    }
    else {
        update(FALSE);
        if (display_readin_msg ||   /* First one gets removed by update() */
              mbuf_mess) {          /* Specific user message */
            int scol = curcol;
            int srow = currow;
            mlwrite_one(mbuf_mess? mbuf_mess: readin_mesg);
            display_readin_msg = 0;
            mbuf_mess = NULL;
            movecursor(srow, scol); /* Send the cursor back to where it was */
            TTflush();
        }
        c = getcmd();
    }
/* If there is something on the command line, clear it */

    if (!mline_persist && (mpresf != FALSE)) {
        mlerase();
        update(FALSE);
#if CLRMSG
        if (c == ' ') goto loop;    /* ITS EMACS does this  */
#endif
    }

/* Check for any numeric prefix */

    com_arg *carg = multiplier_check(c);

/* And execute the command */
    if (carg->f) mlerase();   /* Remove any numeric arg */
    execute(carg->c, carg->f, carg->n);
    goto loop;
}

/* ======================================================================
 * Initialize all of the buffers and windows. The buffer name is passed down
 * as an argument, because the main routine may have been told to read in a
 * file by default, and we want the buffer name to be right.
 */
void edinit(char *bname) {
    struct buffer *bp;
    struct window *wp;

    bp = bfind(bname, TRUE, 0);             /* First buffer         */
    blistp = bfind("//List", TRUE, BFINVS); /* Buffer list buffer   */
    if (bp == NULL || blistp == NULL) exit(1);
    wp = (struct window *)Xmalloc(sizeof(struct window));   /* First window */
    curbp = bp;             /* Make this current    */
    wheadp = wp;
    curwp = wp;
    wp->w_wndp = NULL;                      /* Initialize window    */
    wp->w_bufp = bp;
    bp->b_nwnd = 1;                         /* Displayed.           */
    wp->w_linep = bp->b_linep;
    wp->w.dotp = bp->b_linep;
    wp->w.doto = 0;
    wp->w.markp = NULL;
    wp->w.marko = 0;
    wp->w_toprow = 0;
#if     COLOR
/* initialize colors to global defaults */
    wp->w_fcolor = gfcolor;
    wp->w_bcolor = gbcolor;
#endif
    wp->w.fcol = 0;
    wp->w_ntrows = term.t_vscreen;          /* Ignoring mode-line   */
    wp->w_force = 0;
    wp->w_flag = WFMODE | WFHARD;           /* Full.                */
    return;
}

/* ======================================================================
 * What gets called if we try to run something in the minibuffer which
 * we shouldn't.
 * Requires not_in_mb to have been filled out.
 */
int not_in_mb_error(int f, int n) {
    UNUSED(f); UNUSED(n);
    char vis_key_paras[23];
    if (not_in_mb.keystroke != -1) {
        char vis_key[20];
        cmdstr(not_in_mb.keystroke, vis_key);
        snprintf(vis_key_paras, 22, "(%s)", vis_key);
    }
    else
        strcpy(vis_key_paras, "(?!?)");
    mlwrite("%s%s not allowed in the minibuffer!!",
         not_in_mb.funcname, vis_key_paras);
    return(TRUE);
}

/* ======================================================================
 * What gets called if we try to run something interactively which
 * we shouldn't.
 * Requires not_interactive_fname to have been set.
 */
int not_interactive(int f, int n) {
    UNUSED(f); UNUSED(n);
    mlwrite("%s may not be run interactively!!", not_interactive_fname);
    return(TRUE);
}

/* ======================================================================
 * This is the general command execution routine. It handles the fake binding
 * of all the keys to "self-insert". It also clears out the "thisflag" word,
 * and arranges to move it to the "lastflag", so that the next command can
 * look at it. Return the status of command.
 */
int execute(int c, int f, int n) {
    int status;

/* If the keystroke is a bound function...do it.
 * However, we'll handle Space and B/b as special in VIEW mode
 * and <Return> as special in VIEW and dir_browsing mode (which is only
 * active in VIEW mode).
 */
    int dir_browsing = 0;
    struct key_tab *ktp = NULL;
    if (((c & 0x7fffffff) < 0x7f || c == (CONTROL|'M')) &&
        (curbp->b_mode & MDVIEW)) {
        if (c == ' ') {
            ktp = getbyfnc(forwpage);
        }
        else if ((c | DIFCASE) == 'b') {
            ktp = getbyfnc(backpage);
        }
/* If we are in the //directory buffer in view-only mode with a showdir
 * user_proc then handle certain command chars.
 * NOTE: that the //directory buffer name *MUST* agree with that used
 * in the showdir userproc.
 * Make the test short if it's going to fail...
 * We need to do this *now* in order to trap newline for this...and
 * just remap it to the character that implements what we want it to do...
 * ALSO note that since we've handled Space and B above, these cannot take
 * on any different meaning within the directory browsing window.
 */
        else if (!strcmp("//directory", curbp->b_bname)) {
            struct buffer *sdb = bfind("/showdir", FALSE, 0);
            dir_browsing = (sdb && (sdb->b_type == BTPROC));
        }
        if (c == (CONTROL|'M')) {
            if (dir_browsing) c = 'd';
            else ktp = getbyfnc(forwpage);
        }
    }
    if (!ktp) ktp = getbind(c);

    if (ktp) {
        fn_t execfunc = ktp->hndlr.k_fp;
        if (execfunc == nullproc) return(TRUE);
        int run_not_in_mb = 0;
        int run_not_interactive = 0;
        struct buffer *proc_bp = NULL;
        if (inmb) {
            if (ktp->k_type == FUNC_KMAP) {
                if (ktp->fi->opt.not_mb) {
                    run_not_in_mb = 1;
                    not_in_mb.funcname = ktp->fi->n_name;
                }
            }
            else if (ktp->k_type == PROC_KMAP && ktp->hndlr.pbp != NULL) {
                char pbuf[NBUFN+1];
                pbuf[0] = '/';
                strcpy(pbuf+1, ktp->hndlr.pbp);
                if ((proc_bp = bfind(pbuf, FALSE, 0)) != NULL) {
                    if (proc_bp->btp_opt.not_mb) {
                        run_not_in_mb = 1;
                        not_in_mb.funcname = ktp->hndlr.pbp;
                    }
                }
            }
        }

        if (!clexec) {
            if (ktp->k_type == FUNC_KMAP) {
                if (!clexec && ktp->fi->opt.not_interactive) {
                    run_not_interactive = 1;
                    not_interactive_fname = ktp->fi->n_name;
                }
            }
            else if (ktp->k_type == PROC_KMAP && ktp->hndlr.pbp != NULL) {
                if (!proc_bp) {     /* We might have just done this above */
                    char pbuf[NBUFN+1];
                    pbuf[0] = '/';
                    strcpy(pbuf+1, ktp->hndlr.pbp);
                    proc_bp = bfind(pbuf, FALSE, 0);
                }
                if (proc_bp != NULL) {
                    if (!clexec && proc_bp->btp_opt.not_interactive) {
                        run_not_interactive = 1;
                        not_interactive_fname = ktp->hndlr.pbp;
                    }
                }
            }
        }

/* Factor in any keybinding specified multiplier */

        if (ktp->bk_multiplier != 1) {
            n *= ktp->bk_multiplier;
            f = TRUE;
        }
        if (run_not_in_mb) {
            not_in_mb.keystroke = c;
            execfunc = not_in_mb_error;
        }
        if (run_not_interactive) {
            execfunc = not_interactive;
        }
        thisflag = 0;
/* GGR - implement re-execute */
        if ((execfunc != reexecute) && (execfunc != nullproc) &&
            (execfunc != ctlxrp)) {     /* Remember current set */
            f_arg.func = execfunc;
            f_arg.ca.c = c;
            f_arg.ca.f = f;
            f_arg.ca.n = n;
        }

/* If we are recording a macro and:
 *  o we are not in the minibuffer (which is collected elsewhere)
 *  o we are not re-executing (if we are we've already recorded the reexecute)
 */
        if (!inmb && !inreex && kbdmode == RECORD) {
            if (ktp->fi->opt.skip_in_macro) {   /* Skip these, mostly... */
                if (execfunc == namedcmd) {     /* Use next func directly... */
                    if ((f > 0) && (n != 1))    /* ...but record any count */
                        set_narg_kbdmacro(n);
                }
            }
            else {                          /* Record it */
                if ((f > 0) && (n != 1)) set_narg_kbdmacro(n);
                addto_kbdmacro(ktp->fi->n_name, 1, 0);
            }
        }
        if (!run_not_in_mb &&
             ktp->k_type == PROC_KMAP && ktp->hndlr.pbp != NULL) {
            execfunc = execproc;    /* Run this instead... */
            input_waiting = ktp->hndlr.pbp;
            if (!inmb && kbdmode == RECORD)
                 addto_kbdmacro(input_waiting, 0, 0);
        }
        else input_waiting = NULL;

        running_function = 1;   /* Rather than keyboard input */
        status = execfunc(f, n);
        if (!ktp->fi->opt.search_ok) srch_can_hunt = 0;
        running_function = 0;
        input_waiting = NULL;
        if (execfunc != showcpos) lastflag = thisflag;
/* GGR - abort running/collecting keyboard macro at point of error */
        if ((kbdmode != STOP) & !status) end_kbdmacro();
        return status;
    }

/* A single character "self-insert" also has to be remembered so that
 * ctl-C while typing repeats the last character
 */
    f_arg.func = NULL;
    f_arg.ca.c = c;
    f_arg.ca.f = f;
    f_arg.ca.n = n;

    if (dir_browsing) {
        int test_char = c | DIFCASE;    /* Quick lowercase */
        switch(test_char) { /* NOTE: Space and 'b' *cannot* be used here! */
        case 'd':           /* Dive into entry on current line */
        case 'o':           /* Open entry on current line */
        case 'v':           /* View entry on current line */

/* We want to get to the end of the relevant token.
 * This is defined when setting up the showdir userproc, so that if the
 * line-generating code is changed you don't need to recompile uemacs to
 * change things.
 * Then advance a space and take the rest of the line as the entry name
 * since we're only looking for space we can just use ASCII.
 */
           {char *lp = curwp->w.dotp->l_text;
/* Check that we can handle this type of entry.
 * The showdir command will have followed all symlinks, so
 * we're only interested in directories and files.
 */
            if (*lp != '-' && *lp != 'd') {
                mlwrite_one("Error: showdir only views directories and files");
                break;
            }
            int max = llength(curwp->w.dotp);
            int tok = showdir_tokskip;
            if (tok < 0) {
                mlwrite_one("Error: $showdir_tokskip is undefined");
                break;
            }
            int decr = 1;
            char fname[NFILEN];
            int fnlen;
            for (;;) {
                if (decr && *lp == ' ') {
                    decr = 0;
                    if (--tok == 0) break;
                }
                else
                    if (!decr && *lp != ' ') {
                        decr = 1;
                    }
                if (--max == 0) break;
                lp++;
            }
            if (tok) {      /* Didn't get them all? */
                mlwrite_one("Can't parse line");
                break;
            }
/* Move to start of name, and get the length to end-of-data (no NUL here) */
            lp++;   /* Step over next space */
            fnlen = curwp->w.dotp->l_text+llength(curwp->w.dotp) - lp;

/* Now build up the full pathname
 * Start with the current buffer filename, and append "/", unless we
 * are actually at "/" (quick test).
 */
            strcpy(fname, curwp->w_bufp->b_fname);
            if (fname[1] != '\0') strcat(fname, "/");
/* Add in this entryname, then work out the full pathname length
 * and append a NUL
 */
            strncat(fname, lp, fnlen);
            int full_len = strlen(curwp->w_bufp->b_fname) + 1 + fnlen;
            fname[full_len] = '\0';

/* May be file or dir - getfile() sorts it out */
            getfile(fname, test_char != 'v', TRUE); /* c.f. filefind/viewfile */
            if (test_char == 'v') curwp->w_bufp->b_mode |= MDVIEW;
            break;
           }
        case 'a':           /* Refresh/toggle current view in ASCII mode */
        case 't':           /* Refresh/toggle current view in TIME mode */
        case 'r':           /* Refresh current view in current mode */
        case 'h':           /* Toggle hidden files in current mode */
        case 'm':           /* Toggle mixed/type-sorted display */
        case 'f':           /* Toggle dirs/files order in type-sorted mode */
            getfile(curbp->b_fname, FALSE, TRUE);
            break;
        case 'u':           /* Up to parent. Needs run_user_proc() */
           {char fname[NFILEN];
            strcpy(fname, curwp->w_bufp->b_fname);
            char *upp = strrchr(fname, '/');
            if (upp == fname) upp++;
            *upp = '\0';
            userproc_arg = fname;
            (void)run_user_proc("showdir", 0, 1);
            userproc_arg = NULL;
            break;
           }
        }
        lastflag = 0;       /* Fake last flags.     */
        return TRUE;
    }

/*
 * If a space was typed, fill column is defined, the argument is non-
 * negative, wrap mode is enabled, and we are now past fill column,
 * and we are not read-only, perform word wrap.
 * NOTE that we then continue on to self-insert the space!
 */
    if (c == ' ' && (curwp->w_bufp->b_mode & MDWRAP) && fillcol > 0 &&
          n >= 0 && getccol() > fillcol &&
         (curwp->w_bufp->b_mode & MDVIEW) == FALSE) {
/* Don't start the handler when it is already running as that might
 * just get into a loop...
 */
        if (!meta_spec_active.W) {
            meta_spec_active.W = 1;
            execute(META|SPEC|'W', FALSE, (ggr_opts&GGR_FULLWRAP)? 2: 1);
            meta_spec_active.W = 0;
/* If the result of the wrap is that we are at the start of a line then
 * we don't want to add a space.
 * This has to match the handling in insert_newline() in random.c.
 */
            if (curwp->w.doto == 0) return TRUE;
        }
    }

    if ((c >= 0x20 && c <= 0x7E)    /* Self inserting.      */
#if     BSD || USG       /* 8BIT P.K. */
        || (c >= 0xA0 && c <= 0x10FFFF)) {
#else
        ) {
#endif

/* GGR - Implement Phonetic Translation iff we are about to self-insert.
 * If the mode is active call the handler.
 * If this returns TRUE then the character has been handled such that
 * we do not need to insert it.
 */
        if (curbp->b_mode & MDPHON) {
            if (ptt_handler(c)) return TRUE;
        }

        if (n <= 0) {   /* Fenceposts.          */
            lastflag = 0;
            return n < 0 ? FALSE : TRUE;
        }
        thisflag = 0;   /* For the future.      */

/* If we are in overwrite mode, not at eol, and next char is not a tab
 * or we are at a tab stop, delete a char forward
 */
        if (curwp->w_bufp->b_mode & MDOVER &&
            curwp->w.doto < curwp->w.dotp->l_used &&
            (lgetc(curwp->w.dotp, curwp->w.doto) != '\t' ||
            (curwp->w.doto) % 8 == 7))
                ldelgrapheme(1, FALSE);

/* Do the appropriate insertion */
        if (c == '}' && (curbp->b_mode & MDCMOD) != 0) {
            status = insbrace(n, c);
            if (!inmb && kbdmode == RECORD) {
                if ((f > 0) && (n != 1))    /* Record any count */
                    set_narg_kbdmacro(n);
                addto_kbdmacro("macro-helper", 1, 0);
                addto_kbdmacro("}", 0, 0);
            }
        }
        else if (c == '#' && (curbp->b_mode & MDCMOD) != 0) {
            status = inspound();
            if (!inmb && kbdmode == RECORD) {
                 addto_kbdmacro("macro-helper", 1, 0);
                 addto_kbdmacro("#", 0, 0);
            }
	}
        else {
            status = linsert_uc(n, c);    /* We get Unicode, not utf-8 */
            if (!inmb && kbdmode == RECORD) {
                int nc = 1;
                if ((f > 0) && (n > 1)) nc = n;
                while(nc--) addchar_kbdmacro(c);
	    }
	}

/* Check for CMODE fence matching */
         if ((c == '}' || c == ')' || c == ']') &&
            (curbp->b_mode & MDCMOD) != 0)
                fmatch(c);

/* Check auto-save mode */
        if (curbp->b_mode & MDASAVE)
            if (--gacount == 0) {   /* And save the file if needed */
                upscreen(FALSE, 0);
                filesave(FALSE, 0);
                gacount = gasave;
            }

        lastflag = thisflag;
        return status;
    }
    TTbeep();
    mlwrite_one(MLbkt("Key not bound"));  /* complain             */
    lastflag = 0;                           /* Fake last flags.     */
    return FALSE;
}

/* ======================================================================
 * Fancy quit command, as implemented by Norm. If any buffer has changed
 * do a write on that buffer and exit uemacs, otherwise simply exit.
 */
int quickexit(int f, int n) {
    struct buffer *bp;      /* scanning pointer to buffers */
    struct buffer *oldcb;   /* original current buffer */
    int status;

    oldcb = curbp;          /* save in case we fail */

    bp = bheadp;
    while (bp != NULL) {
        if ((bp->b_flag & BFCHG) != 0       /* Changed.             */
             && (bp->b_flag & BFTRUNC) == 0  /* Not truncated P.K.   */
             && (bp->b_flag & BFINVS) == 0) {/* Real.                */
            curbp = bp;                 /* make that buffer cur */
            mlwrite(MLbkt("Saving %s"), bp->b_fname);
            mlwrite_one("\n");              /* So user can see filename */
            if ((status = filesave(f, n)) != TRUE) {
                curbp = oldcb;          /* restore curbp */
                sleep(1);
                redraw(FALSE, 0);       /* Redraw - remove filenames */
                return status;
            }
        }
        bp = bp->b_bufp;            /* on to the next buffer */
    }
    quit(f, n);                     /* conditionally quit   */
    return TRUE;
}

/* ======================================================================
 * The original signal handler - left bound to SIGHUP.
 */
static void emergencyexit(int signr) {
    UNUSED(signr);
    quickexit(FALSE, 0);
    quit(TRUE, 0);
}

/* ======================================================================
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out. Normally bound to "C-X C-C".
 */
int quit(int f, int n) {
    int s;

    if (f != FALSE          /* Argument forces it.  */
        || anycb() == FALSE /* All buffers clean.   */
        || (s =             /* User says it's OK.   */
            mlyesno("Modified buffers exist. Leave anyway")) == TRUE) {
#if FILOCK && (BSD || SVR4)
        if (lockrel() != TRUE) {
            ttput1c('\n');
            ttput1c('\r');
            TTclose();
            TTkclose();
            exit(1);
        }
#endif
        vttidy();
#ifdef DO_FREE

/* Explicitly free things in debug mode, to help things like valgrind. */
    free_bind();
    free_buffer();
    free_display();
    free_eval();
    free_exec();
    free_input();
    free_line();
    free_names();
    free_search();
    free_utf8();

#if FILOCK && (BSD | SVR4)
    free_lock();
#endif

/* Remove all windows */

    struct window *nextwp;
    for (struct window *wp = wheadp; wp; wp = nextwp) {
        nextwp = wp->w_wndp;
        Xfree(wp);
    }
    if (eos_list) Xfree(eos_list);

#endif
        if (f) exit(n);
        else   exit(GOOD);
    }
    mlwrite_one("");
    return s;
}

/* ======================================================================
 * Begin a keyboard macro.
 * Error if not at the top level in keyboard processing. Set up variables and
 * return.
 */
int ctlxlp(int f, int n) {
    UNUSED(f); UNUSED(n);
    if (kbdmode != STOP) {
        mlwrite_one("%Macro already active");
        return FALSE;
    }
    if (strcmp(curbp->b_bname, kbdmacro_buffer) == 0) {
        mlwrite_one("%Cannot collect macro when in keyboard macro buffer");
        return FALSE;
    }

/* Have to save current c/f/n-last */
    p_arg = f_arg;          /* Restored on ctlxrp in execute() */

    mlwrite_one(MLbkt("Start macro"));
    kbdptr = kbdm;
    kbdend = kbdptr;
    kbdmode = RECORD;
    start_kbdmacro();
    return TRUE;
}

/* ======================================================================
 * End keyboard macro. Check for the same limit conditions as the above
 * routine. Set up the variables and return to the caller.
 * NOTE that since the key strokes to get here when recording a macro
 * are in the macro buffer we take advantage of that to implement
 * re-executing the macro multiple-times.
 */
int ctlxrp(int f, int n) {
    UNUSED(f); UNUSED(n);
    if (kbdmode == STOP) {
        mlwrite_one("%Macro not active");
        return FALSE;
    }
    if (kbdmode == RECORD) end_kbdmacro();

/* Collecting a keyboard macro puts a ctlxrp at the end of the buffer.
 * this is somewhat fortunate, as it allows us to detect the end of a
 * macro pass and set things up for the next reexecute call when we
 * have a numeric arg > 1 for execute-macro.
 */
    if (kbdmode != STOP) {
        f_arg = p_arg;      /* Restore saved set - should be ctlxe */

/* On the last keyboard repeat kbdrep will be 1 (haven't yet reached
 * the end - it will be >1 if we've been asked to run the macro
 * multiple times).
 * If we are on the last keyboard repeat, in PLAY mode and ctlxe still
 * has more repeats to go then call reexecute to set things up for the
 * next pass.
 */
        if ((kbdrep == 1) && kbdmode == PLAY && (ctlxe_togo > 0)) {
            kbdmode = STOP;     /* Leave ctlxe_togo as-is */
            return reexecute(f_arg.ca.f, f_arg.ca.n);
        }
        inreex = FALSE;
/* If an mlforce() message has been written we want it to remain there
 * for the next pass, but then go.
 * This means we need to skip the mpresf test near the start of loop for
 * one (more) pass.
 */
        mline_persist = FALSE;
    }
    return TRUE;
}

/* ======================================================================
 * Execute a macro.
 * The command argument is the number of times to loop. Quit as soon as a
 * command gets an error. Return TRUE if all OK, else FALSE.
 */
int ctlxe(int f, int n) {
    UNUSED(f);
    if (kbdmode != STOP) {
        mlwrite_one("%Macro already active");
        return FALSE;
    }
    if (n <= 0) return TRUE;

/* Have to save current c/f/n-last */
    p_arg = f_arg;          /* Restored on ctlxrp in execute() */

    kbdrep = n;             /* remember how many times to execute */
    kbdmode = PLAY;         /* start us in play mode */
    kbdptr = kbdm;          /*    at the beginning */
    return TRUE;
}

/* ======================================================================
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
int ctrlg(int f, int n) {
    UNUSED(f); UNUSED(n);
    TTbeep();
    if (kbdmode == RECORD) end_kbdmacro();
    mlwrite_one(MLbkt("Aborted"));
    return ABORT;
}

/* ======================================================================
 * Tell the user that this command is illegal while we are in
 * particular modes.
 */
int rdonly(void) {
    TTbeep();
    if (running_function)
        mlwrite(MLbkt("%s illegal in read-only buffer: %s"),
            current_command, curbp->b_fname);
    else
        mlwrite_one(MLbkt("Key illegal in VIEW mode"));
    return FALSE;
}
int resterr(void) {
    TTbeep();
    mlwrite_one(MLbkt("That command is RESTRICTED"));
    return FALSE;
}

/* ======================================================================
 * User function that does NOTHING
 */
int nullproc(int f, int n) {
    UNUSED(f); UNUSED(n);
    return TRUE;
}

/* ======================================================================
 * Dummy function for binding to meta prefix.
 */
int metafn(int f, int n) {
    UNUSED(f); UNUSED(n);
    return TRUE;
}
/* ======================================================================
 * Dummy function for binding to control-x prefix.
 */
int cex(int f, int n) {
    UNUSED(f); UNUSED(n);
    return TRUE;
}

/* ======================================================================
 * Dummy function for binding to universal-argument
 */
int unarg(int f, int n) {
    UNUSED(f); UNUSED(n);
    return TRUE;
}

/* ======================================================================
 * GGR - Function to implement re-execute last command
 */
int reexecute(int f, int n) {
    UNUSED(f);
    int reloop;
/* If we are being asked to reexec running the macro (ctlxe) then we have
 * to return to let it happen (i.e. collect keys from the macro buffer).
 * We also need to remember how many times to pass here if n > 1.
 * Since we can't actually recurse into a keyboard macro we can
 * manage with this fudge, rather than having to restructure everything
 * into recursive calls.
 * Do NOT set inreex for this!! We aren't reexecing anything - instead
 * we're setting up the macro to replay, which is not the same thing...
 */
    if (f_arg.func == ctlxe) {
        if (ctlxe_togo == 0) {  /* First call */
            if (n > 1)  ctlxe_togo = n;
            else        ctlxe_togo = 1;
        }
        ctlxe(f_arg.ca.f, f_arg.ca.n);
        ctlxe_togo--;
        return TRUE;
    }

    inreex = TRUE;
/* We can't just multiply n's. Must loop. */
    for (reloop = 1; reloop<=n; ++reloop) {
        execute(f_arg.ca.c, f_arg.ca.f, f_arg.ca.n);
    }
    inreex = FALSE;
    return(TRUE);
}

/* ======================================================================
 * GGR - Extend the size of the key_table list.
 * If input arg is non-zero use that, otherwise extend by the
 * defined increment and update keytab_alloc_ents.
 */
static struct key_tab endl_keytab = {ENDL_KMAP, 0, {NULL}, NULL, 0};
static struct key_tab ends_keytab = {ENDS_KMAP, 0, {NULL}, NULL, 0};

void extend_keytab(int n_ents) {

    int init_from;
    if (n_ents == 0) {
        init_from = keytab_alloc_ents;
        keytab_alloc_ents += KEYTAB_INCR;
    }
    else {      /* Only happens at start */
        init_from = 0;
        keytab_alloc_ents = n_ents;
    }
    keytab = Xrealloc(keytab, keytab_alloc_ents*sizeof(struct key_tab));
    if (init_from == 0) {           /* Add in starting data */
        int n_init_keys = sizeof(init_keytab)/sizeof(typeof(init_keytab[0]));
        struct key_tab *ktp = keytab;
        for (int n = 0; n < n_init_keys; n++, ktp++) {
            ktp->k_type = FUNC_KMAP;    /* All init ones are this */
            ktp->k_code = init_keytab[n].k_code;
            ktp->hndlr.k_fp = init_keytab[n].k_fp;
            ktp->fi = func_info(ktp->hndlr.k_fp);
            ktp->bk_multiplier = 1;
        }
        init_from = n_init_keys;    /* Only need to add tags from here */
    }
/* Add in marker tags for (new) free entries */
    for (int i = init_from; i < keytab_alloc_ents - 1; i++)
        keytab[i] = endl_keytab;
    keytab[keytab_alloc_ents - 1] = ends_keytab;

    key_index_valid = 0;    /* Rebuild index before using it. */

    return;
}
