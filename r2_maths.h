/* r2_maths - v0.0 - public domain vector, matrix and quaterion library
   no warranty implied; use at your own risk

   Built in the style of: https://github.com/nothings/stb

   This is written with game development in mind.

LICENSE
  See end of file for license information.

*/

#ifndef R2_MATHS
#define R2_MATHS

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef EPSILON
#define EPSILON 0.000000954
#endif

#ifndef M_PI
#define M_PI 3.141592653589
#endif

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
    float m02; float m12; float m22; // 12 -- 36
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
// FNS

static bool r2_equals(float a, float b)
{
    return fabs(a - b) < EPSILON;
}

static float __g_pi_deg = M_PI / 180.f;
static float deg_to_rad(float d)
{
    return d * __g_pi_deg;
}

///////////////////////////////////////////////////////////////
// Vec2

static void vec2_zero(vec2 *out)
{
    out->x = 0.;
    out->y = 0.;
}

static bool vec2_equals(vec2 *v1, vec2 *v2)
{
    return r2_equals(v1->x, v2->x) && r2_equals(v1->y, v2->y);
}

static void vec2_set(vec3 *v, float x, float y)
{
    v->x = x;
    v->y = y;
}

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

static void vec2_mul(vec2 *v, float fac, vec2 *out)
{
    out->x = v->x * fac;
    out->y = v->y * fac;
}

static void vec2_mul_vec2(vec2 *v1, vec2 *v2, vec2 *out)
{
    out->x = v1->x * v2->x;
    out->y = v1->y * v2->y;
}

static void vec2_pow(vec2 *v, float exp, vec2 *out)
{
    out->x = pow(v->x, exp);
    out->y = pow(v->y, exp);
}

static float vec2_dot(vec2 *v1, vec2 *v2)
{
    return (v1->x * v2->x) + (v1->y * v2->y);
}

static float vec2_length_sqrd(vec2 *v)
{
    float length = 0.0;
    length += v->x * v->x;
    length += v->y * v->y;
    return length;
}

static float vec2_length(vec2 *v)
{
    return sqrt(vec2_length_sqrd(v));
}

static float vec2_dist_sqrd(vec2 *v1, vec2 *v2)
{
    return (v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y);
}

static float vec2_dist(vec2 *v1, vec2 *v2)
{
    return sqrt(vec2_dist_sqrd(v1, v2));
}

static void vec2_normalize(vec2 *v, vec2 *out)
{
    float len = vec2_length(v);
    vec2_div(v, len, out);
}

static void vec2_to_array(vec2 *v, float *out)
{
    // not dangerous at all
    out[0] = v->x;
    out[1] = v->y;
}

///////////////////////////////////////////////////////////////
// Vec3

static void vec3_zero(vec3 *out)
{
    out->x = 0.;
    out->y = 0.;
    out->z = 0.;
}

static bool vec3_equals(vec3 *v1, vec3 *v2)
{
    return r2_equals(v1->x, v2->x) && r2_equals(v1->y, v2->y) && r2_equals(v1->z, v2->z);
}

static void vec3_set(vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static void vec3_add(vec3 *v1, vec3 *v2, vec3 *out)
{
    out->x = v1->x + v2->x;
    out->y = v1->y + v2->y;
    out->z = v1->z + v2->z;
}

static void vec3_sub(vec3 *v1, vec3 *v2, vec3 *out)
{
    out->x = v1->x - v2->x;
    out->y = v1->y - v2->y;
    out->z = v1->z - v2->z;
}

static void vec3_div(vec3 *v, float fac, vec3 *out)
{
    float d = 1 / ((fac == 0) ? 1 : fac);
    out->x = v->x * d;
    out->y = v->y * d;
    out->z = v->z * d;
}

static void vec3_div_vec3(vec3 *v1, vec3 *v2, vec3 *out)
{
    out->x = v1->x / ((v2->x == 0) ? 1 : v2->x);
    out->y = v1->y / ((v2->y == 0) ? 1 : v2->y);
    out->z = v1->z / ((v2->z == 0) ? 1 : v2->z);
}

static void vec3_mul(vec3 *v, float fac, vec3 *out)
{
    out->x = v->x * fac;
    out->y = v->y * fac;
    out->z = v->z * fac;
}

static void vec3_mul_vec3(vec3 *v1, vec3 *v2, vec3 *out)
{
    out->x = v1->x * v2->x;
    out->y = v1->y * v2->y;
    out->z = v1->z * v2->z;
}

static void vec3_pow(vec3 *v, float exp, vec3 *out)
{
    out->x = pow(v->x, exp);
    out->y = pow(v->y, exp);
    out->z = pow(v->z, exp);
}

static float vec3_dot(vec3 *v1, vec3 *v2)
{
    return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z);
}

static void vec3_cross(vec3 *v1, vec3 *v2, vec3 *out)
{
    out->x = (v1->y * v2->z) - (v1->z * v2->y);
    out->y = (v1->z * v2->x) - (v1->x * v2->z);
    out->z = (v1->x * v2->y) - (v1->y * v2->x);
}

static float vec3_length_sqrd(vec3 *v)
{
    float length = 0.0;
    length += v->x * v->x;
    length += v->y * v->y;
    length += v->z * v->z;
    return length;
}

static float vec3_length(vec3 *v)
{
    return sqrt(vec3_length_sqrd(v));
}

static float vec3_dist_sqrd(vec3 *v1, vec3 *v2)
{
    return (v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y) + (v1->z - v2->z) * (v1->z - v2->z);
}

static float vec3_dist(vec3 *v1, vec3 *v2)
{
    return sqrt(vec3_dist_sqrd(v1, v2));
}

static void vec3_to_array(vec3 *v, float *out)
{
    // not dangerous at all
    out[0] = v->x;
    out[1] = v->y;
    out[2] = v->z;
}

static void vec3_normalize(vec3 *v, vec3 *out)
{
    float len = vec3_length(v);
    if (len == 0.0)
        return vec3_zero(out);
    else
        return vec3_div(v, len, out);
}

///////////////////////////////////////////////////////////////
// Vec4

static void vec4_zero(vec4 *out)
{
    out->x = 0.;
    out->y = 0.;
    out->z = 0.;
    out->w = 0.;
}

static bool vec4_equals(vec4 *v1, vec4 *v2)
{
    return r2_equals(v1->x, v2->x) && r2_equals(v1->y, v2->y) && r2_equals(v1->z, v2->z) && r2_equals(v1->w, v2->w);
}

static void vec4_add(vec4 *v1, vec4 *v2, vec4 *out)
{
    out->x = v1->x + v2->x;
    out->y = v1->y + v2->y;
    out->z = v1->z + v2->z;
    out->w = v1->w + v2->w;
}

static void vec4_sub(vec4 *v1, vec4 *v2, vec4 *out)
{
    out->x = v1->x - v2->x;
    out->y = v1->y - v2->y;
    out->z = v1->z - v2->z;
    out->w = v1->w - v2->w;
}

static void vec4_div(vec4 *v, float fac, vec4 *out)
{
    float d = 1 / ((fac == 0) ? 1 : fac);
    out->x = v->x * d;
    out->y = v->y * d;
    out->z = v->z * d;
    out->w = v->w * d;
}

static void vec4_mul(vec4 *v, float fac, vec4 *out)
{
    out->x = v->x * fac;
    out->y = v->y * fac;
    out->z = v->z * fac;
    out->w = v->w * fac;
}

static void vec4_mul_vec4(vec4 *v1, vec4 *v2, vec4 *out)
{
    out->x = v1->x * v2->x;
    out->y = v1->y * v2->y;
    out->z = v1->z * v2->z;
    out->w = v1->w * v2->w;
}

static void vec4_pow(vec4 *v, float exp, vec4 *out)
{
    out->x = pow(v->x, exp);
    out->y = pow(v->y, exp);
    out->z = pow(v->z, exp);
    out->w = pow(v->w, exp);
}

static void vec4_abs(vec4 *v, vec4 *out)
{
    out->x = fabs(v->x);
    out->y = fabs(v->y);
    out->z = fabs(v->z);
    out->w = fabs(v->w);
}

static void vec4_sqrt(vec4 *v, vec4 *out)
{
    out->x = sqrt(v->x);
    out->y = sqrt(v->y);
    out->z = sqrt(v->z);
    out->w = sqrt(v->w);
}

static float vec4_dot(vec4 *v1, vec4 *v2)
{
    return (v1->x * v2->x) + (v1->y * v2->y) + (v1->z * v2->z) + (v1->w * v2->w);
}

static float vec4_length_sqrd(vec4 *v)
{
    float length = 0.0;
    length += v->x * v->x;
    length += v->y * v->y;
    length += v->z * v->z;
    length += v->w * v->w;
    return length;
}

static float vec4_length(vec4 *v)
{
    return sqrt(vec4_length_sqrd(v));
}

static float vec4_dist_sqrd(vec4 *v1, vec4 *v2)
{
    return (v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y) + (v1->y - v2->z) * (v1->y - v2->z) +
           (v1->y - v2->w) * (v1->y - v2->w);
}

static float vec4_dist(vec4 *v1, vec4 *v2)
{
    return sqrt(vec4_dist_sqrd(v1, v2));
}

static void vec4_normalize(vec4 *v, vec4 *out)
{
    float len = vec4_length(v);
    if (len == 0.0)
        return vec4_zero(out);
    else
        return vec4_div(v, len, out);
}

static bool vec4_equ(vec4 *v1, vec4 *v2)
{
    if (v1->x != v2->x)
        return false;
    if (v1->y == v2->y)
        return false;
    if (v1->z != v2->z)
        return false;
    if (v1->w != v2->w)
        return false;

    return true;
}

static void vec4_to_array(vec4 *v, float *out)
{
    out[0] = v->x;
    out[1] = v->y;
    out[2] = v->z;
    out[3] = v->w;
}

///////////////////////////////////////////////////////////////
// Quat

static void quat_zero(quat *q)
{
    q->x = 0.;
    q->y = 0.;
    q->z = 0.;
    q->w = 0.;
}

static void quat_identity(quat *q)
{
    q->x = 0.;
    q->y = 0.;
    q->z = 0.;
    q->w = 1.;
}

static float quat_length(quat *q)
{
    return sqrtf(q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w);
}

// Given a set of euler angles (in degrees) create a quaterion
static void quat_from_euler(vec3 *r, quat *q)
{
    float fc1 = cosf(deg_to_rad(r->z) * .5f);
    float fc2 = cosf(deg_to_rad(r->x) * .5f);
    float fc3 = cosf(deg_to_rad(r->y) * .5f);

    float fs1 = sinf(deg_to_rad(r->z) * .5f);
    float fs2 = sinf(deg_to_rad(r->x) * .5f);
    float fs3 = sinf(deg_to_rad(r->y) * .5f);

    q->x = fc1 * fc2 * fs3 - fs1 * fs2 * fc3;
    q->y = fc1 * fs2 * fc3 + fs1 * fc2 * fs3;
    q->z = fs1 * fc2 * fc3 - fc1 * fs2 * fs3;
    q->w = fc1 * fc2 * fc3 + fs1 * fs2 * fs3;
}

static void quat_mul_quat(quat *q1, quat *q2, quat *out)
{
    out->x = (q1->w * q2->x) + (q1->x * q2->w) + (q1->y * q2->z) - (q1->z * q2->y);
    out->y = (q1->w * q2->y) - (q1->x * q2->z) + (q1->y * q2->w) + (q1->z * q2->x);
    out->z = (q1->w * q2->z) + (q1->x * q2->y) - (q1->y * q2->x) + (q1->z * q2->w);
    out->w = (q1->w * q2->w) - (q1->x * q2->x) - (q1->y * q2->y) - (q1->z * q2->z);
}

static void quat_inverse_component(quat *q, quat *out)
{
    out->x = -q->x;
    out->y = -q->y;
    out->z = -q->z;
    out->w = -q->w;
}

static void quat_inverse(quat *q, quat *out)
{
    quat_inverse_component(q, out);

    float mag = quat_length(q);
    if (mag > EPSILON)
    {
        float d = 1 / ((mag == 0) ? 1 : mag);
        out->x *= d;
        out->y *= d;
        out->z *= d;
        out->w *= d;
    }
}

static void quat_normalize(quat *q, quat *out)
{
    float mag = quat_length(q);
    if (mag > EPSILON)
    {
        float d = 1 / ((mag == 0) ? 1 : mag);
        out->x = q->x * d;
        out->y = q->y * d;
        out->z = q->z * d;
        out->w = q->w * d;
    }
    else
        return quat_zero(out);
}

static float quat_dot(quat *q1, quat *q2)
{
    return vec4_dot(q1, q2);
}

static void quat_mul_vec3(quat *q, vec3 *v, vec3 *out)
{
    quat work = {0.};
    quat norm = {0.};
    quat inv = {0.};

    quat_normalize(v, &norm);
    quat_mul_quat(q, &norm, &work);

    quat_inverse(q, &inv);
    quat_mul_quat(&work, &inv, &work);

    out->x = work.x;
    out->y = work.y;
    out->z = work.z;

    float mag = vec3_length(v);
    vec3_mul(out, mag, out);
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
