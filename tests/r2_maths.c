#define R2_MATHS_IMPLEMENTATION
#include "../r2_maths.h"

#include <limits.h>
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
    // printf("%s", mat4_tos(view));
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
    // leak
    // printf("%s", mat3_tos(out));

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

    // char *m = mat3_tos(&out);
    // printf("%s", m);
    // free(m);

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

    return 0;
}
