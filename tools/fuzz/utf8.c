/* r2_strings.h decoding and encoding on arbitrary bytes */
#define R2_STRINGS_IMPLEMENTATION
#include "../../r2_strings.h"
#include "fuzz.h"

#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static rune runes[4096];
    static uint8_t again[4 * 4096];
    s8 str;
    uint32_t len = size > 4096 ? 4096 : (uint32_t)size, at = 0, count = 0, out = 0, k;
    int valid;
    rune r;
    str = s8_from_bytes(data, len);
    valid = s8_is_valid_utf8(str);

    /* walk it, decoding directly as well, and re-encode every rune */
    while (s8_next_rune(str, &at, &r))
    {
        uint8_t enc[4];
        rune back;
        uint32_t n;
        FUZZ_CHECK("rune in range", r <= R2_RUNE_MAX && !(r >= 0xD800u && r <= 0xDFFFu));
        runes[count++] = r;
        n = utf8_encode(r, enc);
        FUZZ_CHECK("encode is 1..4 bytes", n >= 1 && n <= 4);
        FUZZ_CHECK("encode decodes back", utf8_decode(enc, n, &back) == n && back == r);
        memcpy(again + out, enc, n);
        out += n;
    }
    FUZZ_CHECK("walk ends at len", at == len);
    FUZZ_CHECK("rune count", s8_rune_count(str) == count);

    /* every offset: decode consumes 1..4 bytes, never past the end */
    for (k = 0; k < len; k++)
    {
        uint32_t used = utf8_decode(data + k, len - k, &r);
        FUZZ_CHECK("decode consumes 1..4 bytes within len", used >= 1 && used <= 4 && used <= len - k);
    }

    /* well formed input re-encodes to itself; anything re-encodes to well
       formed UTF-8 with the same runes */
    if (valid)
        FUZZ_CHECK("valid text round trips", out == len && memcmp(again, data, len) == 0);
    FUZZ_CHECK("re-encoded text is valid", s8_is_valid_utf8(s8_from_bytes(again, out)));
    FUZZ_CHECK("re-encoded rune count", s8_rune_count(s8_from_bytes(again, out)) == count);
    /* invalid input always holds a replacement rune somewhere */
    if (!valid)
    {
        int has = 0;
        for (k = 0; k < count; k++)
            has |= runes[k] == R2_RUNE_REPLACEMENT;
        FUZZ_CHECK("invalid text decodes a replacement", has);
    }

    /* s8_to_runes with a cap that is too small, exact, and too big */
    if (count)
    {
        static rune dest[4096];
        uint32_t caps[3] = {count / 2, count, count + 1};
        for (k = 0; k < 3; k++)
        {
            uint32_t got = s8_to_runes(str, dest, caps[k]);
            FUZZ_CHECK("to_runes count", got == (caps[k] < count ? caps[k] : count));
            FUZZ_CHECK("to_runes runes", memcmp(dest, runes, got * sizeof(rune)) == 0);
        }
    }

    /* slices clamp, and a slice of valid text cut at rune starts is valid */
    if (len)
    {
        uint32_t start = data[0] % (len + 1), n = (len > 1 ? data[1] : 0) % (len + 2);
        s8 sl = s8_slice(str, start + len, n);
        FUZZ_CHECK("slice past the end is empty", sl.len == 0);
        sl = s8_slice(str, start, n + len);
        FUZZ_CHECK("slice clamps", sl.len == len - start && sl.data == data + start);
        if (valid && (start == len || (data[start] & 0xC0u) != 0x80u))
            FUZZ_CHECK("slice from a rune start is valid", s8_is_valid_utf8(sl));
    }
    return 0;
}
