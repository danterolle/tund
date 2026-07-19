#include "sitest.h"

#include <stdio.h>

int sitest_failures = 0;

void sitest_check(bool ok, const char *file, int line, const char *expr)
{
    if (!ok) {
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expr);
        sitest_failures++;
    }
}

int sitest_finish(const char *suite_name)
{
    if (sitest_failures != 0) {
        fprintf(stderr, "%d %s failed\n", sitest_failures, suite_name);
        return 1;
    }

    printf("%s passed\n", suite_name);
    return 0;
}
