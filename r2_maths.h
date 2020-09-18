/* r2_maths - v0.0 - public domain vector, matrix and quaterion library
   no warranty implied; use at your own risk

   Built in the style of: https://github.com/nothings/stb

   This is written with game development in mind.

LICENSE
  See end of file for license information.

*/

#ifndef R2_MATHS
#define R2_MATHS

#ifdef __cplusplus
extern "C"
{
#endif

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef EPSILON
#define EPSILON 0.000000954
#endif

#ifndef M_PI
#define M_PI 3.141592653589
#endif

    typedef union u_vec2 {
        float a_vec2[2];
        struct
        {
            float x, y;
        };
    } vec2;

    typedef union u_vec4 {
        float a_vec4[4];
        struct
        {
            float x; // 4
            float y; // 4 -- 8
            float z; // 4
            float w; // 4 -- 16
        };
    } vec3, vec4, quat;

    typedef union u_mat3 {
        float a_mat3[9];
        struct
        {
            // clang-format off
	  float m00; float m10; float m20; // 12
	  float m01; float m11; float m21; // 12
	  float m02; float m12; float m22; // 12 -- 36
            // clang-format on
        };
    } mat3;

    typedef union u_mat4 {
        float a_mat4[16];
        struct
        {
            // clang-format off
	  float m00; float m10; float m20; float m30; // 16
	  float m01; float m11; float m21; float m31; // 16
	  float m02; float m12; float m22; float m32; // 16
	  float m03; float m13; float m23; float m33; // 16 -- 64
            // clang-format on
        };
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
        float length = v->x * v->x + v->y * v->y + v->z * v->z;
        return length;
    }

    static float vec3_length(vec3 *v)
    {
        return sqrt(vec3_length_sqrd(v));
    }

    static float vec3_dist_sqrd(vec3 *v1, vec3 *v2)
    {
        return (v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y) +
               (v1->z - v2->z) * (v1->z - v2->z);
    }

    static float vec3_dist(vec3 *v1, vec3 *v2)
    {
        return sqrt(vec3_dist_sqrd(v1, v2));
    }

    static void vec3_to_array(vec3 *v, float *out)
    {
        // this is fine.
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

    static void vec4_set(vec4 *v, float *ary)
    {
        // what could possibly go wrong?
        v->x = ary[0];
        v->y = ary[1];
        v->z = ary[2];
        v->w = ary[3];
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

    static float vec4_length(vec4 *v)
    {
        return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z + v->w * v->w);
    }

    static float vec4_dist_sqrd(vec4 *v1, vec4 *v2)
    {
        return (v1->x - v2->x) * (v1->x - v2->x) + (v1->y - v2->y) * (v1->y - v2->y) +
               (v1->y - v2->z) * (v1->y - v2->z) + (v1->y - v2->w) * (v1->y - v2->w);
    }

    static float vec4_dist(vec4 *v1, vec4 *v2)
    {
        return sqrt(vec4_dist_sqrd(v1, v2));
    }

    static void vec4_normalize(vec4 *v, vec4 *out)
    {
        float mag = vec4_length(v);
        if (mag < EPSILON)
            return vec4_zero(out);
        else
        {
            float d = 1 / mag;
            out->x = v->x * d;
            out->y = v->y * d;
            out->z = v->z * d;
            out->w = v->w * d;
        }
    }

    static void vec4_to_array(vec4 *v, float *out)
    {
        // you break it, you buy it.
        out[0] = v->x;
        out[1] = v->y;
        out[2] = v->z;
        out[3] = v->w;
    }

    ///////////////////////////////////////////////////////////////
    // Quat
    // http://www.tobynorris.com/work/prog/csharp/quatview/help/orientations_and_quaternions.htm

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

    static void quat_add(quat *q1, quat *q2, quat *out)
    {
        vec4_add(q1, q2, out);
    }

    static void quat_sub(quat *q1, quat *q2, quat *out)
    {
        vec4_sub(q1, q2, out);
    }

    static float quat_dot(quat *q1, quat *q2)
    {
        return vec4_dot(q1, q2);
    }

    static float quat_length(quat *q)
    {
        return vec4_length(q);
    }
    static float quat_magnitude(quat *q)
    {
        return quat_length(q);
    }

    static void quat_rot2q(vec3 *axis, float theta, quat *out)
    {
        vec4_mul(axis, sinf(theta * .5), out);
        float d = cosf(theta * .5);
        out->w = d;
    }

    // Given a set of euler angles (in radians) create a quaterion
    static void quat_from_euler(vec3 *r, quat *q)
    {
        float roll = r->x;
        float pitch = r->y;
        float yaw = r->z;

        float cy = cosf(yaw * .5);
        float sy = sinf(yaw * .5);

        float cp = cosf(pitch * .5);
        float sp = sinf(pitch * .5);

        float cr = cosf(roll * .5);
        float sr = sinf(roll * .5);

        // Warning: in most maths apps and documentation about quats the w
        // part (real) is in the front. We have some names here that make it
        // a bit more difficult to see what is going on.
        q->w = cr * cp * cy + sr * sp * sy; // r
        q->x = sr * cp * cy - cr * sp * sy; // i
        q->y = cr * sp * cy + sr * cp * sy; // j
        q->z = cr * cp * sy - sr * sp * cy; // k
    }

    static void quat_mul_quat(quat *q1, quat *q2, quat *out)
    {
        // i^2 = j^2 = k^2 = ijk = -1

        // q1 = a+bi+cj+dk
        float a = q1->w;
        float b = q1->x;
        float c = q1->y;
        float d = q1->z;
        // q2 = e+fi+gj+hk
        float e = q2->w;
        float f = q2->x;
        float g = q2->y;
        float h = q2->z;

        //   |  i  j  k
        //--------------
        // i | -1  k -j
        // j | -k -1  i
        // k |  j -i -1
        out->w = a * e - b * f - c * g - d * h;
        out->x = a * f + b * e + c * h - d * g;
        out->y = a * g - b * h + c * e + d * f;
        out->z = a * h + b * g - c * f + d * e;
    }

    static void quat_conj(quat *q, quat *out)
    {
        out->w = q->w;
        out->x = -q->x;
        out->y = -q->y;
        out->z = -q->z;
    }

    static void quat_normalize(quat *q, quat *out)
    {
        return vec4_normalize(q, out);
    }

    static void quat_mul_vec3(quat *q, vec3 *v, vec3 *out)
    {
        vec3 work; // = [ 0., 0., 0., 0. ];
        quat inv;  // = [ 0., 0., 0., 0. ];

        quat_conj(q, &inv);
        quat_mul_quat(q, v, &work);
        quat_mul_quat(&work, &inv, out);
    }

    static void quat_mat4(quat *q, mat4 *out)
    {
        float a = q->w;
        float b = q->x;
        float c = q->y;
        float d = q->z;

        // a -b -c -d
        // b  a -d  c
        // c  d  a -b
        // d -c  b  a
        out->m00 = a;
        out->m01 = b;
        out->m02 = c;
        out->m03 = d;

        out->m10 = -b;
        out->m11 = a;
        out->m12 = d;
        out->m13 = -c;

        out->m20 = -c;
        out->m21 = -d;
        out->m22 = a;
        out->m23 = b;

        out->m30 = -d;
        out->m31 = c;
        out->m32 = -b;
        out->m33 = a;
    }

    ///////////////////////////////////////////////////////////////
    // Mat4

    // Fills an mat4 with an array. It expects an array of values
    // that are given in sets of 4s one *row* at a time.
    static void mat4_set(mat4 *m, float *arry)
    {
        m->m00 = arry[0];
        m->m01 = arry[4];
        m->m02 = arry[8];
        m->m03 = arry[12];

        m->m10 = arry[1];
        m->m11 = arry[5];
        m->m12 = arry[9];
        m->m13 = arry[13];

        m->m20 = arry[2];
        m->m21 = arry[6];
        m->m22 = arry[10];
        m->m23 = arry[14];

        m->m30 = arry[3];
        m->m31 = arry[7];
        m->m32 = arry[11];
        m->m33 = arry[15];
    }

    static void mat4_transform(vec4 *p, mat4 *mat, vec4 *out)
    {
        out->x = (mat->m00 * p->x) + (mat->m01 * p->y) + (mat->m02 * p->z) + (mat->m03 * p->w);
        out->y = (mat->m10 * p->x) + (mat->m11 * p->y) + (mat->m12 * p->z) + (mat->m13 * p->w);
        out->z = (mat->m20 * p->x) + (mat->m21 * p->y) + (mat->m22 * p->z) + (mat->m23 * p->w);
        out->w = (mat->m30 * p->x) + (mat->m31 * p->y) + (mat->m32 * p->z) + (mat->m33 * p->w);
    }

    static void mat4_mul(mat4 *m1, mat4 *m2, mat4 *out)
    {
        float r1,r2,r3,r4,c1,c2,c3,c4,a1;
        unsigned char i, q;
        unsigned char c = 0;
        for (i = 0; i < 16; i += 4)
        {
	    // Row
            r1 = m1->a_mat4[i + 0];
            r2 = m1->a_mat4[i + 1];
            r3 = m1->a_mat4[i + 2];
            r4 = m1->a_mat4[i + 3];

	    for(q = 0; q < 4; q++) {
	      // Column
	      c1 = m2->a_mat4[q + 0];
	      c2 = m2->a_mat4[q + 4];
	      c3 = m2->a_mat4[q + 8];
	      c4 = m2->a_mat4[q + 12];

	      a1 = r1 * c1 + r2 * c2 + r3 * c3 + r4 * c4;
	      out->a_mat4[i+q] = a1;
	    }
        }
    }

#ifdef __cplusplus
}
#endif

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
