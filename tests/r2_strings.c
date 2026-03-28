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
    r2_assert("string cap is wrong", (str.cap == 14));
    free_S(str);
    return 0;
}

static const char *test_create_string_blank(void)
{
    s8 str = S("");
    r2_assert("string length is wrong", (str.len == 0));
    r2_assert("string cap is wrong", (str.cap == 0));
    free_S(str);
    return 0;
}

static const char *test_create_string_empty(void)
{
    const char *str1 = "";
    s8 str = S(str1);

    r2_assert("empty string length is wrong", (str.len == 0));
    r2_assert("empty string cap is wrong", (str.cap == 0));
    free_S(str);
    return 0;
}

static const char *test_create_string_null(void)
{
    s8 str = S(NULL);

    r2_assert("null string length is wrong", (str.len == 0));
    r2_assert("null string cap is wrong", (str.cap == 0));
    free_S(str);
    return 0;
}

static const char *test_create_utf8_string(void)
{
    s8 str = S("👋👏他是我的😺");
    r2_assert("utf8 string length is wrong", (str.len == 7));
    r2_assert("utf8 string cap is wrong", (str.cap == 24));
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

/// 2-byte UTF-8 (U+0080..U+07FF) ///

static const char *test_two_byte_chars(void)
{
    /* é = U+00E9, encoded as 0xC3 0xA9 (2 bytes)
       ñ = U+00F1, encoded as 0xC3 0xB1 (2 bytes) */
    s8 str = S("éñ");
    r2_assert("2-byte: cap wrong", (str.cap == 4));
    r2_assert("2-byte: len wrong",  (str.len  == 2));
    r2_assert("2-byte: rune[0] wrong", (str.rune[0] == 0x00E9));
    r2_assert("2-byte: rune[1] wrong", (str.rune[1] == 0x00F1));
    r2_assert("2-byte: rune[2] sentinel wrong", (str.rune[2] == 0x0));
    free_S(str);
    return 0;
}

static const char *test_two_byte_single(void)
{
    /* Single 2-byte char: ü = U+00FC, 0xC3 0xBC */
    s8 str = S("ü");
    r2_assert("2-byte single: cap wrong", (str.cap == 2));
    r2_assert("2-byte single: len wrong",  (str.len  == 1));
    r2_assert("2-byte single: rune[0] wrong", (str.rune[0] == 0x00FC));
    free_S(str);
    return 0;
}

/// 3-byte UTF-8 (U+0800..U+FFFF) ///

static const char *test_three_byte_chars(void)
{
    /* 中 = U+4E2D, 0xE4 0xB8 0xAD (3 bytes)
       文 = U+6587, 0xE6 0x96 0x87 (3 bytes) */
    s8 str = S("中文");
    r2_assert("3-byte: cap wrong", (str.cap == 6));
    r2_assert("3-byte: len wrong",  (str.len  == 2));
    r2_assert("3-byte: rune[0] wrong", (str.rune[0] == 0x4E2D));
    r2_assert("3-byte: rune[1] wrong", (str.rune[1] == 0x6587));
    r2_assert("3-byte: rune[2] sentinel wrong", (str.rune[2] == 0x0));
    free_S(str);
    return 0;
}

static const char *test_three_byte_single(void)
{
    /* ∑ = U+2211, 0xE2 0x88 0x91 */
    s8 str = S("∑");
    r2_assert("3-byte single: cap wrong", (str.cap == 3));
    r2_assert("3-byte single: len wrong",  (str.len  == 1));
    r2_assert("3-byte single: rune[0] wrong", (str.rune[0] == 0x2211));
    free_S(str);
    return 0;
}

/// Mixed-width strings ///

static const char *test_mixed_ascii_and_multibyte(void)
{
    /* "Hi中" = H(1) + i(1) + 中(3) = 5 bytes, 3 runes */
    s8 str = S("Hi中");
    r2_assert("mixed: cap wrong", (str.cap == 5));
    r2_assert("mixed: len wrong",  (str.len  == 3));
    r2_assert("mixed: rune[0] wrong", (str.rune[0] == 'H'));
    r2_assert("mixed: rune[1] wrong", (str.rune[1] == 'i'));
    r2_assert("mixed: rune[2] wrong", (str.rune[2] == 0x4E2D));
    r2_assert("mixed: rune[3] sentinel wrong", (str.rune[3] == 0x0));
    free_S(str);
    return 0;
}

static const char *test_mixed_all_widths(void)
{
    /* One char of each byte-width class:
       A   = U+0041 (1 byte)
       é   = U+00E9 (2 bytes)
       中  = U+4E2D (3 bytes)
       👋  = U+1F44B (4 bytes)
       total: 10 bytes, 4 runes */
    s8 str = S("Aé中👋");
    r2_assert("all-widths: cap wrong", (str.cap == 10));
    r2_assert("all-widths: len wrong",  (str.len  == 4));
    r2_assert("all-widths: rune[0] wrong", (str.rune[0] == 0x0041));
    r2_assert("all-widths: rune[1] wrong", (str.rune[1] == 0x00E9));
    r2_assert("all-widths: rune[2] wrong", (str.rune[2] == 0x4E2D));
    r2_assert("all-widths: rune[3] wrong", (str.rune[3] == 0x1F44B));
    r2_assert("all-widths: rune[4] sentinel wrong", (str.rune[4] == 0x0));
    free_S(str);
    return 0;
}

/// Rune array sentinel ///

static const char *test_rune_sentinel(void)
{
    /* The rune array is calloc'd to byte-count elements, so rune[len]
       is always within the allocation and should be zero. */
    s8 str = S("abc");
    r2_assert("sentinel: len wrong", (str.len == 3));
    r2_assert("sentinel: rune[3] not zero", (str.rune[3] == 0x0));
    free_S(str);
    return 0;
}

/// Borrowed data pointer ///

static const char *test_data_pointer(void)
{
    /* S() does not copy the string; data must point back to the source. */
    const char *src = "hello";
    s8 str = S(src);
    r2_assert("data ptr: points elsewhere", (str.data == src));
    free_S(str);
    return 0;
}

/// Single-char edge cases ///

static const char *test_single_ascii_char(void)
{
    s8 str = S("A");
    r2_assert("single ascii: cap wrong", (str.cap == 1));
    r2_assert("single ascii: len wrong",  (str.len  == 1));
    r2_assert("single ascii: rune[0] wrong", (str.rune[0] == 0x41));
    free_S(str);
    return 0;
}

static const char *test_last_ascii_char(void)
{
    /* DEL = U+007F, last 1-byte UTF-8 codepoint */
    s8 str = S("\x7F");
    r2_assert("DEL: cap wrong", (str.cap == 1));
    r2_assert("DEL: len wrong",  (str.len  == 1));
    r2_assert("DEL: rune[0] wrong", (str.rune[0] == 0x7F));
    free_S(str);
    return 0;
}

static const char *test_max_codepoint(void)
{
    /* U+10FFFF is the highest valid Unicode codepoint, 4 bytes: 0xF4 0x8F 0xBF 0xBF */
    s8 str = S("\xF4\x8F\xBF\xBF");
    r2_assert("U+10FFFF: cap wrong", (str.cap == 4));
    r2_assert("U+10FFFF: len wrong",  (str.len  == 1));
    r2_assert("U+10FFFF: rune[0] wrong", (str.rune[0] == 0x10FFFF));
    free_S(str);
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
    r2_assert("utf8 mod string cap is wrong", (str.cap == 18));
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
    r2_assert("utf8 mod2 string cap is wrong", (str.cap == 14));
    free_S(str);
    return 0;
}

////////////////////////////////////////////

static const char *r2_strings_test(void)
{
    // Basic construction
    r2_run_test(test_create_string);
    r2_run_test(test_create_string_blank);
    r2_run_test(test_create_string_empty);
    r2_run_test(test_create_string_null);
    r2_run_test(test_single_ascii_char);
    r2_run_test(test_last_ascii_char);

    // 2-byte UTF-8
    r2_run_test(test_two_byte_single);
    r2_run_test(test_two_byte_chars);

    // 3-byte UTF-8
    r2_run_test(test_three_byte_single);
    r2_run_test(test_three_byte_chars);

    // 4-byte UTF-8
    r2_run_test(test_create_utf8_string);
    r2_run_test(test_max_codepoint);

    // Mixed widths
    r2_run_test(test_mixed_ascii_and_multibyte);
    r2_run_test(test_mixed_all_widths);

    // Struct invariants
    r2_run_test(test_rune_sentinel);
    r2_run_test(test_data_pointer);

    // Joiner / modifier sequences
    r2_run_test(test_create_utf8_string_mod);
    r2_run_test(test_create_utf8_string_mod2);

    // Print smoke test
    r2_run_test(test_print_utf8_string);
    return 0;
}
