/*      FILEIO.C
 *
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here.
 *
 *      modified by Petri Kutvonen
 */

#include        <stdio.h>
#include        "estruct.h"
#include        "edef.h"
#include        "efunc.h"

static FILE *ffp;                       /* File pointer, all functions. */
static int eofflag;                     /* end-of-file flag */

/*
 * Open a file for reading.
 */
int ffropen(char *fn)
{
#if (BSD | USG)
#if EXPAND_TILDE
        expand_tilde(fn);
#endif
#if EXPAND_SHELL
        expand_shell(fn);
#endif
#endif
        if ((ffp = fopen(fn, "r")) == NULL)
                return FIOFNF;
        if (pathexpand) {       /* GGR */
                strcpy(curbp->b_fname, fn);
        }
        eofflag = FALSE;
        return FIOSUC;
}

/*
 * Open a file for writing. Return TRUE if all is well, and FALSE on error
 * (cannot create).
 */
int ffwopen(char *fn)
{
#if (BSD | USG)
#if EXPAND_TILDE
        expand_tilde(fn);
#endif
#if EXPAND_SHELL
        expand_shell(fn);
#endif         
#endif
#if     VMS
        int fd;

        if ((fd = creat(fn, 0666, "rfm=var", "rat=cr")) < 0
            || (ffp = fdopen(fd, "w")) == NULL) {
#else
        if ((ffp = fopen(fn, "w")) == NULL) {
#endif
                mlwrite("Cannot open file for writing");
                return FIOERR;
        }
        if (pathexpand) {       /* GGR */
                strcpy(curbp->b_fname, fn);
        }       
        return FIOSUC;
}

/*
 * Close a file. Should look at the status in all systems.
 */
int ffclose(void)
{
        /* free this since we do not need it anymore */
        if (fline) {
                free(fline);
                fline = NULL;
        }
        eofflag = FALSE;

#if     MSDOS & CTRLZ
        fputc(26, ffp);         /* add a ^Z at the end of the file */
#endif

#if     V7 | USG | BSD | (MSDOS & (MSC | TURBO))
        if (fclose(ffp) != FALSE) {
                mlwrite("Error closing file");
                return FIOERR;
        }
        return FIOSUC;
#else
        fclose(ffp);
        return FIOSUC;
#endif
}

/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
int ffputline(char *buf, int nbuf)
{
        int i;
#if     CRYPT
        char c;                 /* character to translate */

        if (cryptflag) {
                for (i = 0; i < nbuf; ++i) {
                        c = buf[i] & 0xff;
                        myencrypt(&c, 1);
                        fputc(c, ffp);
                }
        } else
                for (i = 0; i < nbuf; ++i)
                        fputc(buf[i] & 0xFF, ffp);
#else
        for (i = 0; i < nbuf; ++i)
                fputc(buf[i] & 0xFF, ffp);
#endif

        fputc('\n', ffp);

        if (ferror(ffp)) {
                mlwrite("Write I/O error");
                return FIOERR;
        }

        return FIOSUC;
}

/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 */
int ffgetline(void)
{
        int c;          /* current character read */
        int i;          /* current index into fline */

        /* if we are at the end...return it */
        if (eofflag)
                return FIOEOF;

        /* dump fline if it ended up too big */
        if (flen > NSTRING) {
                free(fline);
                fline = NULL;
        }

        /* if we don't have an fline, allocate one */
        if (fline == NULL)
                if ((fline = malloc(flen = NSTRING)) == NULL)
                        return FIOMEM;

        /* read the line in */
#if     PKCODE
        if (!nullflag) {
                if (fgets(fline, NSTRING, ffp) == (char *) NULL) {      /* EOF ? */
                        i = 0;
                        c = EOF;
                } else {
                        i = strlen(fline);
                        c = 0;
                        if (i > 0) {
                                c = fline[i - 1];
                                i--;
                        }
                }
        } else {
                i = 0;
                c = fgetc(ffp);
        }
        while (c != EOF && c != '\n') {
#else
        i = 0;
        while ((c = fgetc(ffp)) != EOF && c != '\n') {
#endif
#if     PKCODE
                if (c) {
#endif
                        fline[i++] = c;
                        /* if it's longer, get more room */
                        if (i >= flen) {
                                flen += NSTRING;
                                fline = realloc(fline, flen);
                        }
#if     PKCODE
                }
                c = fgetc(ffp);
#endif
        }

        /* test for any errors that may have occured */
        if (c == EOF) {
                if (ferror(ffp)) {
                        mlwrite("File read error");
                        return FIOERR;
                }

                if (i != 0)
                        eofflag = TRUE;
                else
                        return FIOEOF;
        }

        /* terminate and decrypt the string */
        fline[i] = 0;
/* GGR - record the real size of the line */
        ftrulen = i;

#if     CRYPT
        if (cryptflag)
                myencrypt(fline, strlen(fline));
#endif
        return FIOSUC;
}

/*
 * does <fname> exist on disk?
 *
 * char *fname;         file to check for existance
 */
int fexist(char *fname)
{
        FILE *fp;
#if (BSD | USG)
#if EXPAND_TILDE
        expand_tilde(fname);
#endif
#if EXPAND_SHELL
        expand_shell(fname);
#endif      
#endif
        /* try to open the file for reading */
        fp = fopen(fname, "r");

        /* if it fails, just return false! */
        if (fp == NULL)
                return FALSE;

        /* otherwise, close it and report true */
        fclose(fp);
        return TRUE;
}

#if BSD | USG

#if EXPAND_SHELL
void expand_shell(char *fn)
{
    char *ptr;
    char command[NFILEN];
    int c;
    FILE *pipe;
    
    if (strchr(fn, '$') == NULL)    /* Don't bother if no $s */
        return;
    strcpy(command, "echo ");  /* Not all systems have -n, sadly */
    strcat(command, fn);
    if ((pipe = popen(command, "r")) == NULL)
        return;
    ptr = fn;
    while ((c = getc(pipe)) != EOF && c != 012)
        *ptr++ = c;
    *ptr = 0;
    pclose(pipe);
    return;
}
#endif

#if EXPAND_TILDE
#include <stdlib.h>
#include <pwd.h>
void expand_tilde(char *fn)
{       
    char tilde_copy[NFILEN];
    char *p, *q;
    short i;
    struct passwd *pwptr;
                
    if (fn[0]=='~') {
        if (fn[1]=='/' || fn[1]==0) {
            if ((p = getenv("HOME")) != NULL) {
                strcpy(tilde_copy, fn);
                strcpy(fn , p);
                i = 1;  
                /* special case for root! */
                if (fn[0]=='/' && fn[1]==0 && tilde_copy[1] != 0)
                    i++;
                strcat(fn, &tilde_copy[i]);
            }
        }
        else {
            p = fn + 1;
            q = tilde_copy;
            while (*p != 0 && *p != '/')
                *q++ = *p++;
            *q = 0;
            if ((pwptr = getpwnam(tilde_copy)) != NULL) {
                strcpy(tilde_copy, pwptr->pw_dir);
                strcat(tilde_copy, p);
                strcpy(fn, tilde_copy);
            }
        }       
    }           
    return;     
}               
#endif              
                
#endif
