#include <stdio.h>
#include "version.h"

#include "utf8proc.h"

void version(void) {
    printf("%s version %s\n", PROGRAM_NAME_LONG, VERSION);
    printf(" utf8proc version %s\n", utf8proc_version());
}
