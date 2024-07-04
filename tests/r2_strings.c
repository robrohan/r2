#include "../r2_strings.h"
#include "../r2_unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>

#if (__WORDSIZE == 64)
#define BUILD_64 1
#endif

static const char *test_create_string(void)
{
    s8 str = S("This is a test");
    r2_assert("string length is wrong", (str.len == 14));
    r2_assert("string size is wrong", (str.size == 14));
    free_S(str);
    return 0;
}

static const char *test_create_string_blank(void)
{
    s8 str = S("");
    r2_assert("string length is wrong", (str.len == 0));
    r2_assert("string size is wrong", (str.size == 0));
    free_S(str);
    return 0;
}

static const char *test_create_string_null(void)
{
    char *str1 = (char *)malloc(sizeof(char *) * 10);
    s8 str = S(str1);

    r2_assert("null string length is wrong", (str.len == 0));
    r2_assert("null string size is wrong", (str.size == 0));
    free_S(str);
    return 0;
}

static const char *test_create_utf8_string(void)
{
    s8 str = S("👋👏他是我的😺");
    r2_assert("utf8 string length is wrong", (str.len == 7));
    r2_assert("utf8 string size is wrong", (str.size == 24));
    r2_assert("utf8 rune 0 is wrong", (str.rune[0] == 0x1F44B));
    r2_assert("utf8 rune 1 is wrong", (str.rune[1] == 0x1F44F));
    r2_assert("utf8 rune 6 is wrong", (str.rune[6] == 0x1F63A));
    r2_assert("utf8 rune 7 is wrong", (str.rune[7] == 0x0));
    free_S(str);
    return 0;
}

static const char *test_print_utf8_string(void)
{
    s8 str = S("👋😺👏🏳️‍🌈👨‍👩‍👦");
    setlocale(LC_ALL, "");
    for (unsigned int i = 0; i < str.len; i++)
    {
        printf("%lc", (wint_t)str.rune[i]);
    }
    free_S(str);
    r2_assert("", (0x1 == 0x1));
    return 0;
}

/// Joiner Tests ///

static const char *test_create_utf8_string_mod(void)
{
    /* An Emoji Modifier Sequence is created automatically when a
    modifier character is inserted after a supported base emoji
    character, resulting in a single emoji with skin tone.
    Many emojis with skin tones are Emoji ZWJ Sequences, due to the
    presence of additional characters for gender, multiple people,
    or a combination of people and objects.
    */

    /*
    _Zero Width Joiner_
        zero-width joiner (ZWJ, /ˈzwɪdʒ/)[1] (&#8205;)

        Zero Width Joiner (ZWJ) is a Unicode character that joins
        two or more other characters together in sequence to create
        a new emoji.
        Zero Width Joiner, pronounced "zwidge", is not an emoji and
        has no appearance by itself. This is an invisible character
        when used alone.
        (Zero Width Joiner was approved as part of Unicode 1.1 in 1993
        and added to Emoji 11.0 in 2018.)
    */

    /* ZWJ = 0xe2 0x80 0x8d */

    /* The Thumbs Up: Medium Skin Tone emoji is a modifier
    sequence combining 👍 Thumbs Up and 🏽 Medium Skin Tone.
    These display as a single emoji on supported platforms.
                       4     7      11     14   18
    Another example: [Man] [ZWJ] [Woman] [ZWJ] [Boy] \0
                       0    1     2       3      4    5

    0xf0 0x9f 0x91 0xa8    <= man
    0xe2 0x80 0x8d         <= joiner
    0xf0 0x9f 0x91 0xa9    <= woman
    0xe2 0x80 0x8d         <= joiner
    0xf0 0x9f 0x91 0xa6    <= boy
    */
    s8 str = S("👨‍👩‍👦");
    r2_assert("utf8 mod string length is wrong", (str.len == 5));
    r2_assert("utf8 mod string size is wrong", (str.size == 18));
    free_S(str);
    return 0;
}

static const char *test_create_utf8_string_mod2(void)
{
    /*
     Waving white flag has 2 code points!
     https://emojipedia.org/white-flag#technical

               4           7    10      14
     [Waving white flag] [VS] [ZWJ] [Rainbow] \0
               0           1     2       3

    0xf0 0x9f 0x8f 0xb3                  <= waving white flag
    0xef 0xb8 0x8f                       <= variation selector-16
    0xe2 0x80 0x8d                       <= joiner
    0xf0 0x9f 0x8c 0x88                  <= rainbow
    */
    s8 str = S("🏳️‍🌈");
    r2_assert("utf8 mod2 string length is wrong", (str.len == 4));
    r2_assert("utf8 mod2 string size is wrong", (str.size == 14));
    free_S(str);
    return 0;
}

////////////////////////////////////////////

static const char *r2_strings_test(void)
{
    r2_run_test(test_create_string);
    r2_run_test(test_create_utf8_string);
    r2_run_test(test_create_utf8_string_mod);
    r2_run_test(test_create_utf8_string_mod2);
    r2_run_test(test_create_string_blank);
    r2_run_test(test_create_string_null);
    r2_run_test(test_print_utf8_string);
    return 0;
}
