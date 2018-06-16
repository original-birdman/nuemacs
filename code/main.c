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
"      -g<n>        go to line <n> (same as +<n>)"        NL \
"      -i           insecure mode - look in current dir"  NL \
"      -k<key>      encryption key"                       NL \
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

/* EXPERIMENTAL KBD MACRO CODE */

static char kbd_text[1024];
static int kbd_idx;
static int must_quote;

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
        last_mode = GetTo_KBDM;
        obp = curbp;                    /* Save whence we came */
        wsave = *curwp;                 /* Structure copy - save */
        saved_nw = kbdmac_bp->b_nwnd;   /* Mark as "not viewed" */
        kbdmac_bp->b_nwnd = 0;
        if (!swbuffer(kbdmac_bp)) {
            mlwrite("%s: cannot reach keyboard macro buffer!", who);
            kbdmac_bp->b_nwnd = saved_nw;
            kbdmode = STOP;
            do_savnam = 1;
            return FALSE;
        }
        return TRUE;
    case OutOf_KBDM:
        if (last_mode == OutOf_KBDM) goto out_of_phase;
        last_mode = OutOf_KBDM;
        int status = swbuffer(obp);
        *curwp = wsave;             /* Structure copy - restore */
        kbdmac_bp->b_nwnd = saved_nw;
        do_savnam = 1;
        return status;
    }
out_of_phase:                       /* Can only get here on error */
    mlforce("Keyboard macro collection out of phase - aborted.");
    do_savnam = 1;
    kbdmode = STOP;
    return FALSE;
}

/* ======================================================================
 * Internal routine to create the keyboard-macro buffer.
 */
static int create_kbdmacro_buffer(void) {
    if ((kbdmac_bp = bfind(kbdmacro_buffer, TRUE, BFINVS)) == NULL) {
        mlwrite("Cannot create keyboard macro buffer!");
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
        mlwrite("start: no keyboard macro buffer!");
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
    linstr("insert-raw-string ");
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
        linstr("insert-raw-string ");
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
static int func_rpt_cnt = 0;
static void set_narg_kbdmacro(int n) {
    func_rpt_cnt = n;
    return;
}

/* ======================================================================
 * Add a string to the keyboard macro buffer.
 */
int addto_kbdmacro(char *text, int new_command, int do_quote) {
    if (!kbdmac_bp) {
        mlwrite("addto: no keyboard macro buffer!");
        return FALSE;
    }
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "addto")) return FALSE;

/* If there is any pending text we need to flush it first */
    if (kbd_idx) flush_kbd_text();
    if (new_command) {
        lnewline();
        if (func_rpt_cnt) {
            linstr(ue_itoa(func_rpt_cnt));
            linsert_byte(1, ' ');
            func_rpt_cnt = 0;
        }
    }
    else linsert_byte(1, ' ');
    if (!do_quote) linstr(text);
    else {
        int qreq = 0;
        for (char *tp = text; *tp; tp++) {
            if (*tp == ' ') {
                qreq = 1;
                break;
            }
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
            case '~': xc = '~'; break;      /* It *does* handle this */
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
 */
static int end_kbdmacro(void) {
    if (!kbdmac_bp) {
        mlwrite("end: no keyboard macro buffer!");
        return FALSE;
    }
    if (!kbdmac_buffer_toggle(GetTo_KBDM, "end")) return FALSE;

/* If there is any pending text we need to flush it first */
    if (kbd_idx) flush_kbd_text();
    lnewline();
    kbdmode = STOP;
    return kbdmac_buffer_toggle(OutOf_KBDM, "end");
}

/* ======================================================================
 * Used to get the action for certain active characters into macros,
 * rather than just getting the raw character inserted.
 */
int macro_helper(int f, int n) {
    UNUSED(f);
    char tag[2];                        /* Just char + NULL needed */
    int status = mlreplyall("helper:", tag, 1);
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

#if defined(SIGWINCH) && defined(NUTRACE)
/* Implement a stack dump compile time option using libbacktrace. */

#include "backtrace.h"
#include "backtrace-supported.h"

struct bt_ctx {
    struct backtrace_state *state;
    int error;
};

/* ======================================================================
 * A set of callback routines to provide the stack-trace.
 */
static void syminfo_callback (void *data, uintptr_t pc,
     const char *symname, uintptr_t symval, uintptr_t symsize) {
    UNUSED(data); UNUSED(symval); UNUSED(symsize);
    if (symname) {
        printf("0x%lx %s ??:0\n", (unsigned long)pc, symname);
    }
    else {
        printf("0x%lx ?? ??:0\n", (unsigned long)pc);
    }
}
static void error_callback(void *data, const char *msg, int errnum) {
    struct bt_ctx *ctx = data;
    printf("ERROR: %s (%d)", msg, errnum);
    ctx->error = 1;
}
static int full_callback(void *data, uintptr_t pc, const char *filename,
     int lineno, const char *function) {

    struct bt_ctx *ctx = data;
    if (function) {
        printf("0x%lx %s %s:%d\n", (unsigned long)pc, function,
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
 */
static void do_stackdump(void) {

    printf("Traceback:\n");

    struct backtrace_state *state =
        backtrace_create_state (called_as, BACKTRACE_SUPPORTS_THREADS,
             error_callback, NULL);
    struct bt_ctx ctx = {state, 0};
/* Start the stacktrace 3 levels back (ignore this routine,
 * the signal handler that called us and the signal thrower code).
 */
    backtrace_simple(state, 2, simple_callback, error_callback, &ctx);
    printf("\n");
}
#endif

/* ====================================================================== */
#define Dumpdir_Name "uemacs-dumps"
#define AutoClean_Buffer "//autoclean"

/* ======================================================================
 * Generate the time-tag string.
 * We assume that we don't get multiple dumps in the same second.
 */
static char time_stamp[20]; /* There is only one time_stamp at a time... */
static int set_time_stamp(int days_back) {
    time_t t = time(NULL) - days_back*86400;;
    return strftime(time_stamp, 20, "%Y%m%d-%H%M%S.", localtime(&t));
}

/* ======================================================================
 * Scan through the buffers and look for modified ones associated
 * with files. For all it finds it tries to save them in ~/uemacs-dumps.
 * If any buffers are narrowed it widens them before dumping.
 * Since we are dying we don't preserve the curent working dir.
 */
static void dump_modified_buffers(void) {

    int status;

    printf("Trying to save modified buffers\n");

/* Get the current dir name - possibly needed for the index */
    char cwd[1024];
    char *dnc = getcwd(cwd, 1024);
    UNUSED(dnc);

/* Get to the dump directory */

    char *p;
    if ((p = getenv("HOME")) == NULL) {
        perror("Can't find HOME - no buffer saving:");
        return;
    }
    status = chdir(p);
    if (status !=0) {
        perror("Can't get to HOME - no buffer saving:");

    }
    status = mkdir(Dumpdir_Name, 0700); /* Private by default */
    if (status != 0 && errno != EEXIST) {
        perror("Can't make HOME/" Dumpdir_Name " - no buffer saving: ");
        return;
    }
    status = chdir(Dumpdir_Name);
    if (status !=0) {
        perror("Can't get to HOME/" Dumpdir_Name " - no buffer saving:");
        return;
    }

/* Generate a time-tag for the dumped files.
 * We assume that we don't get multiple dumps in the same second to
 * the same user's HOME.
 */
    char tagged_name[NFILEN], orig_name[NFILEN];
    int ts_len = set_time_stamp(0);     /* Set it for "now" */

/* Scan the buffers */

    int index_open = 0;
    int do_index = 0;
    int add_cwd;
    FILE *index_fp = NULL;  /* Avoid "may be used uninitialized" */
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
            index_fp = fopen("INDEX", "a");
            if (index_fp == NULL) {
                perror("No INDEX update");
            }
            else
                do_index = 1;
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
    if (!swbuffer(auto_bp)) {
        mlwrite("Failed to get to %s!\n", AutoClean_Buffer);
        return;
    }
/* A -ve days_allowed means "never". */
    if (autoclean < 0) {
        mlwrite("autoclean -ve (%d). No cleaning done\n");
        goto revert_buffer;
    }

    char info_message[4096]; /* Hopefully large enough */

/* Open the current directort on a file-descriptor for ease of return */
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

    FILE *index_fp = fopen("INDEX", "a+");
    if (index_fp == NULL) {
        snprintf(info_message, 4096,
              "Can't open ~/%s/INDEX: %s\n", Dumpdir_Name, strerror(errno));
        linstr(info_message);
        goto revert_to_start_fd;
    }

/* We now have the index open...
 * Read through it until a line is greater than the set time_stamp
 * deleting files as we go.
 */
    char *lp = NULL;
    size_t blen = 0;
    int ts_len = set_time_stamp(autoclean);
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
        while (fseek(index_fp, rewrite_from, SEEK_SET)) {
            char cbuf[4096];
            size_t rc = fread(cbuf, sizeof(char), 1, index_fp);
            if (feof(index_fp)) {
                done = 1;
                clearerr(index_fp);
            }
            else {                  /* Update where we read from */
                rewrite_from = ftell(index_fp);
            }
            if (rc) {
                status = fseek(index_fp, new_offs, SEEK_SET);
                size_t wc = fwrite(cbuf, sizeof(char), 1, index_fp);
                new_offs += wc;
                if (wc != rc) {     /* We have a problem... */
                    done = 1;
                }
            }
            if (done) break;
        }
        status = ftruncate(fileno(index_fp), new_offs);
    }

/* Close files and free buffers */

    if (fclose(index_fp)) {
        snprintf(info_message, 4096,
              "INDEX rewrite error: %s\n", strerror(errno));
        linstr(info_message);
    }
    if (lp) free(lp);

revert_to_start_fd:
    status = fchdir(start_fd);
    if (status < 0) mlforce("Stuck in dump dir!!!");
close_start_fd:
    close(start_fd);
revert_buffer:
    swbuffer(saved_bp);
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
    printf("%s: signal %s seen.\n", called_as, strsignal(signr));

/* Possibly a stack dump */
#if defined(SIGWINCH) && defined(NUTRACE)
    do_stackdump();
#endif

/* Try to save any modified files. */
    dump_modified_buffers();

    exit(signr);
}

/* ====================================================================== */

int main(int argc, char **argv) {
    int c = -1;             /* command character */
    int f;                  /* default flag */
    int n;                  /* numeric repeat count */
    int mflag;              /* negative flag on repeat */
    struct buffer *bp;      /* temp buffer pointer */
    int firstfile;          /* first file flag */
    struct buffer *firstbp = NULL;  /* ptr to first buffer in cmd line */
    int basec;              /* c stripped of meta character */
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
    char *rcfile = NULL;    /* GGR non-default rc file */
    char *rcextra[10];      /* GGR additional rc files */
    unsigned int rcnum = 0; /* GGR number of extra files to process */

#if BSD
        sleep(1); /* Time for window manager. */
#endif

#if UNIX
    struct sigaction sigact, oldact;
#ifdef SIGWINCH
    sigact.sa_handler = sizesignal;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sigact, &oldact);
#endif
/* Add a handler for other signals, whcih will exit */
    sigact.sa_handler = exit_via_signal;
    sigact.sa_flags = SA_RESETHAND; /* So we can't loop into our handler */
/* The SIGTERM is there so you can trace a loop */
    int siglist[] = { SIGBUS, SIGFPE, SIGSEGV, SIGTERM };
    for (unsigned int si = 0; si < sizeof(siglist)/sizeof(siglist[0]); si++)
        sigaction(siglist[si], &sigact, &oldact);
    called_as = argv[0];
/* The old one to get you out... */
    sigact.sa_handler = emergencyexit;
    sigaction(SIGHUP, &sigact, &oldact);
#endif

/* GGR The rest of intialisation is done after processing optional args */
    varinit();              /* initialise user variables */

    viewflag = FALSE;       /* view mode defaults off in command line */
    gotoflag = FALSE;       /* set to off to begin with */
    searchflag = FALSE;     /* set to off to begin with */
    firstfile = TRUE;       /* no file to edit yet */
    errflag = FALSE;        /* not doing C error parsing */
    cryptflag = FALSE;      /* no encryption by default */

/* Set up the initial keybindings.  Must be done early, before any
 * command line processing.
 * NOTE that we must initialize the namelooup indexing first, i.e. before
 * calling extend_keytab().
 * We need to allow for the additonal ENDL_KMAP and ENDS_KMAP entries,
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
            if (strchr("cCdDgGkKsSxX", key1)) {
                opt = *argv + 2;
                if (*opt == '\0' && argc > 0 && !strchr("-@", *(*argv + 1))) {
                    if (--argc <= 0) {
                        fprintf(stderr, "parameter missing for -%c\n", key1);
                        usage(EXIT_FAILURE);
                    }
                    opt = *(++argv);
                }
            }

            switch (key1) { /* Process Startup macros */
            case 'a':       /* process error file */
            case 'A':
                errflag = TRUE;
                break;
            case 'c':
            case 'C':       /* GGR -c replacement of default rc */
                rcfile = opt;
                break;
            case 'd':
            case 'D':       /* GGR -d for config/help directory */
                set_pathname(opt);
                break;
            case 'e':       /* -e for Edit file */
            case 'E':
                viewflag = FALSE;
                break;
            case 'g':       /* -g for initial goto */
            case 'G':
                gotoflag = TRUE;
                gline = atoi(opt);
                break;
            case 'i':       /* -i for insecure mode */
            case 'I':
                allow_current = 1;
                break;
            case 'k':       /* -k<key> for code key */
            case 'K':
                /* GGR only if given a key.. */
                /* The leading ; *is* needed! */
                ;int olen = strlen(opt);
                if (olen > 0 && olen < NPAT) {
                    cryptflag = TRUE;
                     strcpy(ekey, opt);
                }
                break;
            case 'n':       /* -n accept null chars */
            case 'N':
                nullflag = TRUE;
                break;
            case 'r':       /* -r restrictive use */
            case 'R':
                restflag = TRUE;
                break;
            case 's':       /* -s for initial search string */
            case 'S':
                searchflag = TRUE;
                strncpy(pat, opt, NPAT);
/* GGR - set-up some more things for the FAST search algorithm */
                rvstrcpy(tap, pat);
                mlenold = matchlen = strlen(pat);
                setpattern(pat, tap);
                break;
            case 'v':       /* -v for View File */
            case 'V':
                if (!verflag) verflag = 1;    /* could be version or */
                viewflag = TRUE;    /* view request */
                break;
            case 'x':       /* GGR: -x for eXtra rc file */
            case 'X':
                if (rcnum < sizeof(rcextra)/sizeof(rcextra[0]))
                     rcextra[rcnum++] = opt;
                break;
            default:        /* unknown switch - ignore this for now */
                break;
            }

        }
        else if (**argv == '@') rcfile = *argv + 1;
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
    if (rcnum) {
        for (unsigned int n = 0; n < rcnum; n++) startup(rcextra[n]);
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
/* Set-up a buffer for this file */
        makename(bname, *argv);
        unqname(bname);
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
        argv++;
    }

/* Run the autocleaner...MUST come after processing start-up files, as they
 * may set $autoclean.
 */
    dumpdir_tidy();

/* Done with processing command line */

    discmd = TRUE;          /* P.K. */

/* If there are any files to read, read the first one! */
    int display_readin_msg = 0;
    bp = bfind("main", FALSE, 0);
    if (firstfile == FALSE && (gflags & GFREAD)) {
        swbuffer(firstbp);
        zotbuf(bp);
        display_readin_msg = 1;
    } else bp->b_mode |= gmode;

/* Deal with startup gotos and searches */
    if (gotoflag && searchflag) {
        update(FALSE);
        mlwrite(MLpre "Can not search and goto at the same time!" MLpost);
    } else if (gotoflag) {
        if (gotoline(TRUE, gline) == FALSE) {
            update(FALSE);
            mlwrite(MLpre "Bogus goto argument" MLpost);
        }
    } else if (searchflag) {
        if (forwhunt(FALSE, 0) == FALSE) update(FALSE);
    }

/* Setup to process commands. */
    lastflag = 0;  /* Fake last flags. */

loop:
/* Execute the "command" macro...normally null. */
    saveflag = lastflag;  /* Preserve lastflag through this. */
    execute(META | SPEC | 'C', FALSE, 1);
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
        if (display_readin_msg) {   /* First one gets removed by update() */
            mlwrite(readin_mesg);
            display_readin_msg = 0;
            movecursor(0, 0);       /* Send the cursor back to BoB */
            TTflush();
        }
        c = getcmd();
    }

/* If there is something on the command line, clear it */
    if (mpresf != FALSE) {
        mlerase();
        update(FALSE);
#if CLRMSG
        if (c == ' ') goto loop;    /* ITS EMACS does this  */
#endif
    }
    f = FALSE;
    n = 1;

/* Do META-# processing if needed */

    basec = c & ~META;      /* strip meta char off if there */
    if ((c & META) && ((basec >= '0' && basec <= '9') || basec == '-')) {
        f = TRUE;       /* there is a # arg */
        n = 0;          /* start with a zero default */
        mflag = 1;      /* current minus flag */
        c = basec;      /* strip the META */
        while ((c >= '0' && c <= '9') || (c == '-')) {
            if (c == '-') { /* already hit a minus or digit? */
                if ((mflag == -1) || (n != 0)) break;
                mflag = -1;
            }
            else {
                n = n * 10 + (c - '0');
            }
            if ((n == 0) && (mflag == -1))  /* lonely - */
                mlwrite("Arg:");
            else
                mlwrite("Arg: %d", n * mflag);

            c = getcmd();   /* get the next key */
        }
        n = n * mflag;  /* figure in the sign */
    }

/* Do ^U repeat argument processing */

    if (c == reptc) {       /* ^U, start argument   */
        f = TRUE;
        n = 4;          /* with argument of 4 */
        mflag = 0;      /* that can be discarded. */
        mlwrite("Arg: 4");
        while (((c = getcmd()) >= '0' && c <= '9') || c == reptc || c == '-') {
            if (c == reptc)
                if ((n > 0) == ((n * 4) > 0)) n = n * 4;
                else                          n = 1;
/*
 * If dash, and start of argument string, set arg.
 * to -1.  Otherwise, insert it.
 */
            else if (c == '-') {
                if (mflag) break;
                n = 0;
                mflag = -1;
            }
/*
 * If first digit entered, replace previous argument
 * with digit and set sign.  Otherwise, append to arg.
 */
            else {
                if (!mflag) {
                    n = 0;
                    mflag = 1;
                }
                n = 10 * n + c - '0';
            }
            mlwrite("Arg: %d", (mflag >= 0) ? n : (n ? -n : -1));
        }
/*
 * Make arguments preceded by a minus sign negative and change
 * the special argument "^U -" to an effective "^U -1".
 */
        if (mflag == -1) {
            if (n == 0) n++;
            n = -n;
            }
    }

/* And execute the command */
    execute(c, f, n);
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
    blistp = bfind("/List", TRUE, BFINVS);  /* Buffer list buffer   */
    wp = (struct window *)malloc(sizeof(struct window));    /* First window */
    if (bp == NULL || wp == NULL || blistp == NULL) exit(1);
    curbp = bp;             /* Make this current    */
    wheadp = wp;
    curwp = wp;
    wp->w_wndp = NULL;                      /* Initialize window    */
    wp->w_bufp = bp;
    bp->b_nwnd = 1;                         /* Displayed.           */
    wp->w_linep = bp->b_linep;
    wp->w_dotp = bp->b_linep;
    wp->w_doto = 0;
    wp->w_markp = NULL;
    wp->w_marko = 0;
    wp->w_toprow = 0;
#if     COLOR
/* initalize colors to global defaults */
    wp->w_fcolor = gfcolor;
    wp->w_bcolor = gbcolor;
#endif
    wp->w_fcol = 0;
    wp->w_ntrows = term.t_nrow - 1;         /* "-1" for mode line.  */
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
 * This is the general command execution routine. It handles the fake binding
 * of all the keys to "self-insert". It also clears out the "thisflag" word,
 * and arranges to move it to the "lastflag", so that the next command can
 * look at it. Return the status of command.
 */
int execute(int c, int f, int n) {
    int status;

/* If the keystroke is a bound function...do it */

    struct key_tab *ktp = getbind(c);
    if (ktp) {
        fn_t execfunc = ktp->hndlr.k_fp;
        if (execfunc == nullproc) return(TRUE);
        int run_not_in_mb = 0;
/* Special handling for nextwind/prevwind key-bindings, which get
 * re-mapped when being prompted for a search string.
 * Since the mapped-from functions are flagged with not_mb we have
 * to skip the minibuffer check.
 */
        if (in_search_prompt) {
            if (execfunc == nextwind) {
                execfunc = next_sstr;
                goto after_mb_check;
            }
            if (execfunc == prevwind) {
                execfunc = prev_sstr;
                goto after_mb_check;
            }
            if (execfunc == listbuffers) {
                execfunc = select_sstr;
                goto after_mb_check;
            }
        }
        if (inmb) {
            if (ktp->k_type == FUNC_KMAP && ktp->fi->opt.not_mb)
                run_not_in_mb = 1;
            else if (ktp->k_type == PROC_KMAP && ktp->hndlr.pbp != NULL) {
                 struct buffer *proc_bp;
                 char pbuf[NBUFN+1];
                 pbuf[0] = '/';
                 strcpy(pbuf+1, ktp->hndlr.pbp);
                 if ((proc_bp = bfind(pbuf, FALSE, 0)) != NULL) {
                    if (proc_bp->btp_opt.not_mb) run_not_in_mb = 1;
                 }
            }
        }
        if (run_not_in_mb) {
            not_in_mb.funcname = ktp->fi->n_name;
            not_in_mb.keystroke = c;
            execfunc = not_in_mb_error;
        }
after_mb_check:
        thisflag = 0;
/* GGR - implement re-execute */
        if (inreex) {
            if ((execfunc == fisearch) || (execfunc == forwsearch))
                execfunc = forwhunt;
            if ((execfunc == risearch) || (execfunc == backsearch))
                execfunc = backhunt;
        }
        else if ((execfunc != reexecute) && (execfunc != nullproc)
                        && (kbdmode != PLAY)) {
            clast = c;
            flast = f;
            nlast = n;
        }

        if (!inmb && kbdmode == RECORD) {
            if (ktp->fi->opt.skip_in_macro) {   /* Quick skip... */
                if (execfunc == namedcmd) {     /* Use next func directly.. */
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

        status = (*execfunc) (f, n);
        input_waiting = NULL;
        if (execfunc != showcpos) lastflag = thisflag;
/* GGR - abort keyboard macro at point of error */
        if ((kbdmode == PLAY) & !status) kbdmode = STOP;
        return status;
    }

/*
 * If a space was typed, fill column is defined, the argument is non-
 * negative, wrap mode is enabled, and we are now past fill column,
 * and we are not read-only, perform word wrap.
 */
    if (c == ' ' && (curwp->w_bufp->b_mode & MDWRAP) && fillcol > 0 &&
          n >= 0 && getccol(FALSE) > fillcol &&
         (curwp->w_bufp->b_mode & MDVIEW) == FALSE)
                execute(META | SPEC | 'W', FALSE, 1);

    if ((c >= 0x20 && c <= 0x7E)    /* Self inserting.      */
#if     IBMPC
        || (c >= 0x80 && c <= 0xFE)) {
#else
#if     BSD || USG       /* 8BIT P.K. */
        || (c >= 0xA0 && c <= 0x10FFFF)) {
#else
        ) {
#endif
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
 * or we are at a tab stop, delete a char forword
 */
        if (curwp->w_bufp->b_mode & MDOVER &&
            curwp->w_doto < curwp->w_dotp->l_used &&
            (lgetc(curwp->w_dotp, curwp->w_doto) != '\t' ||
            (curwp->w_doto) % 8 == 7))
                ldelchar(1, FALSE);

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
    mlwrite(MLpre "Key not bound" MLpost);  /* complain             */
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
            mlwrite(MLpre "Saving %s" MLpost, bp->b_fname);
            mlwrite("\n");              /* So user can see filename */
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
        if (f) exit(n);
        else   exit(GOOD);
    }
    mlwrite("");
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
        mlwrite("%%Macro already active");
        return FALSE;
    }
    if (strcmp(curbp->b_bname, kbdmacro_buffer) == 0) {
        mlwrite("%%Cannot collect macro when in keyboard macro buffer");
        return FALSE;
    }

#if GMLTEST
    for (struct window *wp = wheadp; wp; wp = wp->w_wndp) {
        if (strcmp( wp->w_bufp->b_bname, kbdmacro_buffer) == 0) {
            mlwrite("%%Cannot collect macro while macro buffer is visible");
            return FALSE;
        }
    }
#endif

    mlwrite(MLpre "Start macro" MLpost);
    kbdptr = &kbdm[0];
    kbdend = kbdptr;
    kbdmode = RECORD;
    start_kbdmacro();
    return TRUE;
}

/* ======================================================================
 * End keyboard macro. Check for the same limit conditions as the above
 * routine. Set up the variables and return to the caller.
 */
int ctlxrp(int f, int n) {
    UNUSED(f); UNUSED(n);
    if (kbdmode == STOP) {
        mlwrite("%%Macro not active");
        return FALSE;
    }
    if (kbdmode == RECORD) {
        mlwrite(MLpre "End macro" MLpost);
        end_kbdmacro();
    }
    return TRUE;
}

/* ======================================================================
 * Execute a macro.
 * The command argument is the number of times to loop. Quit as soon as a
 * command gets an error. Return TRUE if all ok, else FALSE.
 */
int ctlxe(int f, int n) {
    UNUSED(f);
    if (kbdmode != STOP) {
        mlwrite("%%Macro already active");
        return FALSE;
    }
    if (n <= 0) return TRUE;
    kbdrep = n;             /* remember how many times to execute */
    kbdmode = PLAY;         /* start us in play mode */
    kbdptr = &kbdm[0];      /*    at the beginning */
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
    mlwrite(MLpre "Aborted" MLpost);
    return ABORT;
}

/* ======================================================================
 * Tell the user that this command is illegal while we are in
 * particular modes.
 */
int rdonly(void) {
    TTbeep();
    mlwrite(MLpre "Key illegal in VIEW mode" MLpost);
    return FALSE;
}
int resterr(void) {
    TTbeep();
    mlwrite(MLpre "That command is RESTRICTED" MLpost);
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

/* ====================================================================== */

/****          Compiler specific Library functions     ****/

#if     RAMSIZE
/*      These routines will allow me to track memory usage by placing
        a layer on top of the standard system malloc() and free() calls.
        with this code defined, the environment variable, $RAM, will
        report on the number of bytes allocated via malloc.

        with SHOWRAM defined, the number is also posted on the
        end of the bottom mode line and is updated whenever it is changed.
*/

#undef  malloc
#undef  free

/* Allocate nbytes and track */
char *allocate(unsigned nbytes) {
    char *mp;               /* ptr returned from malloc */
    char *malloc();

    mp = malloc(nbytes);
    if (mp) {
        envram += nbytes;
#if RAMSHOW
        dspram();
#endif
    }
    return mp;
}

/* Release malloced memory and track */
void release(char *mp) {
    unsigned *lp;           /* ptr to the long containing the block size */

    if (mp) {               /* update amount of ram currently malloced */
        lp = ((unsigned *) mp) - 1;
        envram -= (long) *lp - 2;
        free(mp);
#if RAMSHOW
        dspram();
#endif
    }
}

#if RAMSHOW
/* Display the amount of RAM currently malloced */
void dspram(void) {
    char mbuf[20];
    char *sp;

    TTmove(term.t_nrow - 1, 70);
#if COLOR
    TTforg(7);
    TTbacg(0);
#endif
    sprintf(mbuf, "[%lu]", envram);
    sp = &mbuf[0];
    while (*sp) ttput1c(*sp++);
    TTmove(term.t_nrow, 0);
    movecursor(term.t_nrow, 0);
}
#endif
#endif

/* ======================================================================
 * GGR - Function to implement re-execute last command
 */
int reexecute(int f, int n) {
    UNUSED(f);
    int reloop;
    inreex = TRUE;
/* We can't just multiply n's. Must loop. */
    for (reloop = 1; reloop<=n; ++reloop) {
        execute(clast, flast, nlast);
    }
    inreex = FALSE;
    return(TRUE);
}

/* ======================================================================
 * GGR - Extend the size of the key_table list.
 * If input arg is non-zero use that, otherwise extend by the
 * defined increment and update keytab_alloc_ents.
 */
static struct key_tab endl_keytab = {ENDL_KMAP, 0, {NULL}, NULL};
static struct key_tab ends_keytab = {ENDS_KMAP, 0, {NULL}, NULL};

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
    keytab = realloc(keytab, keytab_alloc_ents*sizeof(struct key_tab));
    if (init_from == 0) {           /* Add in starting data */
        int n_init_keys = sizeof(init_keytab)/sizeof(typeof(init_keytab[0]));
        struct key_tab *ktp = keytab;
        for (int n = 0; n < n_init_keys; n++, ktp++) {
            ktp->k_type = FUNC_KMAP;    /* All init ones are this */
            ktp->k_code = init_keytab[n].k_code;
            ktp->hndlr.k_fp = init_keytab[n].k_fp;
            ktp->fi = func_info(ktp->hndlr.k_fp);
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
