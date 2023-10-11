#include "../r2_strings.h"
#include "../r2_unit.h"

#include <stdlib.h>
#include <stdio.h>

#if (__WORDSIZE == 64)
#define BUILD_64 1
#endif


static char* test_create_string()
{
    s8 str = S("This is a test");
    r2_assert("string length is wrong", (str.len == 14));
    r2_assert("string size is wrong", (str.size == 14));
    return 0;
}

static char* test_create_string_blank()
{
    s8 str = S("");
    r2_assert("string length is wrong", (str.len == 0));
    r2_assert("string size is wrong", (str.size == 0));
    return 0;
}

static char* test_create_string_null()
{
    char *str1 = (char *)malloc(sizeof(char *) * 10);
    s8 str = S(str1);

    r2_assert("null string length is wrong", (str.len == 0));
    r2_assert("null string size is wrong", (str.size == 0));
    return 0;
}

static char* test_create_utf8_string()
{
    s8 str = S("👋👏他是我的😺");
    r2_assert("utf8 string length is wrong", (str.len == 7));
    r2_assert("utf8 string size is wrong", (str.size == 24));
    return 0;
}

////////////////////////////////////////////

static char *r2_strings_test()
{
    r2_run_test(test_create_string);
    r2_run_test(test_create_utf8_string);
    r2_run_test(test_create_string_blank);
    r2_run_test(test_create_string_null);
    return 0;
}
