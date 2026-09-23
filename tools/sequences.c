/* Dump out the relevant termcap sequences for a TERM setting.
 *
 *  Compile by:
 *      gcc sequences.c -ltinfo -o sequences
 *
 *  Usage:
 *      ./sequences term-name
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <curses.h>
#include <term.h>

/* Output control chars as \X
 * (if we displayed the strings directly they'd carry out their action!)
 */
static char *unesc(const char *str) {
    static char ebuf[1024];

    if (!str) return "(null)";
    char *op = ebuf;
    while(*str) {
        if (*str < ' ') {
            *op++ = '\\';
            *op++ = *str + '@';
        }
        else if (*str == 0x7f) {
            *op++ = '\\';
            *op++ = '?';
        }
        else *op++ = *str;
        str++;
    }
    *op = '\0';
    return ebuf;
}

/* Main program */

int main(int argc, char *argv[]) {
    char *tv_stype;

    if (!argv[1]) {
        fprintf(stderr, "Usage: %s termnal-type\n", argv[0]);
        return 1;
    }

    if ((tgetent(NULL, argv[1])) != 1) {
/* Handle overlong TERM settings. Only print the first 40 chars */
        const char *xtra = "";
        if (strlen(argv[1]) > 40) xtra = "...";
        fprintf(stderr, "Unknown terminal type: %.40s%s!", argv[1], xtra);
        exit(1);
    }

    char *PC = tgetstr("pc", NULL);
    printf("PC: %s\n", unesc(PC));
    char *CL = tgetstr("cl", NULL);
    printf("CL: %s\n", unesc(CL));
    char *CM = tgetstr("cm", NULL);
    printf("CM: %s\n", unesc(CM));
    char *CE = tgetstr("ce", NULL);
    printf("CE: %s\n", unesc(CE));
    char *UP = tgetstr("up", NULL);
    printf("UP: %s\n", unesc(UP));
    char *SE = tgetstr("se", NULL);
    printf("SE: %s\n", unesc(SE));
    char *SO = tgetstr("so", NULL);
    printf("SO: %s\n", unesc(SO));
    char *TI = tgetstr("ti", NULL);
    printf("TI: %s\n", unesc(TI));
    char *TE = tgetstr("te", NULL);
    printf("TE: %s\n", unesc(TE));
    char *_CS = tgetstr("cs", NULL);
    printf("_CS: %s\n", unesc(_CS));
    char *SF = tgetstr("sf", NULL);
    printf("SF: %s\n", unesc(SF));
    char *SR = tgetstr("sr", NULL);
    printf("SR: %s\n", unesc(SR));
    char *DL = tgetstr("dl", NULL);
    printf("DL: %s\n", unesc(DL));
    char *AL = tgetstr("al", NULL);
    printf("AL: %s\n", unesc(AL));


    if (CM) {
        printf("CM 1,1 gives %s\n", unesc(tgoto(CM, 1, 1)));
        printf("CM 1,20 gives %s\n", unesc(tgoto(CM, 1, 20)));
    }
    if (_CS) {
        printf("_CS 1,1 gives %s\n", unesc(tgoto(_CS, 1, 1)));
        printf("_CS 1,20 gives %s\n", unesc(tgoto(_CS, 1, 20)));
    }

    return 0;
}
