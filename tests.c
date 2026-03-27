//
// This is the entry file into all the tests
//

#include "r2_unit.h"
#include <stdio.h>
#include <string.h>

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
static struct { const char *name; const char *(*fn)(void); } suites[] = {
    { "termui",  r2_termui_test  },
    { "maths",   r2_maths_test   },
    { "strings", r2_strings_test },
};
///////////////////////////////////////////////

//
// Running
//
int r2_tests_run = 0;

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

static int run_suite(const char *name)
{
    int c = (int)(sizeof suites / sizeof suites[0]);
    for (int x = 0; x < c; x++)
    {
        if (strcmp(name, suites[x].name) == 0)
        {
            const char *error = suites[x].fn();
            if (error != 0)
            {
                char f[100];
                snprintf(f, 100, "FAIL: %s", error);
                test_error(f);
                return 1;
            }
            return 0;
        }
    }
    fprintf(stderr, "Unknown suite '%s'. Available:", name);
    for (int x = 0; x < c; x++) fprintf(stderr, " %s", suites[x].name);
    fprintf(stderr, "\n");
    return 1;
}

static int run_all(void)
{
    int c = (int)(sizeof suites / sizeof suites[0]);
    for (int x = 0; x < c; x++)
    {
        const char *error = suites[x].fn();
        if (error != 0)
        {
            char f[100];
            snprintf(f, 100, "FAIL: %s", error);
            test_error(f);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int failed = (argc > 1) ? run_suite(argv[1]) : run_all();
    test_debug("\n");
    if (!failed)
    {
        test_debug("ALL TESTS PASSED");
        test_debug("-------------------------");
        char p[20];
        snprintf(p, 20, "Tests run: %d\n", r2_tests_run);
        test_debug(p);
    }
    return failed;
}
