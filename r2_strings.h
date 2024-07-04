/* r2_strings - v0.0 - public domain very basic, utf8 string library
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    This is written with game development in mind.

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
#include <string.h>

    typedef uint32_t rune;

    /**
     * This represents a string of char bytes that can be 
     * represented by UTF-8. When you create this struct
     * by doing `s8 str = S("👋World")` the resulting struct
     * has both the raw byte data and a parsed array of "runes"
     * aka single UTF8 code points.
     * *Note*: not all runes are printable, and some runes are
     * modifiers (like joiner sequences). So take care when 
     * displaying runes in, say a terminal. Displaying 3 runes
     * might not always take 3 text spaces, it might only take 
     * 1 for example.
     */
    typedef struct s8
    {
        // The string data a raw bytes
        char *data;
        // The integer representation of the bytes as utf8
        rune *rune;
        // The size of the string (in bytes)
        unsigned int size;
        // The length of the string (in runes)
        unsigned int len;
    } s8;

    /**
     * Create a utf8 string struct from a char array (has size and
     * utf8 string length properties).
     * 
     * Note: use free_S() when done with the string.
     */
    s8 S(char *);

    /**
     * Frees an allocated S
     */
    void free_S(s8 s);

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

    /**
     * Using the first char of a utf8 char, get the length
     * lengths are in the number of bytes.
     * For example:
     *  1110xxxxx would return 3
     */
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
        rune cp = (*chr++ & utf[bytes]->mask) << shift;
        for (int i = 1; i < bytes; ++i, ++chr)
        {
            shift -= utf[0]->bits_stored;
            cp |= ((char)*chr & utf[0]->mask) << shift;
        }
        return cp;
    }

    /**
     * Given an array of chars, create a utf8 array of runes.
     * In other words, given an array of bytes that might have
     * utf8 chars in it, return an array of integers that can
     * be used for display.
     *
     * Returns the new array length as it might be different from
     * the initial src_size.
     */
    int str_to_utf8(const char *str, int src_size, rune *dest)
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
                dest[len] = r;
                len++;
            }
            else
            {
                // it's just one byte
                dest[len] = str[srci];
                len++;
            }
            // move the source pointer based on
            // the number of utf8 bytes.
            srci += plen;
        }
        return len;
    }

    s8 S(char *s)
    {
        if (s == NULL)
            return (s8){"", NULL, 0, 0};

        // string length in bytes
        size_t l = strlen(s);
        // string as an array of integers
        rune *i = calloc(l, sizeof(rune));
        if (!i)
        {
            printf("mem failure, exiting \n");
            exit(EXIT_FAILURE);
        }
        size_t u8l = str_to_utf8(s, l, i);
        return (s8){s, i, l, u8l};
    }

    void free_S(s8 s)
    {
        free(s.rune);
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
