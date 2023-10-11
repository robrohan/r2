/*
    char - 8 bits
    short - 16 bits
    int - 32 bits
    long long - 64 bits
*/
#ifndef R2_STRINGS
#define R2_STRINGS

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <string.h>

    typedef uint32_t rune;
    typedef struct s8
    {
        char *data;
        unsigned int size;
        unsigned int len;
    } s8;

    /**
     * Using the first char of a utf8 char, get the length
     * lengths are in the number of bytes.
     * For example:
     *  1110xxxxx would return 3
     */
    int utf8_len(const char ch);
    /**
     * Given a set of chars (max 4) turn that into a rune. Meaning,
     * look at the first bits of the chars and figure out if they are
     * continuation chars or starter chars and make a rune out of it
     * (if valid).
     */
    rune to_rune(const char chr[4]);
    /**
     * Given an array of chars, create a utf8 array of runes.
     * In other words, given an array of bytes that might have
     * utf8 chars in it, return an array of integers that can
     * be used for display.
     *
     * Returns the new array length as it might be different from
     * the initial src_size.
     */
    int str_to_utf8(const char *str, int src_size);
    /**
     * Create a utf8 string struct from a char array (has size and 
     * utf8 string length properties)
     */
    s8 S(char *);

/* implementation */
#ifdef R2_STRINGS_IMPLEMENTATION

    typedef struct
    {
        char mask;       /* char data will be bitwise AND with this */
        char lead;       /* start bytes of current char in utf-8 encoded character */
        uint32_t beg;    /* beginning of codepoint range */
        uint32_t end;    /* end of codepoint range */
        int bits_stored; /* the number of bits from the codepoint that fits in char */
    } utf_t;

    utf_t *utf[] = {
        // clang-format off
        /*             mask        lead        beg      end       bits */
        [0] = &(utf_t){0b00111111, 0b10000000, 0,       0,        6},
        [1] = &(utf_t){0b01111111, 0b00000000, 0000,    0177,     7},
        [2] = &(utf_t){0b00011111, 0b11000000, 0200,    03777,    5},
        [3] = &(utf_t){0b00001111, 0b11100000, 04000,   0177777,  4},
        [4] = &(utf_t){0b00000111, 0b11110000, 0200000, 04177777, 3},
        &(utf_t){0},
        // clang-format on
    };

    s8 S(char *s)
    {
        if (s == NULL) return (s8){"", 0, 0};
        return (s8){
            s, 
            strlen(s), 
            str_to_utf8(s, strlen(s)) 
        };
    }

    int utf8_len(const char ch)
    {
        int len = 0;
        for (utf_t **u = utf; *u; ++u)
        {
            if ((ch & ~(*u)->mask) == (*u)->lead)
            {
                break;
            }
            ++len;
        }
        return len;
    }

    rune to_rune(const char chr[4])
    {
        int bytes = utf8_len(*chr);
        int shift = utf[0]->bits_stored * (bytes - 1);
        rune codep = (*chr++ & utf[bytes]->mask) << shift;
        for (int i = 1; i < bytes; ++i, ++chr)
        {
            shift -= utf[0]->bits_stored;
            codep |= ((char)*chr & utf[0]->mask) << shift;
        }
        return codep;
    }

    int str_to_utf8(const char *str, int src_size)
    {
        char tmp[5] = {0};
        int srci = 0;
        int len = 0;
        for (int i = 0; i < src_size; i++)
        {
            if (str[srci] == 0)
                break;

            // look at the first char to see if the bytes
            // say this rune is more than one byte
            int plen = utf8_len(str[srci]);
            if (plen > 1)
            {
                // turn multibyte into a single int
                for (int c = 0; c < plen; c++)
                {
                    tmp[c] = str[srci + c];
                }
                rune r = to_rune(tmp);
                // dest[len] = r;
                len++;
            }
            else
            {
                // it's just one byte
                // dest[len] = str[srci];
                len++;
            }
            // move the source pointer based on
            // the number of utf8 bytes.
            srci += plen;
        }
        return len;
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