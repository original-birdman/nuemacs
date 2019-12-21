/*      TERMIO.C
 *
 * The functions in this file negotiate with the operating system for
 * characters, and write characters in a barely buffered fashion on the display.
 * All operating systems.
 *
 *      modified by Petri Kutvonen
 */

#ifndef POSIX

#include        <stdio.h>
#include        "estruct.h"
#include        "edef.h"

#if     MSDOS & (MSC | TURBO)
union REGS rg;                  /* cpu register for use of DOS calls */
int nxtchar = -1;               /* character held from type ahead    */
#endif

#if     USG                     /* System V */
#include        <signal.h>
#include        <termio.h>
#include        <fcntl.h>
int kbdflgs;                    /* saved keyboard fd flags      */
int kbdpoll;                    /* in O_NDELAY mode                     */
int kbdqp;                      /* there is a char in kbdq      */
char kbdq;                      /* char we've already read      */
struct termio otermio;          /* original terminal characteristics */
struct termio ntermio;          /* charactoristics to use inside */
#if     XONXOFF
#define XXMASK  0016000
#endif
#endif

#if     V7 | BSD
#include        <sgtty.h>       /* for stty/gtty functions */
#include        <signal.h>
struct sgttyb ostate;           /* saved tty state */
struct sgttyb nstate;           /* values for editor mode */
struct tchars otchars;          /* Saved terminal special character set */
#if     XONXOFF
struct tchars ntchars = { 0xff, 0xff, 0x11, 0x13, 0xff, 0xff };

                                /* A lot of nothing and XON/XOFF */
#else
struct tchars ntchars = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

                                /* A lot of nothing */
#endif
#if BSD
struct ltchars oltchars;        /* Saved terminal local special character set */
struct ltchars nltchars = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

                                /* A lot of nothing */
#endif

#if BSD
#include <sys/ioctl.h>          /* to get at the typeahead */
extern int rtfrmshell();        /* return from suspended shell */
#define TBUFSIZ 128
char tobuf[TBUFSIZ];            /* terminal output buffer */
#endif
#endif

#if     __hpux | SVR4
extern int rtfrmshell();        /* return from suspended shell */
#define TBUFSIZ 128
char tobuf[TBUFSIZ];            /* terminal output buffer */
#endif

/*
 * This function is called once to set up the terminal device streams.
 * On CPM it is a no-op.
 */
void ttopen(void)
{
#if     MSDOS & (TURBO | MSC)
        /* kill the CONTROL-break interupt */
        rg.h.ah = 0x33;         /* control-break check dos call */
        rg.h.al = 1;            /* set the current state */
        rg.h.dl = 0;            /* set it OFF */
        intdos(&rg, &rg);       /* go for it! */
#endif

#if     USG
        ioctl(0, TCGETA, &otermio);     /* save old settings */
        ntermio.c_iflag = 0;            /* setup new settings */
#if     XONXOFF
        ntermio.c_iflag = otermio.c_iflag & XXMASK; /* save XON/XOFF P.K. */
#endif
        ntermio.c_oflag = 0;
        ntermio.c_cflag = otermio.c_cflag;
        ntermio.c_lflag = 0;
        ntermio.c_line = otermio.c_line;
        ntermio.c_cc[VMIN] = 1;
        ntermio.c_cc[VTIME] = 0;
        ioctl(0, TCSETAW, &ntermio);    /* and activate them */
        kbdflgs = fcntl(0, F_GETFL, 0);
        kbdpoll = FALSE;
#endif

#if     V7 | BSD
        gtty(0, &ostate);       /* save old state */
        gtty(0, &nstate);       /* get base of new state */
#if     XONXOFF
        nstate.sg_flags |= (CBREAK | TANDEM);
#else
        nstate.sg_flags |= RAW;
#endif
        nstate.sg_flags &= ~(ECHO | CRMOD); /* no echo for now... */
        stty(0, &nstate);                   /* set mode */
        ioctl(0, TIOCGETC, &otchars);       /* Save old characters */
        ioctl(0, TIOCSETC, &ntchars);       /* Place new character into K */
#if BSD
        ioctl(0, TIOCGLTC, &oltchars);      /* Save old local characters */
        ioctl(0, TIOCSLTC, &nltchars);      /* New local characters */
#endif
#if     BSD
        /* provide a smaller terminal output buffer so that
           the type ahead detection works better (more often) */
        setbuffer(stdout, &tobuf[0], TBUFSIZ);
        signal(SIGTSTP, SIG_DFL);           /* set signals so that we can */
        signal(SIGCONT, rtfrmshell);        /* suspend & restart emacs */
#endif
#endif

#if     __hpux | SVR4
        /* provide a smaller terminal output buffer so that
           the type ahead detection works better (more often) */
        setvbuf(stdout, &tobuf[0], _IOFBF, TBUFSIZ);
        signal(SIGTSTP, SIG_DFL);           /* set signals so that we can */
        signal(SIGCONT, rtfrmshell);        /* suspend & restart emacs */
        TTflush();
#endif                          /* __hpux */

        /* on all screens we are not sure of the initial position
           of the cursor                                        */
        ttrow = 999;
        ttcol = 999;
}

/*
 * This function gets called just before we go back home to the command
 * interpreter.
 * Another no-operation on CPM.
 */
void ttclose(void)
{
#if     MSDOS & (TURBO | MSC)
        /* restore the CONTROL-break interupt */
        rg.h.ah = 0x33;         /* control-break check dos call */
        rg.h.al = 1;            /* set the current state */
        rg.h.dl = 1;            /* set it ON */
        intdos(&rg, &rg);       /* go for it! */
#endif

#if     USG
        ioctl(0, TCSETAW, &otermio);    /* restore terminal settings */
        fcntl(0, F_SETFL, kbdflgs);
#endif

#if     V7 | BSD
        stty(0, &ostate);
        ioctl(0, TIOCSETC, &otchars);   /* Place old character into K */
        ioctl(0, TIOCSLTC, &oltchars);  /* Place old local character into K */
#endif
}

/*
 * Write a character to the display.
 * On CPM terminal I/O unbuffered, so we just write the byte out. Ditto on
 * MS-DOS (use the very very raw console output routine).
 */
void ttputc(char c)
{
#if     MSDOS & ~IBMPC
        bdos(6, c, 0);
#endif

#if     V7 | USG | BSD
        fputc(c, stdout);
#endif
}

/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
int ttflush(void)
{
#if     MSDOS
#endif

#if     V7 | USG | BSD
/*
 * Add some terminal output success checking, sometimes an orphaned
 * process may be left looping on SunOS 4.1.
 *
 * How to recover here, or is it best just to exit and lose
 * everything?
 *
 * jph, 8-Oct-1993
 */

#include <errno.h>

        int status;

        status = fflush(stdout);

        if (status != 0 && errno != EAGAIN) {
                exit(errno);
        }
#endif
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all. Very simple on CPM, because the system can do exactly what you want.
 */
int ttgetc(void)
{
#if     MSDOS & (MSC | TURBO)
        int c;                  /* character read */

        /* if a char already is ready, return it */
        if (nxtchar >= 0) {
                c = nxtchar;
                nxtchar = -1;
                return c;
        }

        /* call the dos to get a char */
        rg.h.ah = 7;            /* dos Direct Console Input call */
        intdos(&rg, &rg);
        c = rg.h.al;            /* grab the char */
        return c & 255;
#endif

#if     V7 | BSD
        return 255 & fgetc(stdin);      /* 8BIT P.K. */
#endif

#if     USG
        if (kbdqp)
                kbdqp = FALSE;
        else {
                if (kbdpoll && fcntl(0, F_SETFL, kbdflgs) < 0)
                        return FALSE;
                kbdpoll = FALSE;
                while (read(0, &kbdq, 1) != 1);
        }
        return kbdq & 255;
#endif
}

#if     TYPEAH
/* typahead:    Check to see if any characters are already in the
                keyboard buffer
*/

int typahead(void)
{
#if     MSDOS & (MSC | TURBO)
        if (kbhit() != 0)
                return TRUE;
        else
                return FALSE;
#endif

#if     BSD
        int x;                  /* holds # of pending chars */

        return (ioctl(0, FIONREAD, &x) < 0) ? 0 : x;
#endif

#if     USG
        if (!kbdqp) {
                if (!kbdpoll && fcntl(0, F_SETFL, kbdflgs | O_NDELAY) < 0)
                        return FALSE;
                kbdpoll = 1;
                kbdqp = (1 == read(0, &kbdq, 1));
        }
        return kbdqp;
#endif

#if !UNIX & !MSDOS
        return FALSE;
#endif
}
#endif

#endif                          /* not POSIX */
