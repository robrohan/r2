//
// This is the entry file into all the tests
//

#include "r2_unit.h"
#include <stdio.h>

///////////////////////////////////////////////
// ADD TESTS HERE
// This is crazy town :-o
// List C includes here and conventionally call the
// function <whatever>_test() in the all tests() method
///////////////////////////////////////////////
#include "tests/r2_maths.c"
///////////////////////////////////////////////
// Add suites here...
// Defined in the tests files above
char *(*s[1])(void) = {r2_maths_test};
///////////////////////////////////////////////

//
// Running
//

int r2_tests_run = 0;

static char *all_tests()
{
    int c = sizeof s / sizeof(s)[0];

    for (int x = 0; x < c; x++)
    {
        char *error = (*s[x])();
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
    // at_error(test_error);
    // at_debug(test_debug);

    char *result = all_tests();
    printf("\n\n");
    if (result != 0)
    {
        printf("FAIL: %s\n", result);
    }
    else
    {
        printf("ALL TESTS PASSED\n");
    }
    printf("-------------------------\n");
    printf("Tests run: %d\n\n", r2_tests_run);
    // To have Make not get mad about error state
    return !(NULL == result);
}
