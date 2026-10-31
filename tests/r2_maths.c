#define R2_MATHS_IMPLEMENTATION
#include "../r2_maths.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../r2_unit.h"


static const char *test_vec2_add(void)
{
    vec2 v1 = {.x = 10, .y = 10};
    vec2 v2 = {.x = 10.f, .y = 10.f};
    vec2 v3 = {0};
    vec2_add(&v1, &v2, &v3);
    r2_assert("Vec2 add failed", (v3.x == 20.f && v3.y == 20.f));
    return 0;
}

static const char *test_vec2_sub(void)
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 10.f, .y = 10.f};
    vec2 v3 = {0};
    vec2_sub(&v1, &v2, &v3);
    r2_assert("Vec2 sub failed", (v3.x == 0.f && v3.y == 0.f));
    return 0;
}

static const char *test_vec2_div(void)
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v3 = {0};
    vec2_div(&v1, 2., &v3);
    r2_assert("Vec2 div failed", (v3.x == 5.f && v3.y == 5.f));
    return 0;
}

static const char *test_vec2_div_zero(void)
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v3 = {0};
    vec2_div(&v1, 0, &v3);
    r2_assert("Vec2 div zero failed", (v3.x == 10.f && v3.y == 10.f));
    return 0;
}

static const char *test_vec2_div_vec2(void)
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 5.f, .y = 2.f};
    vec2 v3 = {0};
    vec2_div_vec2(&v1, &v2, &v3);
    r2_assert("Vec2 div failed", (v3.x == 2.f && v3.y == 5.f));
    return 0;
}

static const char *test_vec2_div_vec2_zero(void)
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 0.f, .y = 0.f};
    vec2 v3 = {0};
    vec2_div_vec2(&v1, &v2, &v3);
    r2_assert("Vec2 div zero failed", (v3.x == 10.f && v3.y == 10.f));
    return 0;
}

static const char *test_vec2_dot(void)
{
    vec2 v1 = {.x = 3.f, .y = 3.f};
    vec2 v2 = {.x = -3.f, .y = -3.f};
    float theta = vec2_dot(&v1, &v2);
    r2_assert("vec2 dot was wonky", theta == -18.);
    return 0;
}

static const char *test_vec3_add(void)
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -3.f, .y = -3.f, .z = -3.f};
    vec4 out = {0};
    vec3_add(&v1, &v2, &out);
    r2_assert("vec3 add is wrong", (out.x == 0.f && out.y == 0.f && out.z == 0.f));
    return 0;
}

static const char *test_vec3_mul_clobber(void)
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3_mul(&v1, 3, &v1);
    r2_assert("vec3 mul is wrong", (v1.x == 9.f && v1.y == 9.f && v1.z == 9.f));
    return 0;
}

static const char *test_vec3_cross(void)
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
    vec4 out = {0};
    vec3_cross(&v1, &v2, &out);
    r2_assert("vec3 cross is wrong", (out.x == -180.f && out.y == 0.f && out.z == 180.f));
    return 0;
}

static const char *test_vec3_equal(void)
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
    r2_assert("vec3 equal is wrong", !vec3_equals(&v1, &v2));
    return 0;
}

static const char *test_vec3_normalize(void)
{
    vec3 v1 = {.x = 10.f, .y = 0.f, .z = 0.f};
    vec4 out = {0};
    vec3_normalize(&v1, &out);
    r2_assert("vec3 normalize is wrong", (out.x == 1.f && out.y == 0.f && out.z == 0.f));
    return 0;
}

// Magnitude, Length...
static const char *test_vec3_length(void)
{
    vec3 v1 = {.x = 10.f, .y = 0.f, .z = 0.f};
    float l = vec3_length(&v1);
    r2_assert("vec3 length is wrong", l == 10.f);
    return 0;
}

static const char *test_vec4_div(void)
{
    vec3 v1 = {.x = 10.f, .y = 10.f, .z = 12.f, .w = 12.f};
    vec4 out = {0};
    vec4_div(&v1, 2., &out);
    r2_assert("vec4 div is wrong", out.x == 5.f && out.y == 5.f && out.z == 6.f && out.w == 6.f);
    return 0;
}

static const char *test_vec4_normalize(void)
{
    vec4 v1 = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    vec4 out = {0};
    vec4_normalize(&v1, &out);
    // Octave: 0.18257   0.36515   0.54772   0.73030
    // clang-format off
    r2_assert("vec4 normalize is wrong",
        r2_equals(out.x, .182574) && r2_equals(out.y, 0.365148) &&
        r2_equals(out.z, 0.547723) && r2_equals(out.w, 0.730297));
    // clang-format on

    // Normalizing a zero vector should give (0,0,0,0), not (0,0,0,1)
    vec4 zero = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 zero_out = {0};
    vec4_normalize(&zero, &zero_out);
    r2_assert("vec4 normalize zero is wrong",
              r2_equals(zero_out.x, 0.f) && r2_equals(zero_out.y, 0.f) &&
              r2_equals(zero_out.z, 0.f) && r2_equals(zero_out.w, 0.f));
    return 0;
}

static const char *test_vec4_cross(void)
{
    vec4 v1 = {.x = 1.f, .y = 1.f, .z = 0.f, .w = 0.f};
    vec4 v2 = {.x = 0.f, .y = 1.f, .z = 1.f, .w = 0.f};
    vec4 out = {0};
    vec3_cross(&v1, &v2, &out);

    // Octave: 1 -1 1
    r2_assert("vec4 cross is wrong", r2_equals(out.x, 1.) && r2_equals(out.y, -1.) && r2_equals(out.z, 1.));
    return 0;
}

static const char *test_vec4_dist_sqrd(void)
{
    // Identical points: distance should be zero
    vec4 v1 = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    vec4 v2 = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    r2_assert("vec4 dist sqrd same point is wrong", r2_equals(vec4_dist_sqrd(&v1, &v2), 0.f));

    // Axis-aligned: only x differs by 3, so dist_sqrd = 9
    vec4 v3 = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 v4 = {.x = 3.f, .y = 0.f, .z = 0.f, .w = 0.f};
    r2_assert("vec4 dist sqrd x-axis is wrong", r2_equals(vec4_dist_sqrd(&v3, &v4), 9.f));

    // Only z differs — this is what the old bug broke (used v1->y instead of v1->z)
    vec4 v5 = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 v6 = {.x = 0.f, .y = 0.f, .z = 4.f, .w = 0.f};
    r2_assert("vec4 dist sqrd z-axis is wrong", r2_equals(vec4_dist_sqrd(&v5, &v6), 16.f));

    // Only w differs — same bug would have masked this too
    vec4 v7 = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 v8 = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 5.f};
    r2_assert("vec4 dist sqrd w-axis is wrong", r2_equals(vec4_dist_sqrd(&v7, &v8), 25.f));

    // All components: 1²+2²+3²+4² = 1+4+9+16 = 30
    vec4 v9  = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 v10 = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    r2_assert("vec4 dist sqrd all components is wrong", r2_equals(vec4_dist_sqrd(&v9, &v10), 30.f));
    return 0;
}

static const char *test_vec4_dist(void)
{
    // Distance from origin to (3,4,0,0) = 5 (3-4-5 triangle)
    vec4 origin = {.x = 0.f, .y = 0.f, .z = 0.f, .w = 0.f};
    vec4 v      = {.x = 3.f, .y = 4.f, .z = 0.f, .w = 0.f};
    r2_assert("vec4 dist is wrong", r2_equals(vec4_dist(&origin, &v), 5.f));

    // Distance from point to itself is zero
    vec4 p = {.x = 7.f, .y = 3.f, .z = 1.f, .w = 2.f};
    r2_assert("vec4 dist same point is wrong", r2_equals(vec4_dist(&p, &p), 0.f));
    return 0;
}

static const char *test_quat_rot2q(void)
{
    quat out = {0};

    vec3 axis = {.x = 0, .y = 1, .z = 0};
    float angle = M_PI / 4;

    quat_rot2q(&axis, angle, &out);

    // Octave: q = 0.9239 + 0i + 0.3827j + 0k
    float r = out.w;
    float i = out.x;
    float j = out.y;
    float k = out.z;

    // clang-format off
    r2_assert("quat rot2q is wrong",
              r2_equals(r, 0.923880) && r2_equals(i, 0) && 
              r2_equals(j, 0.382683) && r2_equals(k, 0.));
    // clang-format on
    return 0;
}

static const char *test_quat_from_euler(void)
{
    vec3 v1 = {.x = M_PI / 2., .y = 0.f, .z = 0.f};
    quat out = {0};
    quat_from_euler(&v1, &out);

    // Octave: q = 0.7071 + 0.7071i + 0j + 0k
    float r = out.w;
    float i = out.x;
    float j = out.y;
    float k = out.z;

    // clang-format off
    r2_assert("quat euler is wrong",
              r2_equals(r, 0.707107) && r2_equals(i, 0.707107) && 
              r2_equals(j, 0.) && r2_equals(k, 0.));
    // clang-format on
    return 0;
}

static const char *test_quat_length(void)
{
    quat q1 = {.x = 0.000000, .y = 0.850904, .z = 0.000000, .w = 0.525322};
    float actual = quat_length(&q1);
    r2_assert("quat length is wrong", r2_equals(actual, 1.));
    return 0;
}

static const char *test_quat_normalize(void)
{
    quat q1 = {.x = .5f, .y = .3f, .z = .2f, .w = .1f};
    quat out = {0};
    quat_normalize(&q1, &out);
    // clang-format off
    r2_assert("quat normalize is wrong", 
        r2_equals(out.x, .800641) && r2_equals(out.y, 0.480384) &&
        r2_equals(out.z, 0.320256) && r2_equals(out.w, 0.160128));
    // clang-format on
    return 0;
}

static const char *test_quat_conj(void)
{
    quat q1 = {.x = .5f, .y = .3f, .z = .2f, .w = .1f};
    quat out = {.x = 0., .y = 0., .z = 0., .w = 0.};
    quat_conj(&q1, &out);

    // clang-format off
    r2_assert("quat conj is wrong", 
        r2_equals(out.w, .1) && r2_equals(out.x, -.5) &&
        r2_equals(out.y, -.3) && r2_equals(out.z, -.2));
    // clang-format on
    return 0;
}

static const char *test_quat_mul_quat(void)
{
    vec3 v1 = {.x = 0.f, .y = M_PI / 2, .z = 0.f};
    quat q1 = {0};
    quat_from_euler(&v1, &q1);

    vec3 v2 = {.x = M_PI / 4, .y = 0.f, .z = 0.f};
    quat q2 = {0};
    quat_from_euler(&v2, &q2);

    quat q3 = {0};
    quat_mul_quat(&q2, &q1, &q3);

    // clang-format off
    r2_assert("quat mul quat", 
        r2_equals(q3.w, 0.653281) && r2_equals(q3.x, 0.270598) && 
        r2_equals(q3.y, 0.653281) && r2_equals(q3.z, 0.270598));
    // clang-format on
    return 0;
}

static const char *test_quat_mul_vec3_x_90z(void)
{
    // Rotate the X point 90 deg about the Z axis
    //  y
    //  |1
    //  |
    //  |   1
    //  ----->x
    vec3 v1 = {.x = 0.f, .y = 0.f, .z = M_PI / 2};
    quat q = {0};
    quat_from_euler(&v1, &q);

    vec3 v = {.x = 1.f, .y = 0.f, .z = 0.f};
    quat result = {0};

    quat_mul_vec3(&q, &v, &result);

    // clang-format off
    r2_assert("quat mul vec3 is wrong (x)", 
        r2_equals(result.x, 0.) && r2_equals(result.y, 1.) &&
        r2_equals(result.z, 0.) && r2_equals(result.w, 0.));
    // clang-format on
    return 0;
}

static const char *test_quat_mul_vec3_y_90x(void)
{
    // Rotate the Y point 90 deg about the X axis
    //   1
    //   y  1
    //   | /
    //   |/
    //   --->x
    //  /
    // z
    vec3 v1 = {.x = M_PI / 2, .y = 0.f, .z = 0.f};
    quat q = {0};
    quat_from_euler(&v1, &q);

    vec3 v = {.x = 0.f, .y = 1.f, .z = 0.f};
    quat result = {0};

    quat_mul_vec3(&q, &v, &result);

    r2_assert("quat mul vec3 is wrong (y 90x)", r2_equals(result.x, 0.) && r2_equals(result.y, 0.) &&
                                                    r2_equals(result.z, 1.) && r2_equals(result.w, 0.));
    return 0;
}

static const char *test_quat_mul_vec3_y_180x(void)
{
    vec3 v1 = {.x = M_PI, .y = 0.f, .z = 0.f};
    quat q = {0};
    quat_from_euler(&v1, &q);

    vec3 v = {.x = 0.f, .y = 1.f, .z = 0.f};
    quat result = {0};

    quat_mul_vec3(&q, &v, &result);

    r2_assert("quat mul vec3 is wrong (y 180x)", r2_equals(result.x, 0.) && r2_equals(result.y, -1.) &&
                                                     r2_equals(result.z, 0.) && r2_equals(result.w, 0.));
    return 0;
}

static const char *test_quat_mat4(void)
{
    quat q = {0};
    quat_identity(&q);

    mat4 result = {0};
    quat_mat4(&q, &result);

    // clang-format off
    r2_assert("quat mat4 is wrong",
    	r2_equals(result.m00, 1.) && 
    	r2_equals(result.m11, 1.) &&
        r2_equals(result.m22, 1.) && 
        r2_equals(result.m33, 1.)
    );
    // clang-format on
    return 0;
}

static const char *test_mat4_size(void)
{
    r2_assert("mat4 padding is wrong", sizeof(mat4) == 64);
    r2_assert("mat3 padding is wrong", sizeof(mat3) == 36);
    r2_assert("vec4 padding is wrong", sizeof(vec4) == 16);
    r2_assert("vec2 padding is wrong", sizeof(vec2) == 8);
    return 0;
}

static const char *test_mat4_transform(void)
{
    mat4 kern = {0};
    vec4 p = {0};
    vec4 r = {0};

    float ary[4] = {3., 3., 3., 0.};
    vec4_set(ary, &p);

    float ary2[16] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    mat4_set(ary2, &kern);

    mat4_transform(&p, &kern, &r);

    r2_assert("mat4 transform is wrong",
              r2_equals(r.x, 18.) && r2_equals(r.y, 18.) && r2_equals(r.z, 18.) && r2_equals(r.w, 18.));

    return 0;
}

static const char *test_mat4_lookat(void)
{
    mat4 view = {0};

    vec4 pos = {.x = 0, .y = 0, .z = 10};
    vec4 target = {.x = 0, .y = 0, .z = -1};
    vec4 up = {.x = 0, .y = 1, .z = 0};

    mat4_lookat(&pos, &target, &up, &view);
    // char buf[R2_MAT4_TOS_BUF_SIZE];
    // mat4_tos(&view, buf, sizeof(buf));
    // printf("%s", buf);
    return 0;
}

static const char *test_mat4_identity(void)
{
    mat4 out = {0};
    mat4_identity(&out);

    // clang-format off
    r2_assert("mat4 identity is wrong",
        r2_equals(out.m00, 1.) && r2_equals(out.m10, 0.) && r2_equals(out.m20, 0.) && r2_equals(out.m30, 0.) && 
        r2_equals(out.m01, 0.) && r2_equals(out.m11, 1.) && r2_equals(out.m21, 0.) && r2_equals(out.m31, 0.) && 
        r2_equals(out.m02, 0.) && r2_equals(out.m12, 0.) && r2_equals(out.m22, 1.) && r2_equals(out.m32, 0.) &&
        r2_equals(out.m03, 0.) && r2_equals(out.m13, 0.) && r2_equals(out.m23, 0.) && r2_equals(out.m33, 1.));
    // clang-format on
    return 0;
}

static const char *test_mat4_mul(void)
{
    mat4 k1 = {0};
    mat4 k2 = {0};
    mat4 out = {0};

    static const float k1mat[16] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    memcpy(k1.a_mat4, k1mat, sizeof(k1mat));

    static const float k1mat2[16] = {4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1};
    memcpy(k2.a_mat4, k1mat2, sizeof(k1mat2));

    mat4_mul(&k1, &k2, &out);

    // 10.000000 10.000000 10.000000 10.000000
    // 20.000000 20.000000 20.000000 20.000000
    // 30.000000 30.000000 30.000000 30.000000
    // 40.000000 40.000000 40.000000 40.000000
    
    // clang-format off
    r2_assert("mat4 mul is wrong",
        r2_equals(out.m00, 10.) && r2_equals(out.m10, 10.) && r2_equals(out.m20, 10.) && r2_equals(out.m30, 10.) && 
        r2_equals(out.m01, 20.) && r2_equals(out.m11, 20.) && r2_equals(out.m21, 20.) && r2_equals(out.m31, 20.) && 
        r2_equals(out.m02, 30.) && r2_equals(out.m12, 30.) && r2_equals(out.m22, 30.) && r2_equals(out.m32, 30.) &&
        r2_equals(out.m03, 40.) && r2_equals(out.m13, 40.) && r2_equals(out.m23, 40.) && r2_equals(out.m33, 40.));
    // clang-format on
    return 0;
}

static const char *test_mat4_mul2(void)
{
    mat4 k1 = {0};
    mat4 k2 = {0};
    // NOTE: must be zeroed
    mat4 out = {0};

    // M1 = [.9, .82, .1, 3; .39, 4, 1, .22; .20, 3, 0, 1; 0, 3, 1, .9];
    static const float k1mat[16] = {.9, .82, .1, 3, .39, 4, 1, .22, .20, 3, 0, 1, 0, 3, 1, .9};
    memcpy(k1.a_mat4, k1mat, sizeof(k1mat));
    // M2 = [3, 48, 21, 3; .32, .45, .22, .3; 3, 0, .4, .4; .312, .2, 3, 2];
    static const float k1mat2[16] = {3, 48, 21, 3, .32, .45, .22, .3, 3, 0, .4, .4, .312, .2, 3, 2};
    memcpy(k2.a_mat4, k1mat2, sizeof(k1mat2));

    mat4_mul(&k1, &k2, &out);

    // Expected values are the exact mathematical results for these inputs.
    // A slightly wider epsilon is used here because accumulated float rounding
    // differs by a few ULPs across platforms (x86+SSE3, ARM64, wasm, etc).
    // clang-format off
#define M2_EQ(a, b) (fabsf((a) - (b)) < 0.0001f)
    r2_assert("mat4 mul 2 is wrong",
        M2_EQ(out.m00, 4.1984f)  && M2_EQ(out.m10, 44.169f)  && M2_EQ(out.m20, 28.1204f) && M2_EQ(out.m30, 8.986f)
     && M2_EQ(out.m01, 5.51864f) && M2_EQ(out.m11, 20.564f)  && M2_EQ(out.m21, 10.13f)   && M2_EQ(out.m31, 3.21f)
     && M2_EQ(out.m02, 1.872f)   && M2_EQ(out.m12, 11.15f)   && M2_EQ(out.m22, 7.86f)    && M2_EQ(out.m32, 3.5f)
     && M2_EQ(out.m03, 4.2408f)  && M2_EQ(out.m13, 1.53f)    && M2_EQ(out.m23, 3.76f)    && M2_EQ(out.m33, 3.1f));
#undef M2_EQ
    // clang-format on
    return 0;
}

static const char *test_mat4_mul_speed(void)
{
    time_t ti;
    mat4 k1 = {0};
    mat4 k2 = {0};
    mat4 out = {0};

    static const float k1mat[16] = {.9, .82, .01, 3, .3, 4, 1, .22, .20, 3, 111.03, 1, 0, 3, 1, .9};
    memcpy(k1.a_mat4, k1mat, sizeof(k1mat));

    static const float k1mat2[16] = {3, 4.8, 21, 3, .32, .45, .22, .3, 3, 0.993, .4, .4, .312, .2, 3.99999, 2};
    memcpy(k2.a_mat4, k1mat2, sizeof(k1mat2));

    srand((unsigned)time(&ti));

    printf("Matrix Mul 4x4 10 run of 1000...\n");
    double time_taken = 0;
    clock_t t;

    for (int z = 0; z < 10; z++)
    {
        for (unsigned int x = 0; x < 10000; x++)
        {
            k1.m00 = rand();
            k1.m10 = rand();
            k1.m31 = rand();

            t = clock();
            mat4_mul(&k1, &k2, &out);
            time_taken += ((double)clock() - t) / CLOCKS_PER_SEC;
        }
        ////////////////////////
        printf("Run %d; Time %f sec\n", z, time_taken);
        // printf("%f\n", time_taken);
        time_taken = 0;
    }

    return 0;
}

static const char *test_mat3_mul(void)
{
    mat3 k1 = {0};
    mat3 k2 = {0};
    mat3 out = {0};

    // clang-format off
    static const float k1mat[9] = {
        1, 0, 300,
        0, 2, 300,
        0, 0, 1
    };
    // clang-format on
    memcpy(k1.a_mat3, k1mat, sizeof(k1mat));

    // clang-format off
    static const float k1mat2[9] = {
        -1.836970198, 1, 0,
        -1, -1.836970198, 0,
        0, 0, 1
    };
    // clang-format on

    memcpy(k2.a_mat3, k1mat2, sizeof(k1mat2));

    // printf("Matrix Mul 3x3 run...\n");
    mat3_mul(&k1, &k2, &out);
    // char buf[R2_MAT3_TOS_BUF_SIZE];
    // mat3_tos(&out, buf, sizeof(buf));
    // printf("%s", buf);

    // -1.836970198, 1, 300,
    // -2, -3.67394039, 300,
    // 0, 0, 1
    r2_assert("mat3 mul is wrong", r2_equals(out.m00, -1.836970) && r2_equals(out.m10, 1.) &&
                                       r2_equals(out.m20, 300.) && r2_equals(out.m01, -2.) &&
                                       r2_equals(out.m11, -3.673940) && r2_equals(out.m21, 300.) &&
                                       r2_equals(out.m02, 0.) && r2_equals(out.m12, 0.) && r2_equals(out.m22, 1.));
    return 0;
}

static const char *test_mat3_identity(void)
{
    mat3 out = {0};
    mat3_identity(&out);
    // clang-format off
    r2_assert("mat3 identity is wrong",
        r2_equals(out.m00, 1.) && r2_equals(out.m10, 0.) && r2_equals(out.m20, 0.) && 
        r2_equals(out.m01, 0.) && r2_equals(out.m11, 1.) && r2_equals(out.m21, 0.) && 
        r2_equals(out.m02, 0.) && r2_equals(out.m12, 0.) && r2_equals(out.m22, 1.));
    // clang-format on
    return 0;
}

static const char *test_mat_mul(void)
{
    mat3 k1 = {0};
    mat3 k2 = {0};
    mat3 out = {0};

    // clang-format off
    static const float k1mat[9] = {
        1, 0, 300,
        0, 2, 300,
        0, 0, 1
    };
    // clang-format on
    memcpy(k1.a_mat3, k1mat, sizeof(float) * 9);

    // clang-format off
    static const float k1mat2[9] = {
        -1.836970198, 1, 0,
        -1, -1.836970198, 0,
        0, 0, 1
    };
    // clang-format on
    memcpy(k2.a_mat3, k1mat2, sizeof(float) * 9);

    static const float zero[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(out.a_mat3, zero, sizeof(float) * 9);

    // printf("Matrix Generic Mul 3x3 run...\n");
    mat_mul(k1.a_mat3, k2.a_mat3, 3, 3, 3, 3, out.a_mat3);

    // char buf[R2_MAT3_TOS_BUF_SIZE];
    // mat3_tos(&out, buf, sizeof(buf));
    // printf("%s", buf);

    // clang-format off
    r2_assert("mat3 mul is wrong", 
        r2_equals(out.m00, -1.836970) && r2_equals(out.m10, 1.)        && r2_equals(out.m20, 300.) && 
        r2_equals(out.m01, -2.)       && r2_equals(out.m11, -3.673940) && r2_equals(out.m21, 300.) &&
        r2_equals(out.m02, 0.)        && r2_equals(out.m12, 0.)        && r2_equals(out.m22, 1.));
    // clang-format on
    return 0;
}

static const char *test_mat4_mul_identity(void)
{
    mat4 ident = {0};
    mat4 k = {0};
    mat4 out = {0};

    mat4_identity(&ident);

    static const float kmat[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    memcpy(k.a_mat4, kmat, sizeof(kmat));

    // I * A = A
    mat4_mul(&ident, &k, &out);
    // clang-format off
    r2_assert("mat4 mul left identity is wrong",
        r2_equals(out.m00, k.m00) && r2_equals(out.m10, k.m10) && r2_equals(out.m20, k.m20) && r2_equals(out.m30, k.m30) &&
        r2_equals(out.m01, k.m01) && r2_equals(out.m11, k.m11) && r2_equals(out.m21, k.m21) && r2_equals(out.m31, k.m31) &&
        r2_equals(out.m02, k.m02) && r2_equals(out.m12, k.m12) && r2_equals(out.m22, k.m22) && r2_equals(out.m32, k.m32) &&
        r2_equals(out.m03, k.m03) && r2_equals(out.m13, k.m13) && r2_equals(out.m23, k.m23) && r2_equals(out.m33, k.m33));
    // clang-format on

    // A * I = A
    mat4 out2 = {0};
    mat4_mul(&k, &ident, &out2);
    // clang-format off
    r2_assert("mat4 mul right identity is wrong",
        r2_equals(out2.m00, k.m00) && r2_equals(out2.m10, k.m10) && r2_equals(out2.m20, k.m20) && r2_equals(out2.m30, k.m30) &&
        r2_equals(out2.m01, k.m01) && r2_equals(out2.m11, k.m11) && r2_equals(out2.m21, k.m21) && r2_equals(out2.m31, k.m31) &&
        r2_equals(out2.m02, k.m02) && r2_equals(out2.m12, k.m12) && r2_equals(out2.m22, k.m22) && r2_equals(out2.m32, k.m32) &&
        r2_equals(out2.m03, k.m03) && r2_equals(out2.m13, k.m13) && r2_equals(out2.m23, k.m23) && r2_equals(out2.m33, k.m33));
    // clang-format on
    return 0;
}

static const char *test_mat4_mul_not_commutative(void)
{
    mat4 a = {0};
    mat4 b = {0};
    mat4 ab = {0};
    mat4 ba = {0};

    // A has a shear in the first row
    static const float amat[16] = {2, 0, 0, 0,  1, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
    memcpy(a.a_mat4, amat, sizeof(amat));
    // B has a shear in the second row
    static const float bmat[16] = {1, 2, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
    memcpy(b.a_mat4, bmat, sizeof(bmat));

    mat4_mul(&a, &b, &ab);
    mat4_mul(&b, &a, &ba);

    // A*B: m00=2 m10=4 m11=3
    // B*A: m00=4 m10=2 m11=1
    r2_assert("mat4 mul should not be commutative",
              !r2_equals(ab.m00, ba.m00) && !r2_equals(ab.m10, ba.m10) && !r2_equals(ab.m11, ba.m11));
    return 0;
}

static const char *test_mat4_transpose(void)
{
    mat4 k = {0};
    mat4 out = {0};

    static const float kmat[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    memcpy(k.a_mat4, kmat, sizeof(kmat));

    mat4_transpose(&k, &out);

    // clang-format off
    r2_assert("mat4 transpose is wrong",
        r2_equals(out.m00,  1.) && r2_equals(out.m10,  5.) && r2_equals(out.m20,  9.) && r2_equals(out.m30, 13.) &&
        r2_equals(out.m01,  2.) && r2_equals(out.m11,  6.) && r2_equals(out.m21, 10.) && r2_equals(out.m31, 14.) &&
        r2_equals(out.m02,  3.) && r2_equals(out.m12,  7.) && r2_equals(out.m22, 11.) && r2_equals(out.m32, 15.) &&
        r2_equals(out.m03,  4.) && r2_equals(out.m13,  8.) && r2_equals(out.m23, 12.) && r2_equals(out.m33, 16.));
    // clang-format on
    return 0;
}

static const char *test_mat4_transpose_twice(void)
{
    mat4 k = {0};
    mat4 tmp = {0};
    mat4 out = {0};

    static const float kmat[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    memcpy(k.a_mat4, kmat, sizeof(kmat));

    // (A^T)^T = A
    mat4_transpose(&k, &tmp);
    mat4_transpose(&tmp, &out);

    // clang-format off
    r2_assert("mat4 transpose twice is wrong",
        r2_equals(out.m00, k.m00) && r2_equals(out.m10, k.m10) && r2_equals(out.m20, k.m20) && r2_equals(out.m30, k.m30) &&
        r2_equals(out.m01, k.m01) && r2_equals(out.m11, k.m11) && r2_equals(out.m21, k.m21) && r2_equals(out.m31, k.m31) &&
        r2_equals(out.m02, k.m02) && r2_equals(out.m12, k.m12) && r2_equals(out.m22, k.m22) && r2_equals(out.m32, k.m32) &&
        r2_equals(out.m03, k.m03) && r2_equals(out.m13, k.m13) && r2_equals(out.m23, k.m23) && r2_equals(out.m33, k.m33));
    // clang-format on
    return 0;
}

static const char *test_mat3_mul_identity(void)
{
    mat3 ident = {0};
    mat3 k = {0};
    mat3 out = {0};

    mat3_identity(&ident);

    static const float kmat[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    memcpy(k.a_mat3, kmat, sizeof(kmat));

    // I * A = A
    mat3_mul(&ident, &k, &out);
    // clang-format off
    r2_assert("mat3 mul left identity is wrong",
        r2_equals(out.m00, k.m00) && r2_equals(out.m10, k.m10) && r2_equals(out.m20, k.m20) &&
        r2_equals(out.m01, k.m01) && r2_equals(out.m11, k.m11) && r2_equals(out.m21, k.m21) &&
        r2_equals(out.m02, k.m02) && r2_equals(out.m12, k.m12) && r2_equals(out.m22, k.m22));
    // clang-format on

    // A * I = A
    mat3 out2 = {0};
    mat3_mul(&k, &ident, &out2);
    // clang-format off
    r2_assert("mat3 mul right identity is wrong",
        r2_equals(out2.m00, k.m00) && r2_equals(out2.m10, k.m10) && r2_equals(out2.m20, k.m20) &&
        r2_equals(out2.m01, k.m01) && r2_equals(out2.m11, k.m11) && r2_equals(out2.m21, k.m21) &&
        r2_equals(out2.m02, k.m02) && r2_equals(out2.m12, k.m12) && r2_equals(out2.m22, k.m22));
    // clang-format on
    return 0;
}

static const char *test_mat3_mul_not_commutative(void)
{
    mat3 a = {0};
    mat3 b = {0};
    mat3 ab = {0};
    mat3 ba = {0};

    static const float amat[9] = {2, 1, 0,  0, 1, 0,  0, 0, 1};
    memcpy(a.a_mat3, amat, sizeof(amat));
    static const float bmat[9] = {1, 0, 0,  2, 1, 0,  0, 0, 1};
    memcpy(b.a_mat3, bmat, sizeof(bmat));

    mat3_mul(&a, &b, &ab);
    mat3_mul(&b, &a, &ba);

    // A*B: m00=4, m10=1
    // B*A: m00=2, m10=1 (m11 differs: AB=3, BA=1)
    r2_assert("mat3 mul should not be commutative",
              !r2_equals(ab.m00, ba.m00) && !r2_equals(ab.m11, ba.m11));
    return 0;
}

static const char *test_mat_mul_non_square(void)
{
    // 2x3 * 3x2 = 2x2
    static const float a[6]  = {1, 2, 3,  4, 5, 6};
    static const float b[6]  = {7, 8,  9, 10,  11, 12};
    float out[4] = {0};

    mat_mul(a, b, 2, 3, 3, 2, out);

    // [1,2,3] . [7,9,11]  = 7+18+33 = 58
    // [1,2,3] . [8,10,12] = 8+20+36 = 64
    // [4,5,6] . [7,9,11]  = 28+45+66 = 139
    // [4,5,6] . [8,10,12] = 32+50+72 = 154
    r2_assert("mat_mul non-square is wrong",
              r2_equals(out[0], 58.f) && r2_equals(out[1], 64.f) &&
              r2_equals(out[2], 139.f) && r2_equals(out[3], 154.f));
    return 0;
}

static const char *test_vecn_add(void)
{
    float a[4] = {1.f, 2.f, 3.f, 4.f};
    float b[4] = {10.f, 20.f, 30.f, 40.f};
    float out[4] = {0};
    vecn_add(a, b, 4, out);
    r2_assert("vecn_add is wrong",
        r2_equals(out[0], 11.f) && r2_equals(out[1], 22.f) &&
        r2_equals(out[2], 33.f) && r2_equals(out[3], 44.f));
    return 0;
}

static const char *test_vecn_sub(void)
{
    float a[3] = {5.f, 3.f, 1.f};
    float b[3] = {1.f, 2.f, 3.f};
    float out[3] = {0};
    vecn_sub(a, b, 3, out);
    r2_assert("vecn_sub is wrong",
        r2_equals(out[0], 4.f) && r2_equals(out[1], 1.f) && r2_equals(out[2], -2.f));
    return 0;
}

static const char *test_vecn_mul(void)
{
    float a[3] = {1.f, 2.f, 3.f};
    float out[3] = {0};
    vecn_mul(a, 3.f, 3, out);
    r2_assert("vecn_mul is wrong",
        r2_equals(out[0], 3.f) && r2_equals(out[1], 6.f) && r2_equals(out[2], 9.f));
    return 0;
}

static const char *test_vecn_mul_vec(void)
{
    float a[3] = {2.f, 3.f, 4.f};
    float b[3] = {5.f, 6.f, 7.f};
    float out[3] = {0};
    vecn_mul_vec(a, b, 3, out);
    r2_assert("vecn_mul_vec is wrong",
        r2_equals(out[0], 10.f) && r2_equals(out[1], 18.f) && r2_equals(out[2], 28.f));
    return 0;
}

static const char *test_vecn_div(void)
{
    float a[4] = {4.f, 8.f, 12.f, 16.f};
    float out[4] = {0};
    vecn_div(a, 4.f, 4, out);
    r2_assert("vecn_div is wrong",
        r2_equals(out[0], 1.f) && r2_equals(out[1], 2.f) &&
        r2_equals(out[2], 3.f) && r2_equals(out[3], 4.f));
    return 0;
}

static const char *test_vecn_div_zero(void)
{
    float a[2] = {6.f, 8.f};
    float out[2] = {0};
    vecn_div(a, 0.f, 2, out);
    r2_assert("vecn_div zero is wrong",
        r2_equals(out[0], 6.f) && r2_equals(out[1], 8.f));
    return 0;
}

static const char *test_vecn_div_vec(void)
{
    float a[3] = {6.f, 9.f, 12.f};
    float b[3] = {2.f, 3.f, 4.f};
    float out[3] = {0};
    vecn_div_vec(a, b, 3, out);
    r2_assert("vecn_div_vec is wrong",
        r2_equals(out[0], 3.f) && r2_equals(out[1], 3.f) && r2_equals(out[2], 3.f));
    return 0;
}

static const char *test_vecn_dot(void)
{
    float a[3] = {1.f, 2.f, 3.f};
    float b[3] = {4.f, 5.f, 6.f};
    // 1*4 + 2*5 + 3*6 = 4+10+18 = 32
    r2_assert("vecn_dot is wrong", r2_equals(vecn_dot(a, b, 3), 32.f));
    return 0;
}

static const char *test_vecn_length(void)
{
    // 3-4-5 right triangle in first two components
    float a[4] = {3.f, 4.f, 0.f, 0.f};
    r2_assert("vecn_length 2d is wrong", r2_equals(vecn_length(a, 2), 5.f));
    r2_assert("vecn_length 4d is wrong", r2_equals(vecn_length(a, 4), 5.f));
    return 0;
}

static const char *test_vecn_dist(void)
{
    float a[3] = {0.f, 0.f, 0.f};
    float b[3] = {0.f, 3.f, 4.f};
    // sqrt(0+9+16) = 5
    r2_assert("vecn_dist is wrong", r2_equals(vecn_dist(a, b, 3), 5.f));
    return 0;
}

static const char *test_vecn_normalize(void)
{
    float a[3] = {10.f, 0.f, 0.f};
    float out[3] = {0};
    vecn_normalize(a, 3, out);
    r2_assert("vecn_normalize is wrong",
        r2_equals(out[0], 1.f) && r2_equals(out[1], 0.f) && r2_equals(out[2], 0.f));
    return 0;
}

static const char *test_vecn_normalize_zero(void)
{
    float a[4] = {0.f, 0.f, 0.f, 0.f};
    float out[4] = {9.f, 9.f, 9.f, 9.f};  // pre-fill to check all are zeroed
    vecn_normalize(a, 4, out);
    r2_assert("vecn_normalize zero is wrong",
        r2_equals(out[0], 0.f) && r2_equals(out[1], 0.f) &&
        r2_equals(out[2], 0.f) && r2_equals(out[3], 0.f));
    return 0;
}

static const char *test_vecn_abs(void)
{
    float a[4] = {-1.f, 2.f, -3.f, 4.f};
    float out[4] = {0};
    vecn_abs(a, 4, out);
    r2_assert("vecn_abs is wrong",
        r2_equals(out[0], 1.f) && r2_equals(out[1], 2.f) &&
        r2_equals(out[2], 3.f) && r2_equals(out[3], 4.f));
    return 0;
}

static const char *test_vecn_pow(void)
{
    float a[3] = {2.f, 3.f, 4.f};
    float out[3] = {0};
    vecn_pow(a, 2.f, 3, out);
    r2_assert("vecn_pow is wrong",
        r2_equals(out[0], 4.f) && r2_equals(out[1], 9.f) && r2_equals(out[2], 16.f));
    return 0;
}

static const char *test_vecn_sqrt(void)
{
    float a[3] = {4.f, 9.f, 16.f};
    float out[3] = {0};
    vecn_sqrt(a, 3, out);
    r2_assert("vecn_sqrt is wrong",
        r2_equals(out[0], 2.f) && r2_equals(out[1], 3.f) && r2_equals(out[2], 4.f));
    return 0;
}

static const char *test_vecn_arbitrary_n(void)
{
    // Proves generality beyond vec2/3/4 with n=5
    float a[5] = {1.f, 2.f, 3.f, 4.f, 5.f};
    float b[5] = {5.f, 4.f, 3.f, 2.f, 1.f};
    float out[5] = {0};
    vecn_add(a, b, 5, out);
    r2_assert("vecn_add n=5 is wrong",
        r2_equals(out[0], 6.f) && r2_equals(out[1], 6.f) && r2_equals(out[2], 6.f) &&
        r2_equals(out[3], 6.f) && r2_equals(out[4], 6.f));
    // 1*5 + 2*4 + 3*3 + 4*2 + 5*1 = 5+8+9+8+5 = 35
    r2_assert("vecn_dot n=5 is wrong", r2_equals(vecn_dot(a, b, 5), 35.f));
    return 0;
}

// Large-n tests that exercise the BLAS code paths (n=256 ensures BLAS overhead
// is warranted and confirms correctness at scale with and without BLAS).
static const char *test_vecn_dot_large(void)
{
    // a = {1,1,...} b = {2,2,...} -> dot = 256*2 = 512
    float a[256], b[256];
    int i;
    for (i = 0; i < 256; i++) { a[i] = 1.f; b[i] = 2.f; }
    r2_assert("vecn_dot large is wrong", r2_equals(vecn_dot(a, b, 256), 512.f));
    return 0;
}

static const char *test_vecn_length_large(void)
{
    // v = {1,0,0,...} -> length = 1
    float v[256] = {0};
    v[0] = 1.f;
    r2_assert("vecn_length large is wrong", r2_equals(vecn_length(v, 256), 1.f));
    // v = {3,4,0,...} -> length = 5
    v[0] = 3.f; v[1] = 4.f;
    r2_assert("vecn_length large 3-4-5 is wrong", r2_equals(vecn_length(v, 256), 5.f));
    return 0;
}

static const char *test_vecn_mul_large(void)
{
    // a = {2,2,...} * 3 -> {6,6,...}
    float a[256], out[256];
    int i;
    for (i = 0; i < 256; i++) a[i] = 2.f;
    vecn_mul(a, 3.f, 256, out);
    int ok = 1;
    for (i = 0; i < 256; i++) ok &= r2_equals(out[i], 6.f);
    r2_assert("vecn_mul large is wrong", ok);
    return 0;
}

static const char *test_vecn_div_large(void)
{
    // a = {6,6,...} / 2 -> {3,3,...}
    float a[256], out[256];
    int i;
    for (i = 0; i < 256; i++) a[i] = 6.f;
    vecn_div(a, 2.f, 256, out);
    int ok = 1;
    for (i = 0; i < 256; i++) ok &= r2_equals(out[i], 3.f);
    r2_assert("vecn_div large is wrong", ok);
    return 0;
}

static const char *test_vec4_tos(void)
{
    quat q = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    char buf[R2_VEC4_TOS_BUF_SIZE];
    vec4_tos(&q, buf, sizeof(buf));
    r2_assert("vec4_tos produced empty string", strlen(buf) > 0);
    return 0;
}

static const char *test_vec4_tos_truncates_safely(void)
{
    quat q = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    char buf[4];
    vec4_tos(&q, buf, sizeof(buf));
    r2_assert("vec4_tos did not null-terminate an undersized buffer", buf[sizeof(buf) - 1] == '\0');
    return 0;
}

static const char *test_quat_tos(void)
{
    quat q = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    char buf[R2_QUAT_TOS_BUF_SIZE];
    quat_tos(&q, buf, sizeof(buf));
    r2_assert("quat_tos produced empty string", strlen(buf) > 0);
    return 0;
}

static const char *test_mat3_tos(void)
{
    mat3 m = {0};
    mat3_identity(&m);
    char buf[R2_MAT3_TOS_BUF_SIZE];
    mat3_tos(&m, buf, sizeof(buf));
    r2_assert("mat3_tos produced empty string", strlen(buf) > 0);
    return 0;
}

static const char *test_mat4_tos(void)
{
    mat4 m = {0};
    mat4_identity(&m);
    char buf[R2_MAT4_TOS_BUF_SIZE];
    mat4_tos(&m, buf, sizeof(buf));
    r2_assert("mat4_tos produced empty string", strlen(buf) > 0);
    return 0;
}

///////////////////////////////////////////////////////////////
// Helpers for the tests below

static bool m_near(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

/* Deterministic xorshift, returns a float in [-1, 1] */
static uint32_t m_seed = 0x12345678u;
static float m_rand(void)
{
    m_seed ^= m_seed << 13;
    m_seed ^= m_seed >> 17;
    m_seed ^= m_seed << 5;
    return (float)(m_seed & 0xFFFFFF) / (float)0x7FFFFF - 1.f;
}

/* out = M * p, treating a_mat4 as row-major with column vectors (the
   layout mat4_perspective and mat4_lookat produce) */
static void m_apply(const mat4 *m, const vec4 *p, vec4 *out)
{
    mat4 t;
    mat4_transpose(m, &t);
    mat4_transform(p, &t, out);
}

static bool m_vec4_near(const vec4 *a, float x, float y, float z, float w, float tol)
{
    return m_near(a->x, x, tol) && m_near(a->y, y, tol) && m_near(a->z, z, tol) && m_near(a->w, w, tol);
}

///////////////////////////////////////////////////////////////
// Fns

static const char *test_deg_to_rad(void)
{
    r2_assert("deg_to_rad 0 is wrong", r2_equals(deg_to_rad(0.f), 0.f));
    r2_assert("deg_to_rad 180 is wrong", m_near(deg_to_rad(180.f), (float)M_PI, 1e-6f));
    r2_assert("deg_to_rad -90 is wrong", m_near(deg_to_rad(-90.f), (float)(-M_PI / 2), 1e-6f));
    return 0;
}

static const char *test_r2_equals_epsilon(void)
{
    r2_assert("r2_equals same", r2_equals(1.f, 1.f));
    r2_assert("r2_equals inside eps", r2_equals(1.f, 1.f + 0.0000005f));
    r2_assert("r2_equals outside eps", !r2_equals(1.f, 1.00001f));
    r2_assert("r2_equals NaN", !r2_equals(NAN, NAN));
    return 0;
}

///////////////////////////////////////////////////////////////
// Vec2 (the remaining functions)

static const char *test_vec2_zero_set_equals(void)
{
    vec2 a = {.x = 5.f, .y = 6.f};
    vec2 b = {0};
    vec2_zero(&a);
    r2_assert("vec2 zero is wrong", a.x == 0.f && a.y == 0.f);
    vec2_set(1.f, -2.f, &a);
    r2_assert("vec2 set is wrong", a.x == 1.f && a.y == -2.f);
    vec2_set(1.f, -2.f, &b);
    r2_assert("vec2 equals same is wrong", vec2_equals(&a, &b));
    b.y = -2.1f;
    r2_assert("vec2 equals diff is wrong", !vec2_equals(&a, &b));
    return 0;
}

static const char *test_vec2_mul(void)
{
    vec2 a = {.x = 2.f, .y = -3.f};
    vec2 b = {.x = 4.f, .y = 5.f};
    vec2 out = {0};
    vec2_mul(&a, 2.5f, &out);
    r2_assert("vec2 mul is wrong", out.x == 5.f && out.y == -7.5f);
    vec2_mul_vec2(&a, &b, &out);
    r2_assert("vec2 mul_vec2 is wrong", out.x == 8.f && out.y == -15.f);
    vec2_pow(&b, 2.f, &out);
    r2_assert("vec2 pow is wrong", r2_equals(out.x, 16.f) && r2_equals(out.y, 25.f));
    return 0;
}

static const char *test_vec2_length_dist(void)
{
    vec2 a = {.x = 3.f, .y = 4.f};
    vec2 b = {.x = 6.f, .y = 8.f};
    r2_assert("vec2 length_sqrd is wrong", vec2_length_sqrd(&a) == 25.f);
    r2_assert("vec2 length is wrong", vec2_length(&a) == 5.f);
    r2_assert("vec2 dist_sqrd is wrong", vec2_dist_sqrd(&a, &b) == 25.f);
    r2_assert("vec2 dist is wrong", vec2_dist(&a, &b) == 5.f);
    r2_assert("vec2 dist symmetric is wrong", vec2_dist(&b, &a) == 5.f);
    return 0;
}

static const char *test_vec2_normalize(void)
{
    vec2 a = {.x = 3.f, .y = 4.f};
    vec2 z = {0};
    vec2 out = {.x = 9.f, .y = 9.f};
    vec2_normalize(&a, &out);
    r2_assert("vec2 normalize is wrong", r2_equals(out.x, 0.6f) && r2_equals(out.y, 0.8f));
    vec2_normalize(&z, &out);
    r2_assert("vec2 normalize zero is wrong", out.x == 0.f && out.y == 0.f);
    return 0;
}

static const char *test_vec2_edge_cross(void)
{
    /* Twice the signed area of abc: positive counter-clockwise */
    vec2 a = {.x = 0.f, .y = 0.f};
    vec2 b = {.x = 4.f, .y = 0.f};
    vec2 c = {.x = 0.f, .y = 3.f};
    r2_assert("edge_cross ccw is wrong", vec2_edge_cross(&a, &b, &c) == 12.f);
    r2_assert("edge_cross cw is wrong", vec2_edge_cross(&a, &c, &b) == -12.f);
    r2_assert("edge_cross collinear is wrong", vec2_edge_cross(&a, &b, &b) == 0.f);
    return 0;
}

static const char *test_vec2_to_array(void)
{
    vec2 a = {.x = 1.5f, .y = -2.5f};
    float out[2] = {0};
    vec2_to_array(&a, out);
    r2_assert("vec2 to_array is wrong", out[0] == 1.5f && out[1] == -2.5f);
    return 0;
}

///////////////////////////////////////////////////////////////
// Vec3 (the remaining functions)

static const char *test_vec3_zero_set(void)
{
    vec3 a = {.x = 5.f, .y = 6.f, .z = 7.f};
    vec3_zero(&a);
    r2_assert("vec3 zero is wrong", a.x == 0.f && a.y == 0.f && a.z == 0.f);
    vec3_set(1.f, 2.f, 3.f, &a);
    r2_assert("vec3 set is wrong", a.x == 1.f && a.y == 2.f && a.z == 3.f);
    return 0;
}

static const char *test_vec3_sub_div_mul(void)
{
    vec3 a = {.x = 6.f, .y = -4.f, .z = 2.f};
    vec3 b = {.x = 2.f, .y = 2.f, .z = 0.f};
    vec3 out = {0};
    vec3_sub(&a, &b, &out);
    r2_assert("vec3 sub is wrong", out.x == 4.f && out.y == -6.f && out.z == 2.f);
    vec3_div(&a, 2.f, &out);
    r2_assert("vec3 div is wrong", out.x == 3.f && out.y == -2.f && out.z == 1.f);
    vec3_div(&a, 0.f, &out);
    r2_assert("vec3 div zero is wrong", out.x == 6.f && out.y == -4.f && out.z == 2.f);
    vec3_div_vec3(&a, &b, &out);
    r2_assert("vec3 div_vec3 (zero lane) is wrong", out.x == 3.f && out.y == -2.f && out.z == 2.f);
    vec3_mul_vec3(&a, &b, &out);
    r2_assert("vec3 mul_vec3 is wrong", out.x == 12.f && out.y == -8.f && out.z == 0.f);
    vec3_pow(&b, 3.f, &out);
    r2_assert("vec3 pow is wrong", r2_equals(out.x, 8.f) && r2_equals(out.y, 8.f) && out.z == 0.f);
    return 0;
}

static const char *test_vec3_dot_length_dist(void)
{
    vec3 a = {.x = 1.f, .y = 2.f, .z = 2.f};
    vec3 b = {.x = 4.f, .y = 6.f, .z = 2.f};
    r2_assert("vec3 dot is wrong", vec3_dot(&a, &b) == 20.f);
    r2_assert("vec3 length_sqrd is wrong", vec3_length_sqrd(&a) == 9.f);
    r2_assert("vec3 dist_sqrd is wrong", vec3_dist_sqrd(&a, &b) == 25.f);
    r2_assert("vec3 dist is wrong", vec3_dist(&a, &b) == 5.f);
    return 0;
}

static const char *test_vec3_ignores_w(void)
{
    /* vec3 shares storage with vec4; the vec3_ functions must not let a
       stray w leak into the result */
    vec3 a = {.x = 1.f, .y = 2.f, .z = 2.f, .w = 100.f};
    vec3 b = {.x = 1.f, .y = 2.f, .z = 2.f, .w = -50.f};
    r2_assert("vec3 length used w", vec3_length(&a) == 3.f);
    r2_assert("vec3 dot used w", vec3_dot(&a, &b) == 9.f);
    r2_assert("vec3 equals used w", vec3_equals(&a, &b));
    r2_assert("vec3 dist used w", vec3_dist(&a, &b) == 0.f);
    return 0;
}

static const char *test_vec3_cross_properties(void)
{
    vec3 x = {.x = 1.f}, y = {.y = 1.f}, z = {0};
    vec3 a, b, axb, bxa;
    vec3_cross(&x, &y, &z);
    r2_assert("cross x*y should be z (right handed)", z.x == 0.f && z.y == 0.f && z.z == 1.f);
    for (int i = 0; i < 100; i++)
    {
        vec3_set(m_rand(), m_rand(), m_rand(), &a);
        vec3_set(m_rand(), m_rand(), m_rand(), &b);
        vec3_cross(&a, &b, &axb);
        vec3_cross(&b, &a, &bxa);
        r2_assert("cross not orthogonal to a", m_near(vec3_dot(&axb, &a), 0.f, 1e-5f));
        r2_assert("cross not orthogonal to b", m_near(vec3_dot(&axb, &b), 0.f, 1e-5f));
        /* not exact: the compiler may fuse a*b - c*d into an fma */
        r2_assert("cross not anti-commutative",
                  m_near(axb.x, -bxa.x, 1e-6f) && m_near(axb.y, -bxa.y, 1e-6f) && m_near(axb.z, -bxa.z, 1e-6f));
    }
    return 0;
}

static const char *test_vec3_normalize_zero(void)
{
    vec3 z = {0};
    vec3 out = {.x = 9.f, .y = 9.f, .z = 9.f};
    vec3_normalize(&z, &out);
    r2_assert("vec3 normalize zero is wrong", out.x == 0.f && out.y == 0.f && out.z == 0.f);
    return 0;
}

///////////////////////////////////////////////////////////////
// Vec4 (the remaining functions)

static const char *test_vec4_zero_set_equals(void)
{
    float ary[4] = {1.f, 2.f, 3.f, 4.f};
    vec4 a = {0}, b = {0};
    vec4_set(ary, &a);
    r2_assert("vec4 set is wrong", a.x == 1.f && a.y == 2.f && a.z == 3.f && a.w == 4.f);
    vec4_set(ary, &b);
    r2_assert("vec4 equals same is wrong", vec4_equals(&a, &b));
    b.w = 4.01f;
    r2_assert("vec4 equals diff w is wrong", !vec4_equals(&a, &b));
    vec4_zero(&a);
    r2_assert("vec4 zero is wrong", a.x == 0.f && a.y == 0.f && a.z == 0.f && a.w == 0.f);
    return 0;
}

static const char *test_vec4_add_sub_mul(void)
{
    vec4 a = {.x = 1.f, .y = -2.f, .z = 3.f, .w = -4.f};
    vec4 b = {.x = 2.f, .y = 2.f, .z = 2.f, .w = 2.f};
    vec4 out = {0};
    vec4_add(&a, &b, &out);
    r2_assert("vec4 add is wrong", m_vec4_near(&out, 3.f, 0.f, 5.f, -2.f, 0.f));
    vec4_sub(&a, &b, &out);
    r2_assert("vec4 sub is wrong", m_vec4_near(&out, -1.f, -4.f, 1.f, -6.f, 0.f));
    vec4_mul(&a, -1.f, &out);
    r2_assert("vec4 mul is wrong", m_vec4_near(&out, -1.f, 2.f, -3.f, 4.f, 0.f));
    vec4_mul_vec4(&a, &b, &out);
    r2_assert("vec4 mul_vec4 is wrong", m_vec4_near(&out, 2.f, -4.f, 6.f, -8.f, 0.f));
    vec4_abs(&a, &out);
    r2_assert("vec4 abs is wrong", m_vec4_near(&out, 1.f, 2.f, 3.f, 4.f, 0.f));
    vec4_pow(&b, 3.f, &out);
    r2_assert("vec4 pow is wrong", m_vec4_near(&out, 8.f, 8.f, 8.f, 8.f, 1e-5f));
    return 0;
}

static const char *test_vec4_sqrt_dot_length(void)
{
    vec4 a = {.x = 4.f, .y = 9.f, .z = 16.f, .w = 25.f};
    vec4 b = {.x = 1.f, .y = 1.f, .z = 1.f, .w = 1.f};
    vec4 out = {0};
    vec4_sqrt(&a, &out);
    r2_assert("vec4 sqrt is wrong", m_vec4_near(&out, 2.f, 3.f, 4.f, 5.f, 0.f));
    r2_assert("vec4 dot is wrong", vec4_dot(&a, &b) == 54.f);
    r2_assert("vec4 length_sqrd is wrong", vec4_length_sqrd(&b) == 4.f);
    r2_assert("vec4 length is wrong", vec4_length(&b) == 2.f);
    return 0;
}

static const char *test_vec4_normalize_random(void)
{
    vec4 a, out;
    for (int i = 0; i < 100; i++)
    {
        a.x = m_rand() * 100.f;
        a.y = m_rand() * 100.f;
        a.z = m_rand() * 100.f;
        a.w = m_rand() * 100.f;
        vec4_normalize(&a, &out);
        r2_assert("vec4 normalize not unit length", m_near(vec4_length(&out), 1.f, 1e-5f));
        r2_assert("vec4 normalize changed direction", vec4_dot(&a, &out) > 0.f);
    }
    return 0;
}

static const char *test_vecn_equals_zero_n(void)
{
    float a[1] = {1.f}, b[1] = {2.f};
    r2_assert("vecn equals n=0 should be true", vecn_equals(a, b, 0));
    r2_assert("vecn dot n=0 should be 0", vecn_dot(a, b, 0) == 0.f);
    return 0;
}

///////////////////////////////////////////////////////////////
// Quat (the remaining functions, plus algebraic properties)

static const char *test_quat_zero_identity(void)
{
    quat q = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    quat_zero(&q);
    r2_assert("quat zero is wrong", m_vec4_near(&q, 0.f, 0.f, 0.f, 0.f, 0.f));
    quat_identity(&q);
    r2_assert("quat identity is wrong", m_vec4_near(&q, 0.f, 0.f, 0.f, 1.f, 0.f));
    return 0;
}

static const char *test_quat_add_sub_dot_magnitude(void)
{
    quat a = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    quat b = {.x = 1.f, .y = 1.f, .z = 1.f, .w = 1.f};
    quat out = {0};
    quat_add(&a, &b, &out);
    r2_assert("quat add is wrong", m_vec4_near(&out, 2.f, 3.f, 4.f, 5.f, 0.f));
    quat_sub(&a, &b, &out);
    r2_assert("quat sub is wrong", m_vec4_near(&out, 0.f, 1.f, 2.f, 3.f, 0.f));
    r2_assert("quat dot is wrong", quat_dot(&a, &b) == 10.f);
    r2_assert("quat magnitude is wrong", quat_magnitude(&b) == 2.f);
    r2_assert("quat magnitude != length", quat_magnitude(&a) == quat_length(&a));
    return 0;
}

static const char *test_quat_mul_identity_and_conj(void)
{
    quat id, q, out, conj;
    quat_identity(&id);
    for (int i = 0; i < 100; i++)
    {
        q.x = m_rand();
        q.y = m_rand();
        q.z = m_rand();
        q.w = m_rand();
        quat_mul_quat(&q, &id, &out);
        r2_assert("q * 1 != q", m_vec4_near(&out, q.x, q.y, q.z, q.w, 0.f));
        quat_mul_quat(&id, &q, &out);
        r2_assert("1 * q != q", m_vec4_near(&out, q.x, q.y, q.z, q.w, 0.f));
        /* q * conj(q) = |q|^2, a real number */
        quat_conj(&q, &conj);
        quat_mul_quat(&q, &conj, &out);
        r2_assert("q * conj(q) != |q|^2", m_vec4_near(&out, 0.f, 0.f, 0.f, quat_dot(&q, &q), 1e-5f));
    }
    return 0;
}

static const char *test_quat_mul_associative(void)
{
    quat a, b, c, ab, bc, ab_c, a_bc;
    for (int i = 0; i < 100; i++)
    {
        a = (quat){.x = m_rand(), .y = m_rand(), .z = m_rand(), .w = m_rand()};
        b = (quat){.x = m_rand(), .y = m_rand(), .z = m_rand(), .w = m_rand()};
        c = (quat){.x = m_rand(), .y = m_rand(), .z = m_rand(), .w = m_rand()};
        quat_mul_quat(&a, &b, &ab);
        quat_mul_quat(&ab, &c, &ab_c);
        quat_mul_quat(&b, &c, &bc);
        quat_mul_quat(&a, &bc, &a_bc);
        r2_assert("quat mul not associative", m_vec4_near(&ab_c, a_bc.x, a_bc.y, a_bc.z, a_bc.w, 1e-5f));
    }
    return 0;
}

static const char *test_quat_ijk(void)
{
    /* Hamilton: ij = k, jk = i, ki = j, ji = -k */
    quat i = {.x = 1.f}, j = {.y = 1.f}, k = {.z = 1.f}, out;
    quat_mul_quat(&i, &j, &out);
    r2_assert("ij != k", m_vec4_near(&out, 0.f, 0.f, 1.f, 0.f, 0.f));
    quat_mul_quat(&j, &k, &out);
    r2_assert("jk != i", m_vec4_near(&out, 1.f, 0.f, 0.f, 0.f, 0.f));
    quat_mul_quat(&k, &i, &out);
    r2_assert("ki != j", m_vec4_near(&out, 0.f, 1.f, 0.f, 0.f, 0.f));
    quat_mul_quat(&j, &i, &out);
    r2_assert("ji != -k", m_vec4_near(&out, 0.f, 0.f, -1.f, 0.f, 0.f));
    quat_mul_quat(&i, &i, &out);
    r2_assert("ii != -1", m_vec4_near(&out, 0.f, 0.f, 0.f, -1.f, 0.f));
    return 0;
}

static const char *test_quat_rotation_preserves_length(void)
{
    quat q, qn;
    vec3 v, out;
    for (int i = 0; i < 100; i++)
    {
        q = (quat){.x = m_rand(), .y = m_rand(), .z = m_rand(), .w = m_rand()};
        quat_normalize(&q, &qn);
        v = (vec3){.x = m_rand(), .y = m_rand(), .z = m_rand(), .w = 0.f};
        quat_mul_vec3(&qn, &v, &out);
        r2_assert("rotation changed length", m_near(vec3_length(&out), vec3_length(&v), 1e-5f));
        r2_assert("rotation left a real part", m_near(out.w, 0.f, 1e-5f));
    }
    return 0;
}

static const char *test_quat_rot2q_composes(void)
{
    /* two 90 degree turns about z = one 180 degree turn */
    vec3 axis = {.z = 1.f};
    vec3 x = {.x = 1.f};
    quat q90, q180, qq;
    vec3 out;
    quat_rot2q(&axis, (float)M_PI / 2.f, &q90);
    quat_rot2q(&axis, (float)M_PI, &q180);
    quat_mul_quat(&q90, &q90, &qq);
    r2_assert("90*90 != 180", m_vec4_near(&qq, q180.x, q180.y, q180.z, q180.w, 1e-6f));
    quat_mul_vec3(&q90, &x, &out);
    r2_assert("90 about z should take x to y", m_near(out.x, 0.f, 1e-6f) && m_near(out.y, 1.f, 1e-6f));
    return 0;
}

static const char *test_quat_from_euler_matches_rot2q(void)
{
    /* a single euler angle is a rotation about that one axis */
    vec3 axes[3] = {{.x = 1.f}, {.y = 1.f}, {.z = 1.f}};
    for (int a = 0; a < 3; a++)
    {
        for (int i = 0; i < 20; i++)
        {
            float theta = m_rand() * (float)M_PI;
            vec3 euler = {0};
            quat qe, qa;
            euler.a_vec[a] = theta;
            quat_from_euler(&euler, &qe);
            quat_rot2q(&axes[a], theta, &qa);
            r2_assert("from_euler != rot2q on one axis", m_vec4_near(&qe, qa.x, qa.y, qa.z, qa.w, 1e-6f));
        }
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// Matrices (the remaining functions, plus algebraic properties)

static const char *test_mat_transpose(void)
{
    // 2x3 -> 3x2
    static const float a[6] = {1, 2, 3, 4, 5, 6};
    float t[6] = {0}, back[6] = {0};
    mat_transpose(a, 2, 3, t);
    r2_assert("mat_transpose is wrong",
              t[0] == 1 && t[1] == 4 && t[2] == 2 && t[3] == 5 && t[4] == 3 && t[5] == 6);
    mat_transpose(t, 3, 2, back);
    for (int i = 0; i < 6; i++)
        r2_assert("mat_transpose twice is wrong", back[i] == a[i]);
    return 0;
}

static const char *test_mat_transpose_matches_mat4(void)
{
    mat4 m, t;
    float g[16];
    for (int i = 0; i < 16; i++)
        m.a_mat4[i] = (float)i;
    mat4_transpose(&m, &t);
    mat_transpose(m.a_mat4, 4, 4, g);
    for (int i = 0; i < 16; i++)
        r2_assert("mat_transpose differs from mat4_transpose", g[i] == t.a_mat4[i]);
    return 0;
}

#ifndef HAVE_BLAS
static const char *test_mat_mul_size_mismatch(void)
{
    /* c1 != r2: out is left alone (and an error is printed) */
    static const float a[6] = {1, 2, 3, 4, 5, 6};
    float out[4] = {-1, -1, -1, -1};
    fprintf(stderr, "(expected error follows) ");
    mat_mul(a, a, 2, 3, 2, 3, out);
    r2_assert("mat_mul size mismatch wrote output",
              out[0] == -1 && out[1] == -1 && out[2] == -1 && out[3] == -1);
    return 0;
}
#endif

static const char *test_mat4_set_layout(void)
{
    /* mat4_set copies the rows straight into a_mat4 */
    float ary[16];
    mat4 m;
    for (int i = 0; i < 16; i++)
        ary[i] = (float)i;
    mat4_set(ary, &m);
    for (int i = 0; i < 16; i++)
        r2_assert("mat4_set layout is wrong", m.a_mat4[i] == ary[i]);
    return 0;
}

static const char *test_mat4_mul_properties(void)
{
    mat4 a, b, c, ab, ab_c, bc, a_bc, abt, at, bt, btat;
    for (int n = 0; n < 50; n++)
    {
        for (int i = 0; i < 16; i++)
        {
            a.a_mat4[i] = m_rand();
            b.a_mat4[i] = m_rand();
            c.a_mat4[i] = m_rand();
        }
        mat4_mul(&a, &b, &ab);
        mat4_mul(&ab, &c, &ab_c);
        mat4_mul(&b, &c, &bc);
        mat4_mul(&a, &bc, &a_bc);
        /* (AB)^T = B^T A^T */
        mat4_transpose(&ab, &abt);
        mat4_transpose(&a, &at);
        mat4_transpose(&b, &bt);
        mat4_mul(&bt, &at, &btat);
        for (int i = 0; i < 16; i++)
        {
            r2_assert("mat4 mul not associative", m_near(ab_c.a_mat4[i], a_bc.a_mat4[i], 1e-5f));
            r2_assert("(AB)^T != B^T A^T", m_near(abt.a_mat4[i], btat.a_mat4[i], 1e-5f));
        }
    }
    return 0;
}

static const char *test_mat3_mul_matches_mat_mul(void)
{
    mat3 a, b, out;
    float g[9];
    for (int i = 0; i < 9; i++)
    {
        a.a_mat3[i] = m_rand();
        b.a_mat3[i] = m_rand();
    }
    mat3_mul(&a, &b, &out);
    mat_mul(a.a_mat3, b.a_mat3, 3, 3, 3, 3, g);
    for (int i = 0; i < 9; i++)
        r2_assert("mat3_mul differs from mat_mul", out.a_mat3[i] == g[i]);
    return 0;
}

static const char *test_mat4_perspective(void)
{
    /* OpenGL clip space: the near plane maps to z/w = -1, far to +1, and
       the edge of the field of view to x/w or y/w = +-1 */
    float fov = (float)M_PI / 2.f, aspect = 2.f, n = 0.5f, f = 100.f;
    mat4 p;
    vec4 near_c = {.z = -n, .w = 1.f};
    vec4 far_c = {.z = -f, .w = 1.f};
    vec4 top = {.y = n * tanf(fov / 2.f), .z = -n, .w = 1.f};
    vec4 right = {.x = n * tanf(fov / 2.f) * aspect, .z = -n, .w = 1.f};
    vec4 out;
    mat4_perspective(fov, aspect, n, f, &p);

    m_apply(&p, &near_c, &out);
    r2_assert("perspective near plane is wrong", m_near(out.z / out.w, -1.f, 1e-5f));
    m_apply(&p, &far_c, &out);
    r2_assert("perspective far plane is wrong", m_near(out.z / out.w, 1.f, 1e-4f));
    r2_assert("perspective w should be -z", m_near(out.w, f, 1e-4f));
    m_apply(&p, &top, &out);
    r2_assert("perspective top edge is wrong", m_near(out.y / out.w, 1.f, 1e-5f));
    m_apply(&p, &right, &out);
    r2_assert("perspective right edge is wrong", m_near(out.x / out.w, 1.f, 1e-5f));
    return 0;
}

static const char *test_mat4_lookat_eye_to_origin(void)
{
    /* A view matrix moves the eye to the origin, and a point straight
       ahead at distance d to (0, 0, -d) */
    vec4 pos = {.x = 0, .y = 0, .z = 10, .w = 1};
    vec4 fwd = {.x = 0, .y = 0, .z = -1};
    vec4 up = {.x = 0, .y = 1, .z = 0};
    vec4 ahead = {.x = 0, .y = 0, .z = 7, .w = 1};
    vec4 out;
    mat4 view;
    mat4_lookat(&pos, &fwd, &up, &view);

    m_apply(&view, &pos, &out);
    r2_assert("lookat: eye does not map to origin", m_vec4_near(&out, 0.f, 0.f, 0.f, 1.f, 1e-6f));
    m_apply(&view, &ahead, &out);
    r2_assert("lookat: point ahead is wrong", m_vec4_near(&out, 0.f, 0.f, -3.f, 1.f, 1e-6f));
    return 0;
}

static const char *test_mat4_lookat_off_axis(void)
{
    /* Camera at (5, 0, 0) looking down +x: the eye still maps to the
       origin (needs the translation to be -dot(axis, pos), not -pos) */
    vec4 pos = {.x = 5, .y = 0, .z = 0, .w = 1};
    vec4 fwd = {.x = 1, .y = 0, .z = 0};
    vec4 up = {.x = 0, .y = 1, .z = 0};
    vec4 ahead = {.x = 9, .y = 0, .z = 0, .w = 1};
    vec4 out;
    mat4 view;
    mat4_lookat(&pos, &fwd, &up, &view);

    m_apply(&view, &pos, &out);
    r2_assert("lookat off-axis: eye does not map to origin", m_vec4_near(&out, 0.f, 0.f, 0.f, 1.f, 1e-6f));
    m_apply(&view, &ahead, &out);
    r2_assert("lookat off-axis: point ahead is wrong", m_vec4_near(&out, 0.f, 0.f, -4.f, 1.f, 1e-6f));
    return 0;
}

static const char *test_mat4_lookat_random(void)
{
    /* Any orthonormal camera: eye -> origin, eye + d*fwd -> (0, 0, -d),
       eye + up -> (0, 1, 0) */
    for (int n = 0; n < 100; n++)
    {
        vec4 fwd = {0}, up = {0}, side = {0}, tmp = {0}, pos, p, out;
        mat4 view;
        tmp = (vec4){.x = m_rand(), .y = m_rand(), .z = m_rand()};
        vec3_normalize(&tmp, &fwd);
        tmp = (vec4){.x = m_rand(), .y = m_rand(), .z = m_rand()};
        vec3_cross(&fwd, &tmp, &side);
        vec3_cross(&side, &fwd, &tmp);
        vec3_normalize(&tmp, &up);
        pos = (vec4){.x = m_rand() * 50.f, .y = m_rand() * 50.f, .z = m_rand() * 50.f, .w = 1.f};
        mat4_lookat(&pos, &fwd, &up, &view);

        m_apply(&view, &pos, &out);
        r2_assert("lookat random: eye not at origin", m_vec4_near(&out, 0.f, 0.f, 0.f, 1.f, 1e-4f));
        p = (vec4){.x = pos.x + 3.f * fwd.x, .y = pos.y + 3.f * fwd.y, .z = pos.z + 3.f * fwd.z, .w = 1.f};
        m_apply(&view, &p, &out);
        r2_assert("lookat random: ahead not on -z", m_vec4_near(&out, 0.f, 0.f, -3.f, 1.f, 1e-4f));
        p = (vec4){.x = pos.x + up.x, .y = pos.y + up.y, .z = pos.z + up.z, .w = 1.f};
        m_apply(&view, &p, &out);
        r2_assert("lookat random: up not on +y", m_vec4_near(&out, 0.f, 1.f, 0.f, 1.f, 1e-4f));
    }
    return 0;
}

///////////////////////////////////////////////////////////////
// *_tos exact output

static const char *test_tos_exact(void)
{
    vec4 v = {.x = 1.f, .y = -2.f, .z = 0.5f, .w = 4.f};
    mat3 m3;
    mat4 m4;
    char buf[R2_MAT4_TOS_BUF_SIZE];
    vec4_tos(&v, buf, R2_VEC4_TOS_BUF_SIZE);
    r2_assert("vec4_tos text is wrong", strcmp(buf, "(1.000000, -2.000000, 0.500000, 4.000000)\n") == 0);
    quat_tos(&v, buf, R2_QUAT_TOS_BUF_SIZE);
    r2_assert("quat_tos text is wrong",
              strcmp(buf, "[4.000000 + 1.000000i + -2.000000j + 0.500000k]\n") == 0);
    mat3_identity(&m3);
    mat3_tos(&m3, buf, R2_MAT3_TOS_BUF_SIZE);
    r2_assert("mat3_tos text is wrong",
              strcmp(buf, "[\n 1.000000, 0.000000, 0.000000 \n 0.000000, 1.000000, 0.000000 \n"
                          " 0.000000, 0.000000, 1.000000 \n]\n") == 0);
    mat4_identity(&m4);
    mat4_tos(&m4, buf, R2_MAT4_TOS_BUF_SIZE);
    r2_assert("mat4_tos text is wrong",
              strcmp(buf, "[\n 1.000000, 0.000000, 0.000000, 0.000000 \n"
                          " 0.000000, 1.000000, 0.000000, 0.000000 \n"
                          " 0.000000, 0.000000, 1.000000, 0.000000 \n"
                          " 0.000000, 0.000000, 0.000000, 1.000000 \n]\n") == 0);
    return 0;
}

static const char *test_tos_buf_sizes_fit_large_values(void)
{
    /* The suggested buffer sizes should hold "typical" big values
       (-99999.999999 everywhere) without truncating */
    vec4 v = {.x = -99999.f, .y = -99999.f, .z = -99999.f, .w = -99999.f};
    mat3 m3;
    mat4 m4;
    char buf[1024];
    for (int i = 0; i < 9; i++)
        m3.a_mat3[i] = -99999.f;
    for (int i = 0; i < 16; i++)
        m4.a_mat4[i] = -99999.f;
    vec4_tos(&v, buf, sizeof(buf));
    r2_assert("R2_VEC4_TOS_BUF_SIZE too small", strlen(buf) < R2_VEC4_TOS_BUF_SIZE);
    quat_tos(&v, buf, sizeof(buf));
    r2_assert("R2_QUAT_TOS_BUF_SIZE too small", strlen(buf) < R2_QUAT_TOS_BUF_SIZE);
    mat3_tos(&m3, buf, sizeof(buf));
    r2_assert("R2_MAT3_TOS_BUF_SIZE too small", strlen(buf) < R2_MAT3_TOS_BUF_SIZE);
    mat4_tos(&m4, buf, sizeof(buf));
    r2_assert("R2_MAT4_TOS_BUF_SIZE too small", strlen(buf) < R2_MAT4_TOS_BUF_SIZE);
    return 0;
}

static const char *r2_maths_test(void)
{
    // v2
    r2_run_test(test_vec2_add);
    r2_run_test(test_vec2_sub);
    r2_run_test(test_vec2_div);
    r2_run_test(test_vec2_div_zero);
    r2_run_test(test_vec2_div_vec2);
    r2_run_test(test_vec2_div_vec2_zero);
    r2_run_test(test_vec2_dot);

    // v3
    r2_run_test(test_vec3_add);
    r2_run_test(test_vec3_mul_clobber);
    r2_run_test(test_vec3_cross);
    r2_run_test(test_vec3_normalize);
    r2_run_test(test_vec3_length);
    r2_run_test(test_vec3_equal);

    // v4
    r2_run_test(test_vec4_div);
    r2_run_test(test_vec4_normalize);
    r2_run_test(test_vec4_cross);
    r2_run_test(test_vec4_dist_sqrd);
    r2_run_test(test_vec4_dist);

    // quat
    r2_run_test(test_quat_rot2q);
    r2_run_test(test_quat_from_euler);
    r2_run_test(test_quat_length);
    r2_run_test(test_quat_normalize);
    r2_run_test(test_quat_conj);
    r2_run_test(test_quat_mul_quat);
    r2_run_test(test_quat_mul_vec3_x_90z);
    r2_run_test(test_quat_mul_vec3_y_90x);
    r2_run_test(test_quat_mul_vec3_y_180x);
    r2_run_test(test_quat_mat4);

    // mat4
    r2_run_test(test_mat4_size);
    r2_run_test(test_mat4_transform);
    r2_run_test(test_mat4_lookat);
    r2_run_test(test_mat4_identity);
    r2_run_test(test_mat4_mul);
    r2_run_test(test_mat4_mul2);
    r2_run_test(test_mat4_mul_identity);
    r2_run_test(test_mat4_mul_not_commutative);
    r2_run_test(test_mat4_transpose);
    r2_run_test(test_mat4_transpose_twice);
    r2_run_test(test_mat4_mul_speed);

    // mat3
    r2_run_test(test_mat3_mul);
    r2_run_test(test_mat3_identity);
    r2_run_test(test_mat3_mul_identity);
    r2_run_test(test_mat3_mul_not_commutative);

    // generic mat
    r2_run_test(test_mat_mul);
    r2_run_test(test_mat_mul_non_square);

    // vecn
    r2_run_test(test_vecn_add);
    r2_run_test(test_vecn_sub);
    r2_run_test(test_vecn_mul);
    r2_run_test(test_vecn_mul_vec);
    r2_run_test(test_vecn_div);
    r2_run_test(test_vecn_div_zero);
    r2_run_test(test_vecn_div_vec);
    r2_run_test(test_vecn_dot);
    r2_run_test(test_vecn_length);
    r2_run_test(test_vecn_dist);
    r2_run_test(test_vecn_normalize);
    r2_run_test(test_vecn_normalize_zero);
    r2_run_test(test_vecn_abs);
    r2_run_test(test_vecn_pow);
    r2_run_test(test_vecn_sqrt);
    r2_run_test(test_vecn_arbitrary_n);
    r2_run_test(test_vecn_dot_large);
    r2_run_test(test_vecn_length_large);
    r2_run_test(test_vecn_mul_large);
    r2_run_test(test_vecn_div_large);

    r2_run_test(test_vec4_tos);
    r2_run_test(test_vec4_tos_truncates_safely);
    r2_run_test(test_quat_tos);
    r2_run_test(test_mat3_tos);
    r2_run_test(test_mat4_tos);
    r2_run_test(test_tos_exact);
    r2_run_test(test_tos_buf_sizes_fit_large_values);

    // fns
    r2_run_test(test_deg_to_rad);
    r2_run_test(test_r2_equals_epsilon);

    // v2 (rest)
    r2_run_test(test_vec2_zero_set_equals);
    r2_run_test(test_vec2_mul);
    r2_run_test(test_vec2_length_dist);
    r2_run_test(test_vec2_normalize);
    r2_run_test(test_vec2_edge_cross);
    r2_run_test(test_vec2_to_array);

    // v3 (rest)
    r2_run_test(test_vec3_zero_set);
    r2_run_test(test_vec3_sub_div_mul);
    r2_run_test(test_vec3_dot_length_dist);
    r2_run_test(test_vec3_ignores_w);
    r2_run_test(test_vec3_cross_properties);
    r2_run_test(test_vec3_normalize_zero);

    // v4 (rest)
    r2_run_test(test_vec4_zero_set_equals);
    r2_run_test(test_vec4_add_sub_mul);
    r2_run_test(test_vec4_sqrt_dot_length);
    r2_run_test(test_vec4_normalize_random);
    r2_run_test(test_vecn_equals_zero_n);

    // quat (rest + properties)
    r2_run_test(test_quat_zero_identity);
    r2_run_test(test_quat_add_sub_dot_magnitude);
    r2_run_test(test_quat_mul_identity_and_conj);
    r2_run_test(test_quat_mul_associative);
    r2_run_test(test_quat_ijk);
    r2_run_test(test_quat_rotation_preserves_length);
    r2_run_test(test_quat_rot2q_composes);
    r2_run_test(test_quat_from_euler_matches_rot2q);

    // matrices (rest + properties)
    r2_run_test(test_mat_transpose);
    r2_run_test(test_mat_transpose_matches_mat4);
#ifndef HAVE_BLAS
    r2_run_test(test_mat_mul_size_mismatch);
#endif
    r2_run_test(test_mat4_set_layout);
    r2_run_test(test_mat4_mul_properties);
    r2_run_test(test_mat3_mul_matches_mat_mul);
    r2_run_test(test_mat4_perspective);
    r2_run_test(test_mat4_lookat_eye_to_origin);
    r2_run_test(test_mat4_lookat_off_axis);
    r2_run_test(test_mat4_lookat_random);

    return 0;
}
