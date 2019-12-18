#include <stdio.h>
#include "version.h"

#include <string.h>

#include "utf8proc.h"

/* How to Stringize a defined parameter...*/
#define str(s) #s
#define xstr(s) str(s)

void version(void) {
    printf("%s version %s\n", PROGRAM_NAME_LONG, VERSION);
#if defined(__GNUC__)
    printf(" Compiled on %s, by GCC %s, on %s at %s\n",
        xstr(BUILDER), __VERSION__, __DATE__, __TIME__);
#else
    printf(" Compiled on %s\n", xstr(BUILDER));
#endif
    printf(" utf8proc version %s\n", utf8proc_version());

/* Try to get the libc version...ripped from BOINC code. */

    FILE* f = popen("ldd --version 2>/dev/null", "r");
    if (f) {
        char buf[128];
        char xbuf[128];
        char* retval = fgets(buf, sizeof(buf), f);
/* Consume output to allow command to exit gracefully */
        while (fgets(xbuf, sizeof(xbuf), f));
        (void)pclose(f);    /* Ignore the status, though */
/* Now print the info - just remove the leading "ldd " quickly */
        if (retval && strlen(retval) > 4)
              printf(" libc version %s", retval+4);
    }
}
