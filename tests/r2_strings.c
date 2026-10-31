#define R2_STRINGS_IMPLEMENTATION
#include "../r2_strings.h"
#include "../r2_unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>

#if (__WORDSIZE == 64)
#define BUILD_64 1
#endif

/* Decode the whole string into runes; returns the rune count. */
static uint32_t decode_all(s8 str, rune *dest, uint32_t cap)
{
    return s8_to_runes(str, dest, cap);
}

static const char *test_create_string(void)
{
    s8 str = S8("This is a test");
    r2_assert("string len is wrong", (str.len == 14));
    r2_assert("string rune count is wrong", (s8_rune_count(str) == 14));
    return 0;
}

static const char *test_create_string_blank(void)
{
    s8 str = S8("");
    r2_assert("string len is wrong", (str.len == 0));
    r2_assert("string rune count is wrong", (s8_rune_count(str) == 0));
    return 0;
}

static const char *test_create_string_empty(void)
{
    const char *str1 = "";
    s8 str = s8_from_cstr(str1);

    r2_assert("empty string len is wrong", (str.len == 0));
    r2_assert("empty string rune count is wrong", (s8_rune_count(str) == 0));
    return 0;
}

static const char *test_create_string_null(void)
{
    s8 a = s8_from_cstr(NULL);
    s8 b = s8_from_bytes(NULL, 10);

    r2_assert("null cstr len is wrong", (a.len == 0));
    r2_assert("null cstr rune count is wrong", (s8_rune_count(a) == 0));
    r2_assert("null bytes len should clamp to 0", (b.len == 0));
    return 0;
}

static const char *test_from_cstr_matches_literal(void)
{
    s8 a = S8("Aé中👋");
    s8 b = s8_from_cstr("Aé中👋");
    r2_assert("from_cstr len differs from S8", (a.len == b.len));
    r2_assert("from_cstr bytes differ from S8", s8_equal(a, b));
    return 0;
}

static const char *test_create_utf8_string(void)
{
    rune r[8];
    s8 str = S8("👋👏他是我的😺");
    r2_assert("utf8 string len is wrong", (str.len == 24));
    r2_assert("utf8 string rune count is wrong", (s8_rune_count(str) == 7));
    r2_assert("utf8 decode count is wrong", (decode_all(str, r, 8) == 7));
    r2_assert("utf8 rune 0 is wrong", (r[0] == 0x1F44B));
    r2_assert("utf8 rune 1 is wrong", (r[1] == 0x1F44F));
    r2_assert("utf8 rune 6 is wrong", (r[6] == 0x1F63A));
    return 0;
}

static const char *test_print_utf8_string(void)
{
    s8 str = S8("👋😺👏🏳️‍🌈👨‍👩‍👦");
    uint32_t at = 0;
    rune r;
    setlocale(LC_ALL, "");
    while (s8_next_rune(str, &at, &r))
    {
        printf("%lc", (wint_t)r);
    }
    r2_assert("", (0x1 == 0x1));
    return 0;
}

/// 2-byte UTF-8 (U+0080..U+07FF) ///

static const char *test_two_byte_chars(void)
{
    /* é = U+00E9, encoded as 0xC3 0xA9 (2 bytes)
       ñ = U+00F1, encoded as 0xC3 0xB1 (2 bytes) */
    rune r[4];
    s8 str = S8("éñ");
    r2_assert("2-byte: len wrong", (str.len == 4));
    r2_assert("2-byte: rune count wrong", (decode_all(str, r, 4) == 2));
    r2_assert("2-byte: rune[0] wrong", (r[0] == 0x00E9));
    r2_assert("2-byte: rune[1] wrong", (r[1] == 0x00F1));
    return 0;
}

static const char *test_two_byte_single(void)
{
    /* Single 2-byte char: ü = U+00FC, 0xC3 0xBC */
    rune r[2];
    s8 str = S8("ü");
    r2_assert("2-byte single: len wrong", (str.len == 2));
    r2_assert("2-byte single: rune count wrong", (decode_all(str, r, 2) == 1));
    r2_assert("2-byte single: rune[0] wrong", (r[0] == 0x00FC));
    return 0;
}

/// 3-byte UTF-8 (U+0800..U+FFFF) ///

static const char *test_three_byte_chars(void)
{
    /* 中 = U+4E2D, 0xE4 0xB8 0xAD (3 bytes)
       文 = U+6587, 0xE6 0x96 0x87 (3 bytes) */
    rune r[4];
    s8 str = S8("中文");
    r2_assert("3-byte: len wrong", (str.len == 6));
    r2_assert("3-byte: rune count wrong", (decode_all(str, r, 4) == 2));
    r2_assert("3-byte: rune[0] wrong", (r[0] == 0x4E2D));
    r2_assert("3-byte: rune[1] wrong", (r[1] == 0x6587));
    return 0;
}

static const char *test_three_byte_single(void)
{
    /* ∑ = U+2211, 0xE2 0x88 0x91 */
    rune r[2];
    s8 str = S8("∑");
    r2_assert("3-byte single: len wrong", (str.len == 3));
    r2_assert("3-byte single: rune count wrong", (decode_all(str, r, 2) == 1));
    r2_assert("3-byte single: rune[0] wrong", (r[0] == 0x2211));
    return 0;
}

/// Mixed-width strings ///

static const char *test_mixed_ascii_and_multibyte(void)
{
    /* "Hi中" = H(1) + i(1) + 中(3) = 5 bytes, 3 runes */
    rune r[4];
    s8 str = S8("Hi中");
    r2_assert("mixed: len wrong", (str.len == 5));
    r2_assert("mixed: rune count wrong", (decode_all(str, r, 4) == 3));
    r2_assert("mixed: rune[0] wrong", (r[0] == 'H'));
    r2_assert("mixed: rune[1] wrong", (r[1] == 'i'));
    r2_assert("mixed: rune[2] wrong", (r[2] == 0x4E2D));
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
    rune r[5];
    s8 str = S8("Aé中👋");
    r2_assert("all-widths: len wrong", (str.len == 10));
    r2_assert("all-widths: rune count wrong", (decode_all(str, r, 5) == 4));
    r2_assert("all-widths: rune[0] wrong", (r[0] == 0x0041));
    r2_assert("all-widths: rune[1] wrong", (r[1] == 0x00E9));
    r2_assert("all-widths: rune[2] wrong", (r[2] == 0x4E2D));
    r2_assert("all-widths: rune[3] wrong", (r[3] == 0x1F44B));
    return 0;
}

/// Iteration ///

static const char *test_next_rune_offsets(void)
{
    /* Offsets advance by the byte width of each rune */
    s8 str = S8("Aé中👋");
    uint32_t at = 0;
    rune r;
    r2_assert("next: 1st missing", s8_next_rune(str, &at, &r));
    r2_assert("next: 1st wrong", (r == 0x41 && at == 1));
    r2_assert("next: 2nd missing", s8_next_rune(str, &at, &r));
    r2_assert("next: 2nd wrong", (r == 0xE9 && at == 3));
    r2_assert("next: 3rd missing", s8_next_rune(str, &at, &r));
    r2_assert("next: 3rd wrong", (r == 0x4E2D && at == 6));
    r2_assert("next: 4th missing", s8_next_rune(str, &at, &r));
    r2_assert("next: 4th wrong", (r == 0x1F44B && at == 10));
    r2_assert("next: should be at end", !s8_next_rune(str, &at, &r));
    r2_assert("next: offset moved at end", (at == 10));
    return 0;
}

static const char *test_to_runes_truncates(void)
{
    rune r[2] = {0, 0};
    s8 str = S8("abc");
    r2_assert("to_runes: should stop at cap", (s8_to_runes(str, r, 2) == 2));
    r2_assert("to_runes: rune[0] wrong", (r[0] == 'a'));
    r2_assert("to_runes: rune[1] wrong", (r[1] == 'b'));
    r2_assert("to_runes: zero cap", (s8_to_runes(str, r, 0) == 0));
    return 0;
}

/// Borrowed data pointer ///

static const char *test_data_pointer(void)
{
    /* s8 does not copy the string; data must point back to the source. */
    const char *src = "hello";
    s8 str = s8_from_cstr(src);
    r2_assert("data ptr: points elsewhere", ((const char *)str.data == src));
    return 0;
}

static const char *test_embedded_nul(void)
{
    /* Binary payloads may contain 0x00; it is a normal 1-byte rune */
    const uint8_t pkt[] = {'a', 0x00, 'b'};
    rune r[3];
    s8 str = s8_from_bytes(pkt, 3);
    r2_assert("nul: len wrong", (str.len == 3));
    r2_assert("nul: rune count wrong", (decode_all(str, r, 3) == 3));
    r2_assert("nul: rune[1] wrong", (r[1] == 0x00));
    r2_assert("nul: should be valid", s8_is_valid_utf8(str));
    return 0;
}

/// Single-char edge cases ///

static const char *test_single_ascii_char(void)
{
    rune r[1];
    s8 str = S8("A");
    r2_assert("single ascii: len wrong", (str.len == 1));
    r2_assert("single ascii: rune count wrong", (decode_all(str, r, 1) == 1));
    r2_assert("single ascii: rune[0] wrong", (r[0] == 0x41));
    return 0;
}

static const char *test_last_ascii_char(void)
{
    /* DEL = U+007F, last 1-byte UTF-8 codepoint */
    rune r[1];
    s8 str = S8("\x7F");
    r2_assert("DEL: len wrong", (str.len == 1));
    r2_assert("DEL: rune count wrong", (decode_all(str, r, 1) == 1));
    r2_assert("DEL: rune[0] wrong", (r[0] == 0x7F));
    return 0;
}

static const char *test_max_codepoint(void)
{
    /* U+10FFFF is the highest valid Unicode codepoint, 4 bytes: 0xF4 0x8F 0xBF 0xBF */
    rune r[1];
    s8 str = S8("\xF4\x8F\xBF\xBF");
    r2_assert("U+10FFFF: len wrong", (str.len == 4));
    r2_assert("U+10FFFF: rune count wrong", (decode_all(str, r, 1) == 1));
    r2_assert("U+10FFFF: rune[0] wrong", (r[0] == 0x10FFFF));
    r2_assert("U+10FFFF: should be valid", s8_is_valid_utf8(str));
    return 0;
}

/// Invalid UTF-8 (U+FFFD, maximal subparts) ///

static const char *test_stray_continuation(void)
{
    /* v0.0 hung on this. Each stray byte is its own U+FFFD */
    rune r[4];
    s8 str = S8("a\x80\xBF" "b");
    r2_assert("stray: rune count wrong", (decode_all(str, r, 4) == 4));
    r2_assert("stray: rune[1] wrong", (r[1] == R2_RUNE_REPLACEMENT));
    r2_assert("stray: rune[2] wrong", (r[2] == R2_RUNE_REPLACEMENT));
    r2_assert("stray: rune[3] wrong", (r[3] == 'b'));
    r2_assert("stray: should be invalid", !s8_is_valid_utf8(str));
    return 0;
}

static const char *test_invalid_lead_bytes(void)
{
    /* C0/C1 (overlong) and F5..FF are never valid; v0.0 read past
       its table on F8..FF */
    const uint8_t bytes[] = {0xC0, 0xC1, 0xF5, 0xF8, 0xFE, 0xFF};
    rune r[6];
    s8 str = s8_from_bytes(bytes, sizeof(bytes));
    r2_assert("bad lead: rune count wrong", (decode_all(str, r, 6) == 6));
    for (int i = 0; i < 6; i++)
    {
        r2_assert("bad lead: not replacement", (r[i] == R2_RUNE_REPLACEMENT));
    }
    r2_assert("bad lead: should be invalid", !s8_is_valid_utf8(str));
    return 0;
}

static const char *test_truncated_sequence(void)
{
    /* 中 is E4 B8 AD; cut off after two bytes. Must not read past len,
       and the partial sequence is one maximal subpart -> one U+FFFD */
    const uint8_t bytes[] = {'x', 0xE4, 0xB8};
    rune r[3];
    s8 str = s8_from_bytes(bytes, sizeof(bytes));
    r2_assert("truncated: rune count wrong", (decode_all(str, r, 3) == 2));
    r2_assert("truncated: rune[0] wrong", (r[0] == 'x'));
    r2_assert("truncated: rune[1] wrong", (r[1] == R2_RUNE_REPLACEMENT));
    r2_assert("truncated: should be invalid", !s8_is_valid_utf8(str));
    return 0;
}

static const char *test_interrupted_sequence(void)
{
    /* F0 9F 91 'A': lead + 2 good bytes then ASCII. The 3 bytes are one
       U+FFFD and decoding resumes at 'A' */
    rune r[3];
    rune out;
    s8 str = S8("\xF0\x9F\x91" "A");
    r2_assert("interrupted: consumed wrong",
              (utf8_decode(str.data, str.len, &out) == 3));
    r2_assert("interrupted: rune count wrong", (decode_all(str, r, 3) == 2));
    r2_assert("interrupted: rune[0] wrong", (r[0] == R2_RUNE_REPLACEMENT));
    r2_assert("interrupted: rune[1] wrong", (r[1] == 'A'));
    return 0;
}

static const char *test_overlong_and_surrogate(void)
{
    /* E0 80 80 is an overlong NUL; ED A0 80 is surrogate U+D800;
       F4 90 80 80 is past U+10FFFF. The bad 2nd byte ends each maximal
       subpart at 1 byte, so the trailing bytes are stray continuations. */
    rune out;
    r2_assert("overlong E0: wrong",
              (utf8_decode((const uint8_t *)"\xE0\x80\x80", 3, &out) == 1 &&
               out == R2_RUNE_REPLACEMENT));
    r2_assert("surrogate ED: wrong",
              (utf8_decode((const uint8_t *)"\xED\xA0\x80", 3, &out) == 1 &&
               out == R2_RUNE_REPLACEMENT));
    r2_assert("past max F4: wrong",
              (utf8_decode((const uint8_t *)"\xF4\x90\x80\x80", 4, &out) == 1 &&
               out == R2_RUNE_REPLACEMENT));
    r2_assert("surrogate: rune count wrong",
              (s8_rune_count(S8("\xED\xA0\x80")) == 3));
    return 0;
}

static const char *test_decode_empty(void)
{
    rune out = 0;
    r2_assert("decode empty: should consume 0",
              (utf8_decode((const uint8_t *)"", 0, &out) == 0));
    return 0;
}

static const char *test_real_replacement_is_valid(void)
{
    /* A literal U+FFFD (EF BF BD) in the text is well-formed */
    s8 str = S8("a\xEF\xBF\xBD" "b");
    r2_assert("real FFFD: should be valid", s8_is_valid_utf8(str));
    r2_assert("real FFFD: rune count wrong", (s8_rune_count(str) == 3));
    return 0;
}

/// Encoding ///

static const char *test_encode_round_trip(void)
{
    const rune cps[] = {0x41, 0x7F, 0x80, 0xE9, 0x7FF, 0x800, 0x4E2D,
                        0xFFFF, 0x10000, 0x1F44B, 0x10FFFF};
    const uint32_t widths[] = {1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4};
    for (int i = 0; i < (int)(sizeof(cps) / sizeof(cps[0])); i++)
    {
        uint8_t buf[4];
        rune back;
        uint32_t n = utf8_encode(cps[i], buf);
        r2_assert("encode: width wrong", (n == widths[i]));
        r2_assert("encode: decode width wrong", (utf8_decode(buf, n, &back) == n));
        r2_assert("encode: round trip wrong", (back == cps[i]));
    }
    return 0;
}

static const char *test_encode_matches_literal(void)
{
    uint8_t buf[4];
    uint32_t n = utf8_encode(0x1F44B, buf);
    r2_assert("encode 👋: bytes wrong",
              s8_equal(s8_from_bytes(buf, n), S8("👋")));
    return 0;
}

static const char *test_encode_invalid(void)
{
    /* Surrogates and > U+10FFFF encode as U+FFFD (EF BF BD) */
    uint8_t buf[4];
    s8 fffd = S8("\xEF\xBF\xBD");
    uint32_t n = utf8_encode(0xD800, buf);
    r2_assert("encode surrogate: wrong", s8_equal(s8_from_bytes(buf, n), fffd));
    n = utf8_encode(0x110000, buf);
    r2_assert("encode past max: wrong", s8_equal(s8_from_bytes(buf, n), fffd));
    return 0;
}

/// Equality and slicing ///

static const char *test_equal(void)
{
    r2_assert("equal: same", s8_equal(S8("abc"), S8("abc")));
    r2_assert("equal: diff byte", !s8_equal(S8("abc"), S8("abd")));
    r2_assert("equal: diff len", !s8_equal(S8("abc"), S8("ab")));
    r2_assert("equal: empty", s8_equal(S8(""), s8_from_cstr(NULL)));
    return 0;
}

static const char *test_slice(void)
{
    s8 str = S8("Hello, World");
    r2_assert("slice: middle", s8_equal(s8_slice(str, 7, 5), S8("World")));
    r2_assert("slice: len clamped", s8_equal(s8_slice(str, 7, 100), S8("World")));
    r2_assert("slice: start clamped", (s8_slice(str, 100, 5).len == 0));
    r2_assert("slice: zero len", (s8_slice(str, 3, 0).len == 0));
    return 0;
}

static const char *test_slice_splits_rune(void)
{
    /* Slicing by bytes through 中 (3 bytes) leaves a broken sequence */
    rune r[2];
    s8 str = s8_slice(S8("中"), 0, 2);
    r2_assert("split: rune count wrong", (decode_all(str, r, 2) == 1));
    r2_assert("split: not replacement", (r[0] == R2_RUNE_REPLACEMENT));
    return 0;
}

static const char *test_four_byte_middle_leads(void)
{
    /* F1..F3 leads (U+40000..U+FFFFF) take the full 80..BF second byte */
    rune r[2];
    s8 str = S8("\xF1\x80\x80\x80\xF3\xBF\xBF\xBF");
    r2_assert("F1-F3: rune count wrong", (decode_all(str, r, 2) == 2));
    r2_assert("F1-F3: rune[0] wrong", (r[0] == 0x40000));
    r2_assert("F1-F3: rune[1] wrong", (r[1] == 0xFFFFF));
    return 0;
}

static const char *test_lead_byte_at_end(void)
{
    /* A lead byte with nothing after it is one U+FFFD of one byte */
    rune out;
    r2_assert("lone C3: wrong",
              (utf8_decode((const uint8_t *)"\xC3", 1, &out) == 1 && out == R2_RUNE_REPLACEMENT));
    r2_assert("lone F0: wrong",
              (utf8_decode((const uint8_t *)"\xF0", 1, &out) == 1 && out == R2_RUNE_REPLACEMENT));
    return 0;
}

/// Exhaustive / differential ///

/* An independent reference decoder built only from the definition of
   UTF-8: a scalar value is 0..10FFFF minus the surrogates, each has
   exactly one (shortest) encoding, and nothing else is well-formed.
   Invalid input gives one U+FFFD per maximal subpart: the longest prefix
   of some well-formed sequence, or one byte. utf8_decode is checked
   against it on every input of 1..3 bytes and on every 4-byte input that
   starts with a valid 3-byte prefix. */

static int ref_is_scalar(uint32_t cp)
{
    return cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

static uint32_t ref_encode(uint32_t cp, uint8_t out[4])
{
    static const uint8_t lead[5] = {0, 0x00, 0xC0, 0xE0, 0xF0};
    uint32_t n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
    for (uint32_t i = n - 1; i > 0; i--)
    {
        out[i] = (uint8_t)(0x80 | (cp & 0x3F));
        cp >>= 6;
    }
    out[0] = (uint8_t)(lead[n] | cp);
    return n;
}

/* Bitmaps of every proper prefix (length 1..3) of a well-formed sequence */
static uint8_t ref_prefix1[(1u << 8) / 8];
static uint8_t ref_prefix2[(1u << 16) / 8];
static uint8_t ref_prefix3[(1u << 24) / 8];
static uint8_t *const ref_prefix[4] = {0, ref_prefix1, ref_prefix2, ref_prefix3};

static uint32_t ref_key(const uint8_t *b, uint32_t k)
{
    uint32_t key = 0;
    for (uint32_t i = 0; i < k; i++)
        key = (key << 8) | b[i];
    return key;
}

static void ref_build(void)
{
    static int built = 0;
    if (built)
        return;
    for (uint32_t cp = 0; cp <= 0x10FFFF; cp++)
    {
        uint8_t b[4];
        uint32_t n;
        if (!ref_is_scalar(cp))
            continue;
        n = ref_encode(cp, b);
        for (uint32_t k = 1; k < n; k++)
        {
            uint32_t key = ref_key(b, k);
            ref_prefix[k][key >> 3] |= (uint8_t)(1u << (key & 7));
        }
    }
    built = 1;
}

static int ref_has_prefix(const uint8_t *b, uint32_t k)
{
    uint32_t key = ref_key(b, k);
    return (ref_prefix[k][key >> 3] >> (key & 7)) & 1;
}

/* b[0..k) is a whole well-formed sequence: decode the payload bits
   naively, then require that re-encoding gives back the same bytes */
static int ref_is_full(const uint8_t *b, uint32_t k, rune *out)
{
    uint8_t enc[4];
    uint32_t cp = (k == 1) ? b[0] : (uint32_t)(b[0] & (0x7F >> k));
    for (uint32_t i = 1; i < k; i++)
        cp = (cp << 6) | (b[i] & 0x3F);
    if (!ref_is_scalar(cp) || ref_encode(cp, enc) != k)
        return 0;
    for (uint32_t i = 0; i < k; i++)
        if (enc[i] != b[i])
            return 0;
    *out = cp;
    return 1;
}

/* Returns bytes consumed; *valid is 0 when this was a replacement */
static uint32_t ref_decode(const uint8_t *b, uint32_t len, rune *out, int *valid)
{
    uint32_t m = 1;
    *valid = 0;
    *out = R2_RUNE_REPLACEMENT;
    if (len == 0)
        return 0;
    for (uint32_t k = 1; k <= len && k <= 4; k++)
    {
        if (ref_is_full(b, k, out))
        {
            *valid = 1;
            return k;
        }
    }
    for (uint32_t k = 1; k <= len && k <= 3; k++)
        if (ref_has_prefix(b, k))
            m = k;
    return m;
}

/* Bytes after len are filled with continuation bytes that would extend
   a sequence, so a decoder that reads past len gives a different answer.
   Two fillers because E0 wants A0..BF and ED/F4 want 80..9F/80..8F. */
static const uint8_t ref_fillers[2] = {0x80, 0xA0};

static int decode_matches_ref(uint8_t buf[8], uint32_t len)
{
    for (int f = 0; f < 2; f++)
    {
        rune got, want;
        int valid;
        uint32_t used, ref_used;
        for (uint32_t i = len; i < 8; i++)
            buf[i] = ref_fillers[f];
        used = utf8_decode(buf, len, &got);
        ref_used = ref_decode(buf, len, &want, &valid);
        if (used != ref_used || got != want)
            return 0;
    }
    return 1;
}

static const char *test_decode_exhaustive_short(void)
{
    /* every input of 1, 2 and 3 bytes: 16.8M cases */
    uint8_t buf[8];
    ref_build();
    for (uint32_t len = 1; len <= 3; len++)
    {
        uint32_t total = 1u << (8 * len);
        for (uint32_t key = 0; key < total; key++)
        {
            for (uint32_t i = 0; i < len; i++)
                buf[i] = (uint8_t)(key >> (8 * (len - 1 - i)));
            r2_assert("exhaustive: decode disagrees with reference", decode_matches_ref(buf, len));
        }
    }
    return 0;
}

static const char *test_decode_exhaustive_four(void)
{
    /* every 4-byte input whose first 3 bytes are a valid prefix (all the
       interesting 4-byte cases), plus random 4-byte inputs */
    uint8_t buf[8];
    uint32_t seed = 0x2545F491u;
    ref_build();
    for (uint32_t key = 0; key < (1u << 24); key++)
    {
        buf[0] = (uint8_t)(key >> 16);
        buf[1] = (uint8_t)(key >> 8);
        buf[2] = (uint8_t)key;
        if (!ref_has_prefix(buf, 3))
            continue;
        for (uint32_t last = 0; last < 256; last++)
        {
            buf[3] = (uint8_t)last;
            r2_assert("exhaustive 4: decode disagrees with reference", decode_matches_ref(buf, 4));
        }
    }
    for (int i = 0; i < 1000000; i++)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        buf[0] = (uint8_t)seed;
        buf[1] = (uint8_t)(seed >> 8);
        buf[2] = (uint8_t)(seed >> 16);
        buf[3] = (uint8_t)(seed >> 24);
        r2_assert("random 4: decode disagrees with reference", decode_matches_ref(buf, 4));
    }
    return 0;
}

static const char *test_encode_exhaustive(void)
{
    /* every codepoint, plus a few past the end */
    const rune extra[] = {0x110000, 0x7FFFFFFF, 0xFFFFFFFF};
    for (uint32_t cp = 0; cp <= 0x10FFFF + 3; cp++)
    {
        rune r = (cp <= 0x10FFFF) ? cp : extra[cp - 0x110000];
        uint8_t got[4], want[4];
        rune back;
        uint32_t n = utf8_encode(r, got);
        uint32_t wn = ref_encode(ref_is_scalar(r) ? r : R2_RUNE_REPLACEMENT, want);
        r2_assert("encode: width differs from reference", (n == wn));
        for (uint32_t i = 0; i < n; i++)
            r2_assert("encode: byte differs from reference", (got[i] == want[i]));
        r2_assert("encode: decode width wrong", (utf8_decode(got, n, &back) == n));
        r2_assert("encode: round trip wrong", (back == (ref_is_scalar(r) ? r : R2_RUNE_REPLACEMENT)));
    }
    return 0;
}

static const char *test_string_functions_random(void)
{
    /* Random strings drawn mostly from "interesting" bytes. Checks the
       whole-string functions against the reference decoder. */
    static const uint8_t pool[] = {'a', 0x00, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0,
                                   0xC2, 0xDF, 0xE0, 0xE1, 0xED, 0xEF, 0xF0, 0xF1, 0xF4, 0xF5, 0xFF};
    uint32_t seed = 0x9E3779B9u;
    uint8_t buf[40];
    rune runes[40];
    ref_build();
    for (int iter = 0; iter < 200000; iter++)
    {
        uint32_t len, at = 0, ref_at = 0, count = 0;
        int all_valid = 1;
        rune got;
        s8 str;
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        len = seed % 33;
        for (uint32_t i = 0; i < len; i++)
        {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            buf[i] = (seed & 0x100) ? (uint8_t)seed : pool[seed % sizeof(pool)];
        }
        str = s8_from_bytes(buf, len);

        while (s8_next_rune(str, &at, &got))
        {
            rune want;
            int valid;
            ref_at += ref_decode(buf + ref_at, len - ref_at, &want, &valid);
            all_valid &= valid;
            r2_assert("random: rune differs from reference", (got == want));
            r2_assert("random: offset differs from reference", (at == ref_at));
            r2_assert("random: walked past len", (at <= len));
            count++;
        }
        r2_assert("random: did not stop at len", (at == len));
        r2_assert("random: rune count wrong", (s8_rune_count(str) == count));
        r2_assert("random: to_runes count wrong", (s8_to_runes(str, runes, 40) == count));
        r2_assert("random: is_valid differs from reference", (s8_is_valid_utf8(str) == all_valid));

        if (len > 0)
        {
            uint32_t start = seed % (len + 2);
            uint32_t slen = (seed >> 8) % (len + 2);
            s8 sl = s8_slice(str, start, slen);
            uint32_t want_start = start > len ? len : start;
            uint32_t want_len = slen > len - want_start ? len - want_start : slen;
            r2_assert("random: slice start wrong", (sl.data == buf + want_start));
            r2_assert("random: slice len wrong", (sl.len == want_len));
        }
    }
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
    rune r[5];
    s8 str = S8("👨‍👩‍👦");
    r2_assert("utf8 mod string len is wrong", (str.len == 18));
    r2_assert("utf8 mod string rune count is wrong", (decode_all(str, r, 5) == 5));
    r2_assert("utf8 mod string ZWJ is wrong", (r[1] == 0x200D && r[3] == 0x200D));
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
    s8 str = S8("🏳️‍🌈");
    r2_assert("utf8 mod2 string len is wrong", (str.len == 14));
    r2_assert("utf8 mod2 string rune count is wrong", (s8_rune_count(str) == 4));
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
    r2_run_test(test_from_cstr_matches_literal);
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

    // Iteration
    r2_run_test(test_next_rune_offsets);
    r2_run_test(test_to_runes_truncates);

    // Struct invariants
    r2_run_test(test_data_pointer);
    r2_run_test(test_embedded_nul);

    // Invalid UTF-8
    r2_run_test(test_stray_continuation);
    r2_run_test(test_invalid_lead_bytes);
    r2_run_test(test_truncated_sequence);
    r2_run_test(test_interrupted_sequence);
    r2_run_test(test_overlong_and_surrogate);
    r2_run_test(test_decode_empty);
    r2_run_test(test_real_replacement_is_valid);
    r2_run_test(test_four_byte_middle_leads);
    r2_run_test(test_lead_byte_at_end);

    // Exhaustive / differential against a reference decoder
    r2_run_test(test_decode_exhaustive_short);
    r2_run_test(test_decode_exhaustive_four);
    r2_run_test(test_encode_exhaustive);
    r2_run_test(test_string_functions_random);

    // Encoding
    r2_run_test(test_encode_round_trip);
    r2_run_test(test_encode_matches_literal);
    r2_run_test(test_encode_invalid);

    // Equality and slicing
    r2_run_test(test_equal);
    r2_run_test(test_slice);
    r2_run_test(test_slice_splits_rune);

    // Joiner / modifier sequences
    r2_run_test(test_create_utf8_string_mod);
    r2_run_test(test_create_utf8_string_mod2);

    // Print smoke test
    r2_run_test(test_print_utf8_string);
    return 0;
}
