//
// This is the entry file into all the tests
//

#include "r2_unit.h"
#include <stdio.h>

#define R2_MATHS_IMPLEMENTATION
#include "r2_maths.h"

#define R2_STRINGS_IMPLEMENTATION
#include "r2_strings.h"

///////////////////////////////////////////////
// ADD TESTS HERE
// List C includes here and conventionally call the
// function <whatever>_test() in the all tests() method
///////////////////////////////////////////////
#include "tests/r2_maths.c"
#include "tests/r2_strings.c"
#include "tests/r2_termui.c"
///////////////////////////////////////////////
// Add suites here...
// Defined in the tests files above
const char *(*s[3])(void) = {r2_termui_test, r2_maths_test, r2_strings_test};
///////////////////////////////////////////////

//
// Running
//
int r2_tests_run = 0;

static const char *all_tests(void)
{
    int c = sizeof s / sizeof(s)[0];

    for (int x = 0; x < c; x++)
    {
        const char *error = (*s[x])();
        if (error != 0)
        {
            return error;
        }
    }
    return 0;
}

static void test_error(const char *str)
{
    fprintf(stderr, "%s\n", str);
    fflush(stderr);
}

static void test_debug(const char *str)
{
    fprintf(stdout, "%s\n", str);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *result = all_tests();
    test_debug("\n");
    if (result != 0)
    {
        char f[100];
        snprintf(f, 100, "FAIL: %s", result);
        test_error(f);
        return 0;
    }
    else
    {
        test_debug("ALL TESTS PASSED");
    }
    test_debug("-------------------------");

    char p[20];
    snprintf(p, 20, "Tests run: %d\n", r2_tests_run);
    test_debug(p);
    // To have Make not get mad about error state
    return !(NULL == result);
}
