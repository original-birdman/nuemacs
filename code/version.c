#include <stdio.h>
#include "version.h"

#include <string.h>

#include "utf8proc.h"

void version(void) {
    printf("%s version %s\n", PROGRAM_NAME_LONG, VERSION);
#if defined(__clang_version__)
    printf("Compiled on %s, by %s, on %s at %s\n",
        xstr(BUILDER), __VERSION__, __DATE__, __TIME__);
#elif defined(__GNUC__)
    printf("Compiled on %s, by GCC %s, on %s at %s\n",
        xstr(BUILDER), __VERSION__, __DATE__, __TIME__);
#else
    printf(" Compiled on %s\n", xstr(BUILDER));
#endif

#ifdef NUTRACE
    printf(" Stack dumping available\n");
#endif

/* STATLIB is 1 for all dynamic, -1 for libc/m only and 0 for all static */
#if STAT_LIB > 0
#define UTF8LIB "dynamic"
#else
#define UTF8LIB "static"
#endif
    const char *utf8vp = utf8proc_version();
    printf("Running with\n (" UTF8LIB ") utf8proc version %s", utf8vp);
/* Assume we have utf8proc_unicode_version() */
// This is the code if we want handle really old libs without it.
// Needs -ldl
// #include <dlfcn.h>
//    char *(*sym)(void) = dlsym(RTLD_DEFAULT, "utf8proc_unicode_version");
//    if (sym) printf(", supports Unicode %s", sym());
    printf(", supports Unicode %s", utf8proc_unicode_version());
    printf("\n");

/* Try to get the libc version...ripped from BOINC code.
 * But if this build is using a static libc, we'll have been sent
 * the version as an option.
 */

#if STAT_LIB == 0
    printf(" (static) libc version " xstr(LCV) "\n");
#else
    FILE* f = popen("ldd --version 2>/dev/null", "r");
    if (f) {
        char buf[128];
        char xbuf[128];
        char* retval = fgets(buf, sizeof(buf), f);
/* Consume output to allow command to exit gracefully */
        while (fgets(xbuf, sizeof(xbuf), f));
        (void)pclose(f);    /* Ignore the status, though */
/* Now print the info - just remove the leading "ldd " quickly and
 * use the trailing newline we know is there
 */
        if (retval && strlen(retval) > 4)
              printf(" (dynamic) libc version %s", retval+4);
    }
#endif
}
