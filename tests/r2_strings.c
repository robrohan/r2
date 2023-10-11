#include "../r2_strings.h"
#include "../r2_unit.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if (__WORDSIZE == 64)
#define BUILD_64 1
#endif


static char* test_create_string()
{
    s8 str = S("This is a test");

    r2_assert("string length is wrong", (str.len == 14));

    return 0;
}


static char *r2_strings_test()
{
    r2_run_test(test_create_string);

    return 0;
}
