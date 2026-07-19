#ifndef SITEST_H
#define SITEST_H

#include <stdbool.h>

extern int sitest_failures;

#define CHECK(expr) sitest_check((expr), __FILE__, __LINE__, #expr)

void sitest_check(bool ok, const char *file, int line, const char *expr);
int sitest_finish(const char *suite_name);

#endif /* SITEST_H */
