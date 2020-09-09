/* r2_maths - v0.0 - public domain vector, matrix and quaterion library
   no warranty implied; use at your own risk

   Built in the style of: https://github.com/nothings/stb

   This is written with game development in mind.

LICENSE
  See end of file for license information.

*/

#ifndef R2_MATHS
#define R2_MATHS

#ifndef R2_NO_STDIO
#include <stdio.h>
#endif // R2_NO_STDIO

typedef struct s_vec2
{
    float x; // 4
    float y; // 4 -- 8
} vec2;

typedef struct s_vec3
{
    float x; // 4
    float y; // 4 -- 8
    float z; // 4
    float w; // 4 -- 16
} vec3, vec4, quat;

typedef struct s_mat3
{
    // clang-format off
    float m00; float m10; float m20; // 12
    float m01; float m11; float m21; // 12
    float m02; float m12; float m22; // 12
    float m03; float m13; float m23; // 12 -- 48
    // clang-format on
} mat3;

typedef struct s_mat4
{
    // clang-format off
    float m00; float m10; float m20; float m30; // 16
    float m01; float m11; float m21; float m31; // 16
    float m02; float m12; float m22; float m32; // 16
    float m03; float m13; float m23; float m33; // 16 -- 64
    // clang-format on
} mat4;

///////////////////////////////////////////////////////////////

static void vec2_add(vec2 *v1, vec2 *v2, vec2 *out)
{
    out->x = v1->x + v2->x;
    out->y = v1->y + v2->y;
}

static void vec2_sub(vec2 *v1, vec2 *v2, vec2 *out)
{
    out->x = v1->x - v2->x;
    out->y = v1->y - v2->y;
}

static void vec2_div(vec2 *v, float fac, vec2 *out)
{
    float d = 1 / ((fac == 0) ? 1 : fac);
    out->x = v->x * d;
    out->y = v->y * d;
}

static void vec2_div_vec2(vec2 *v1, vec2 *v2, vec2 *out)
{
    out->x = v1->x / ((v2->x == 0) ? 1 : v2->x);
    out->y = v1->y / ((v2->y == 0) ? 1 : v2->y);
}

///////////////////////////////////////////////////////////////

#endif

/*
   revision history:
    0.0   (2020-09-09) Initial bits
*/

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
