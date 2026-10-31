/* r2_strings - v0.1 - public domain very basic, utf8 string library
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    This is written with game development and small embedded targets in
    mind: no allocation, no libc beyond <stdint.h>, never reads past the
    length it is given.

    Do this:
       #define R2_STRINGS_IMPLEMENTATION
    before you include this file in *one* C or C++ file
    to create the implementation.

    // i.e. it should look like this:
    #include ...
    #include ...
    #include ...
    #define R2_STRINGS_IMPLEMENTATION
    #include "r2_strings.h"

    You can then include without the define to just use the types

OVERVIEW
    The main type is s8: a run of bytes and a length.

        data      - pointer to the bytes (see OWNERSHIP below)
        len       - length in *bytes*

    The bytes are not NUL-terminated and may contain 0x00, so an s8 can hold
    any binary payload (a radio packet, a file block) as well as text. Text
    is UTF-8; runes (codepoints) are decoded on demand while iterating, so
    nothing is allocated:

        s8 str = S8("café");
        str.len == 5                 // c(1) + a(1) + f(1) + é(2)
        s8_rune_count(str) == 4      // four codepoints

        uint32_t at = 0;
        rune r;
        while (s8_next_rune(str, &at, &r))
        {
            // r = 'c', 'a', 'f', 0xE9
        }

    Invalid UTF-8 (a stray continuation byte, a sequence cut off by the end
    of the string, an overlong form, a surrogate, anything past U+10FFFF)
    decodes as U+FFFD, one replacement per "maximal subpart" as the Unicode
    standard recommends (ch. 3.9, "U+FFFD Substitution of Maximal
    Subparts"). Decoding always consumes at least one byte, so loops over
    corrupt input still finish.

    Need random access to codepoints? s8_to_runes() decodes into a buffer
    you provide.

OWNERSHIP
    s8 is a *borrowing* view: it stores a pointer to the bytes without
    copying them. The caller keeps that memory alive for as long as the s8
    is in use. There is nothing to free.

        s8 a = S8("Hello");              // literal: lives forever
        s8 b = s8_from_cstr(buf);        // valid while buf is
        s8 c = s8_from_bytes(pkt, n);    // valid while pkt is

CHANGES
    v0.1  s8 is now { data, len (bytes) }; the heap-allocated rune array,
          S() and free_S() are gone (decode on demand instead). No libc
          dependency. Decoder rewritten against the Unicode well-formed
          byte table (Table 3-7): v0.0 hung on a stray continuation byte,
          read past its table on bytes 0xF8-0xFF, and read past the input
          on a sequence truncated by the end of the string.
    v0.0  first version, influenced by
          https://rosettacode.org/wiki/UTF-8_encode_and_decode#C

LICENSE
    See end of file for license information.
*/
#ifndef R2_STRINGS
#define R2_STRINGS

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef uint32_t rune;

#define R2_RUNE_REPLACEMENT 0xFFFDu
#define R2_RUNE_MAX 0x10FFFFu

    /**
     * A run of bytes (usually UTF-8 text) and its length in bytes.
     * Not NUL-terminated; may contain 0x00.
     */
    typedef struct s8
    {
        const uint8_t *data;
        uint32_t len;
    } s8;

/**
 * s8 for a string literal, without a strlen at run time.
 * Only for literals / char arrays: sizeof on a pointer is wrong.
 */
#define S8(literal) s8_from_bytes((literal), (uint32_t)(sizeof(literal) - 1))

    /** View of len bytes at data. */
    static inline s8 s8_from_bytes(const void *data, uint32_t len);

    /** View of a NUL-terminated string (the terminator is not included). */
    static inline s8 s8_from_cstr(const char *str);

    /**
     * Decode one rune from data[0 .. len). Writes the codepoint, or
     * R2_RUNE_REPLACEMENT for invalid input, to *out and returns the number
     * of bytes consumed: 1..4, and never more than len. Returns 0 only when
     * len is 0.
     */
    static inline uint32_t utf8_decode(const uint8_t *data, uint32_t len, rune *out);

    /**
     * Encode r as UTF-8 into out, returning the number of bytes written
     * (1..4). Surrogates and values past U+10FFFF encode U+FFFD.
     */
    static inline uint32_t utf8_encode(rune r, uint8_t out[4]);

    /**
     * Iterate: decode the rune at *offset, advance *offset past it.
     * Returns 1 while there was a rune, 0 at the end of the string.
     */
    static inline int s8_next_rune(s8 str, uint32_t *offset, rune *out);

    /** Number of runes (invalid sequences count as one U+FFFD each). */
    static inline uint32_t s8_rune_count(s8 str);

    /** 1 if every byte is part of well-formed UTF-8, else 0. */
    static inline int s8_is_valid_utf8(s8 str);

    /** 1 if both hold the same bytes. */
    static inline int s8_equal(s8 a, s8 b);

    /**
     * Sub-view of len bytes from byte start, clamped to the string.
     * Byte offsets: a slice can split a multi-byte rune, which then
     * decodes as U+FFFD.
     */
    static inline s8 s8_slice(s8 str, uint32_t start, uint32_t len);

    /**
     * Decode into dest (at most dest_cap runes). Returns the number
     * written; compare with s8_rune_count() to detect truncation.
     */
    static inline uint32_t s8_to_runes(s8 str, rune *dest, uint32_t dest_cap);

/* implementation */
#ifdef R2_STRINGS_IMPLEMENTATION

    static inline s8 s8_from_bytes(const void *data, uint32_t len)
    {
        s8 str;
        str.data = (const uint8_t *)data;
        str.len = data ? len : 0;
        return str;
    }

    static inline s8 s8_from_cstr(const char *str)
    {
        uint32_t len = 0;
        if (str)
        {
            while (str[len] != '\0')
            {
                len++;
            }
        }
        return s8_from_bytes(str, len);
    }

    static inline int r2_is_continuation(uint8_t byte)
    {
        return (byte & 0xC0u) == 0x80u;
    }

    static inline uint32_t utf8_decode(const uint8_t *data, uint32_t len, rune *out)
    {
        uint8_t lead;
        uint32_t need;  /* total bytes in the sequence */
        uint8_t low;    /* allowed range for the second byte */
        uint8_t high;
        rune value;
        uint32_t idx;

        if (len == 0)
        {
            *out = R2_RUNE_REPLACEMENT;
            return 0;
        }
        lead = data[0];

        /* Unicode Table 3-7, well-formed UTF-8 byte sequences */
        if (lead < 0x80u)
        {
            *out = lead;
            return 1;
        }
        else if (lead >= 0xC2u && lead <= 0xDFu)
        {
            need = 2; low = 0x80u; high = 0xBFu;
            value = lead & 0x1Fu;
        }
        else if (lead == 0xE0u)
        {
            need = 3; low = 0xA0u; high = 0xBFu; /* no overlongs */
            value = lead & 0x0Fu;
        }
        else if (lead == 0xEDu)
        {
            need = 3; low = 0x80u; high = 0x9Fu; /* no surrogates */
            value = lead & 0x0Fu;
        }
        else if (lead >= 0xE1u && lead <= 0xEFu)
        {
            need = 3; low = 0x80u; high = 0xBFu;
            value = lead & 0x0Fu;
        }
        else if (lead == 0xF0u)
        {
            need = 4; low = 0x90u; high = 0xBFu; /* no overlongs */
            value = lead & 0x07u;
        }
        else if (lead >= 0xF1u && lead <= 0xF3u)
        {
            need = 4; low = 0x80u; high = 0xBFu;
            value = lead & 0x07u;
        }
        else if (lead == 0xF4u)
        {
            need = 4; low = 0x80u; high = 0x8Fu; /* <= U+10FFFF */
            value = lead & 0x07u;
        }
        else
        {
            /* 80..BF stray continuation, C0/C1 overlong, F5..FF */
            *out = R2_RUNE_REPLACEMENT;
            return 1;
        }

        /* Maximal subpart: on a bad or missing byte, replace everything
           consumed so far and resume at the bad byte. */
        if (len < 2 || data[1] < low || data[1] > high)
        {
            *out = R2_RUNE_REPLACEMENT;
            return 1;
        }
        value = (value << 6) | (data[1] & 0x3Fu);
        for (idx = 2; idx < need; idx++)
        {
            if (idx >= len || !r2_is_continuation(data[idx]))
            {
                *out = R2_RUNE_REPLACEMENT;
                return idx;
            }
            value = (value << 6) | (data[idx] & 0x3Fu);
        }
        *out = value;
        return need;
    }

    static inline uint32_t utf8_encode(rune r, uint8_t out[4])
    {
        if (r > R2_RUNE_MAX || (r >= 0xD800u && r <= 0xDFFFu))
        {
            r = R2_RUNE_REPLACEMENT;
        }
        if (r < 0x80u)
        {
            out[0] = (uint8_t)r;
            return 1;
        }
        if (r < 0x800u)
        {
            out[0] = (uint8_t)(0xC0u | (r >> 6));
            out[1] = (uint8_t)(0x80u | (r & 0x3Fu));
            return 2;
        }
        if (r < 0x10000u)
        {
            out[0] = (uint8_t)(0xE0u | (r >> 12));
            out[1] = (uint8_t)(0x80u | ((r >> 6) & 0x3Fu));
            out[2] = (uint8_t)(0x80u | (r & 0x3Fu));
            return 3;
        }
        out[0] = (uint8_t)(0xF0u | (r >> 18));
        out[1] = (uint8_t)(0x80u | ((r >> 12) & 0x3Fu));
        out[2] = (uint8_t)(0x80u | ((r >> 6) & 0x3Fu));
        out[3] = (uint8_t)(0x80u | (r & 0x3Fu));
        return 4;
    }

    static inline int s8_next_rune(s8 str, uint32_t *offset, rune *out)
    {
        uint32_t used;
        if (*offset >= str.len)
        {
            return 0;
        }
        used = utf8_decode(str.data + *offset, str.len - *offset, out);
        *offset += used;
        return 1;
    }

    static inline uint32_t s8_rune_count(s8 str)
    {
        uint32_t offset = 0;
        uint32_t count = 0;
        rune r;
        while (s8_next_rune(str, &offset, &r))
        {
            count++;
        }
        return count;
    }

    static inline int s8_is_valid_utf8(s8 str)
    {
        uint32_t offset = 0;
        while (offset < str.len)
        {
            rune r;
            uint32_t used = utf8_decode(str.data + offset, str.len - offset, &r);
            /* A real U+FFFD in the text is 3 bytes (EF BF BD); a
               replacement for bad input never consumes 3 valid bytes
               that decode to it, so check the bytes, not just r. */
            if (r == R2_RUNE_REPLACEMENT &&
                !(used == 3 && str.data[offset] == 0xEFu &&
                  str.data[offset + 1] == 0xBFu &&
                  str.data[offset + 2] == 0xBDu))
            {
                return 0;
            }
            offset += used;
        }
        return 1;
    }

    static inline int s8_equal(s8 a, s8 b)
    {
        uint32_t idx;
        if (a.len != b.len)
        {
            return 0;
        }
        for (idx = 0; idx < a.len; idx++)
        {
            if (a.data[idx] != b.data[idx])
            {
                return 0;
            }
        }
        return 1;
    }

    static inline s8 s8_slice(s8 str, uint32_t start, uint32_t len)
    {
        if (start > str.len)
        {
            start = str.len;
        }
        if (len > str.len - start)
        {
            len = str.len - start;
        }
        return s8_from_bytes(str.data + start, len);
    }

    static inline uint32_t s8_to_runes(s8 str, rune *dest, uint32_t dest_cap)
    {
        uint32_t offset = 0;
        uint32_t count = 0;
        rune r;
        while (count < dest_cap && s8_next_rune(str, &offset, &r))
        {
            dest[count] = r;
            count++;
        }
        return count;
    }

#endif
    /* implementation */

#ifdef __cplusplus
}
#endif

#endif /* R2_STRINGS */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2020 Rob Rohan
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
