/* combi-class.c
 *  Copyleft(c) 2020 - G.M.Lack
 * 
 * Produce an include file for uemacs defining the range of characters
 * that have a non-zero Combining Class in utf8proc.
 * We also produce a parallel file listing that actual values.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "utf8proc.h"

int main(int argc, char *argv[]) {

#define xpand_last(desc) desc[maxi(desc)] = malloc(32);

    utf8proc_int32_t uc;
    int combining_class, utf8width;
    int in_combi_class = 0;
    utf8proc_int32_t combi_class_start = -1;

    FILE *headfh, *infofh;
    
    if (!(headfh = fopen("combi.h", "w"))) {
        perror("open combi.h");
        return errno;
    }
    if (!(infofh = fopen("combi.info", "w"))) {
        perror("open combi.info");
        return errno;
    }

/* Get the version and generating time.
 * ctime has a trailing newline, so remove it...
 */

    const char *version = utf8proc_version();
    time_t epoch = time(NULL);
    const char *now_nl = ctime(&epoch);
    char now[64];
    strcpy(now, now_nl);
    now[strlen(now)-1] = '\0'; 

/* Start with the headers for each file */

    fprintf(headfh,
"/* combi.h\n"
" * utf8proc Combining Class ranges for version %s.\n"
" * Generated at %s.\n"
" *\n"
" * ANNOTATE any hand-made alterations!!\n"
" */\n"
"#ifndef COMBI_H\n"
"#define COMBI_H\n"
"\n"
"static struct range_t {\n"
"    unicode_t start;\n"
"    unicode_t end;\n"
"} const combi_range[] = { /* THIS LIST MUST BE IN SORTED ORDER!! */\n",
    version, now);

    fprintf(infofh,
"/* combi.info\n"
" * utf8proc Combining Class values for version %s.\n"
" * Generated at %s.\n"
" *\n"
" *  codepoint -> C-Class(hex) [width]\n"
" */\n",
    version, now);

    for (uc = 0; uc <= 0x10FFFF; uc++) {
        combining_class = utf8proc_get_property(uc)->combining_class;
/* Might still be the case. Add all Me or Mn types too */
        if (!combining_class) {
            const char *cs = utf8proc_category_string(uc);
            combining_class = (cs[0] == 'M' &&
                 ((cs[1] == 'n') || (cs[1] == 'e')));
        }
        utf8width = utf8proc_charwidth(uc);
        if (!in_combi_class && (combining_class > 0)) {
            combi_class_start = uc;
            in_combi_class = 1;
        }
        else if (in_combi_class && (combining_class == 0)) {
            fprintf(headfh, "    {0x%04x, 0x%04x},\n",
                 combi_class_start, uc - 1);
            in_combi_class = 0;
        }
        if (in_combi_class) {
            fprintf(infofh, " 0x%04x -> %d(0x%04x) [%d]\n",
                 uc, combining_class, combining_class, utf8width);
        }
    }

/* Now add the endings...must put the UEM_NOCHAR endof list markers
 * in the header file
 */

    fprintf(headfh,
"    {UEM_NOCHAR, UEM_NOCHAR}    /* End of list marker */\n"
"};\n"
"#endif\n");

    fclose(headfh);
    fclose(infofh);
    return 0;
}
