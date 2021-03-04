#include "../r2_maths.h"
#include "../r2_unit.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if (__WORDSIZE == 64)
#define BUILD_64 1
#endif

static char *test_vec2_add()
{
    vec2 v1 = {.x = 10, .y = 10};
    vec2 v2 = {.x = 10.f, .y = 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_add(&v1, &v2, v3);
    r2_assert("Vec2 add failed", (v3->x == 20.f && v3->y == 20.f));
    free(v3);
    return 0;
}

static char *test_vec2_sub()
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 10.f, .y = 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_sub(&v1, &v2, v3);
    r2_assert("Vec2 sub failed", (v3->x == 0.f && v3->y == 0.f));
    free(v3);
    return 0;
}

static char *test_vec2_div()
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div(&v1, 2., v3);
    r2_assert("Vec2 div failed", (v3->x == 5.f && v3->y == 5.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_zero()
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
    vec2_div(&v1, 0, v3);
    r2_assert("Vec2 div zero failed", (v3->x == 10.f && v3->y == 10.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_vec2()
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 5.f, .y = 2.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div_vec2(&v1, &v2, v3);
    r2_assert("Vec2 div failed", (v3->x == 2.f && v3->y == 5.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_vec2_zero()
{
    vec2 v1 = {.x = 10.f, .y = 10.f};
    vec2 v2 = {.x = 0.f, .y = 0.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div_vec2(&v1, &v2, v3);
    r2_assert("Vec2 div zero failed", (v3->x == 10.f && v3->y == 10.f));
    free(v3);
    return 0;
}

static char *test_vec2_dot()
{
    vec2 v1 = {.x = 3.f, .y = 3.f};
    vec2 v2 = {.x = -3.f, .y = -3.f};
    float theta = vec2_dot(&v1, &v2);
    r2_assert("vec2 dot was wonky", theta == -18.);
    return 0;
}

static char *test_vec3_add()
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -3.f, .y = -3.f, .z = -3.f};
    vec4 *out = malloc(sizeof(vec4));
    vec3_add(&v1, &v2, out);
    r2_assert("vec3 add is wrong", (out->x == 0.f && out->y == 0.f && out->z == 0.f));
    free(out);
    return 0;
}

static char *test_vec3_mul_clobber()
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3_mul(&v1, 3, &v1);
    r2_assert("vec3 mul is wrong", (v1.x == 9.f && v1.y == 9.f && v1.z == 9.f));
    return 0;
}

static char *test_vec3_cross()
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
    vec4 *out = malloc(sizeof(vec4));
    vec3_cross(&v1, &v2, out);
    r2_assert("vec3 cross is wrong", (out->x == -180.f && out->y == 0.f && out->z == 180.f));
    free(out);
    return 0;
}

static char *test_vec3_equal()
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
    r2_assert("vec3 equal is wrong", !vec3_equals(&v1, &v2));
    return 0;
}

static char *test_vec3_to_array()
{
    vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
    float v2[3] = {0., 0., 0.};
    // TODO: I don't think this is needed this anymore
    vec3_to_array(&v1, v2);
    r2_assert("vec3 to array is wrong", (v2[0] == 3. && v2[1] == 3. && v2[2] == 3.));
    return 0;
}

static char *test_vec3_normalize()
{
    vec3 v1 = {.x = 10.f, .y = 0.f, .z = 0.f};
    vec4 *out = malloc(sizeof(vec3));
    vec3_normalize(&v1, out);
    r2_assert("vec3 normalize is wrong", (out->x == 1.f && out->y == 0.f && out->z == 0.f));
    free(out);
    return 0;
}

// Magnitude, Length...
static char *test_vec3_length()
{
    vec3 v1 = {.x = 10.f, .y = 0.f, .z = 0.f};
    float l = vec3_length(&v1);
    r2_assert("vec3 length is wrong", l == 10.f);
    return 0;
}

static char *test_vec4_div()
{
    vec3 v1 = {.x = 10.f, .y = 10.f, .z = 12.f, .w = 12.f};
    vec4 *out = malloc(sizeof(vec4));
    vec4_div(&v1, 2., out);
    r2_assert("vec4 div is wrong", out->x == 5.f && out->y == 5.f && out->z == 6.f && out->w == 6.f);
    free(out);
    return 0;
}

static char *test_vec4_normalize()
{
    vec4 v1 = {.x = 1.f, .y = 2.f, .z = 3.f, .w = 4.f};
    vec4 *out = malloc(sizeof(vec4));
    vec4_normalize(&v1, out);

    // Octave: 0.18257   0.36515   0.54772   0.73030
    r2_assert("vec4 normalize is wrong", r2_equals(out->x, .182574) && r2_equals(out->y, 0.365148) &&
                                             r2_equals(out->z, 0.547723) && r2_equals(out->w, 0.730297));

    free(out);
    return 0;
}

static char *test_vec4_cross()
{
    vec4 v1 = {.x = 1.f, .y = 1.f, .z = 0.f, .w = 0.f};
    vec4 v2 = {.x = 0.f, .y = 1.f, .z = 1.f, .w = 0.f};
    vec4 *out = malloc(sizeof(vec4));

    vec3_cross(&v1, &v2, out);

    // Octave: 1 -1 1
    r2_assert("vec4 cross is wrong", r2_equals(out->x, 1.) && r2_equals(out->y, -1.) && r2_equals(out->z, 1.));

    free(out);
    return 0;
}

static char *test_quat_rot2q()
{
    quat *out = malloc(sizeof(quat));

    vec3 axis = {.x = 0, .y = 1, .z = 0};
    float angle = M_PI / 4;

    quat_rot2q(&axis, angle, out);

    // Octave: q = 0.9239 + 0i + 0.3827j + 0k
    float r = out->w;
    float i = out->x;
    float j = out->y;
    float k = out->z;

    r2_assert("quat rot2q is wrong",
              r2_equals(r, 0.923880) && r2_equals(i, 0) && r2_equals(j, 0.382683) && r2_equals(k, 0.));

    free(out);
    return 0;
}

static char *test_quat_from_euler()
{
    vec3 v1 = {.x = M_PI / 2., .y = 0.f, .z = 0.f};
    quat *out = malloc(sizeof(quat));
    quat_from_euler(&v1, out);

    // Octave: q = 0.7071 + 0.7071i + 0j + 0k
    float r = out->w;
    float i = out->x;
    float j = out->y;
    float k = out->z;

    r2_assert("quat euler is wrong",
              r2_equals(r, 0.707107) && r2_equals(i, 0.707107) && r2_equals(j, 0.) && r2_equals(k, 0.));
    free(out);
    return 0;
}

static char *test_quat_length()
{
    quat q1 = {.x = 0.000000, .y = 0.850904, .z = 0.000000, .w = 0.525322};
    float actual = quat_length(&q1);
    r2_assert("quat length is wrong", r2_equals(actual, 1.));
    return 0;
}

static char *test_quat_normalize()
{
    quat q1 = {.x = .5f, .y = .3f, .z = .2f, .w = .1f};
    quat *out = malloc(sizeof(quat));
    quat_normalize(&q1, out);
    r2_assert("quat normalize is wrong", r2_equals(out->x, .800641) && r2_equals(out->y, 0.480384) &&
                                             r2_equals(out->z, 0.320256) && r2_equals(out->w, 0.160128));
    free(out);
    return 0;
}

static char *test_quat_mul_quat()
{
    vec3 v1 = {.x = 0.f, .y = M_PI / 2, .z = 0.f};
    quat *q1 = malloc(sizeof(quat));
    quat_from_euler(&v1, q1);

    vec3 v2 = {.x = M_PI / 4, .y = 0.f, .z = 0.f};
    quat *q2 = malloc(sizeof(quat));
    quat_from_euler(&v2, q2);

    quat *q3 = malloc(sizeof(quat));
    quat_mul_quat(q2, q1, q3);

    r2_assert("quat mul quat", r2_equals(q3->w, 0.653281) && r2_equals(q3->x, 0.270598) && r2_equals(q3->y, 0.653281) &&
                                   r2_equals(q3->z, 0.270598));

    free(q1);
    free(q2);
    free(q3);
    return 0;
}

static char *test_quat_mul_vec3_x_90z()
{
    // Rotate the X point 90 deg about the Z axis
    //  y
    //  |1
    //  |
    //  |   1
    //  ----->x
    vec3 v1 = {.x = 0.f, .y = 0.f, .z = M_PI / 2};
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {.x = 1.f, .y = 0.f, .z = 0.f};
    quat *result = malloc(sizeof(vec3));

    quat_mul_vec3(q, &v, result);

    r2_assert("quat mul vec3 is wrong (x)", r2_equals(result->x, 0.) && r2_equals(result->y, 1.) &&
                                                r2_equals(result->z, 0.) && r2_equals(result->w, 0.));

    free(q);
    free(result);
    return 0;
}

static char *test_quat_mul_vec3_y_90x()
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
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {.x = 0.f, .y = 1.f, .z = 0.f};
    quat *result = malloc(sizeof(vec3));

    quat_mul_vec3(q, &v, result);

    r2_assert("quat mul vec3 is wrong (y 90x)", r2_equals(result->x, 0.) && r2_equals(result->y, 0.) &&
                                                    r2_equals(result->z, 1.) && r2_equals(result->w, 0.));

    free(q);
    free(result);
    return 0;
}

static char *test_quat_mul_vec3_y_180x()
{
    vec3 v1 = {.x = M_PI, .y = 0.f, .z = 0.f};
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {.x = 0.f, .y = 1.f, .z = 0.f};
    quat *result = malloc(sizeof(vec3));

    quat_mul_vec3(q, &v, result);

    r2_assert("quat mul vec3 is wrong (y 180x)", r2_equals(result->x, 0.) && r2_equals(result->y, -1.) &&
                                                     r2_equals(result->z, 0.) && r2_equals(result->w, 0.));

    free(q);
    free(result);
    return 0;
}

static char *test_quat_mat4()
{
    quat *q = malloc(sizeof(quat));
    quat_identity(q);

    mat4 *result = malloc(sizeof(mat4));
    quat_mat4(q, result);

    r2_assert("quat mat4 is wrong", r2_equals(result->m00, 1.) && r2_equals(result->m11, 1.) &&
                                        r2_equals(result->m22, 1.) && r2_equals(result->m33, 1.));

    free(q);
    free(result);
    return 0;
}

static char *test_mat4_size()
{
    r2_assert("mat4 padding is wrong", sizeof(mat4) == 64);
    r2_assert("mat3 padding is wrong", sizeof(mat3) == 36);
    r2_assert("vec4 padding is wrong", sizeof(vec4) == 16);
    r2_assert("vec2 padding is wrong", sizeof(vec2) == 8);
    return 0;
}

static char *test_mat4_transform()
{
    mat4 *kern = malloc(sizeof(mat4));
    vec4 *p = malloc(sizeof(vec4));
    vec4 *r = malloc(sizeof(vec4));

    float ary[4] = {3., 3., 3., 0.};
    vec4_set(p, ary);

    float ary2[16] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    mat4_set(kern, ary2);

    mat4_transform(p, kern, r);

    r2_assert("mat4 transform is wrong",
              r2_equals(r->x, 18.) && r2_equals(r->y, 18.) && r2_equals(r->z, 18.) && r2_equals(r->w, 18.));

    free(kern);
    free(p);
    free(r);
    return 0;
}

static char *test_mat4_mul()
{
    mat4 *k1 = malloc(sizeof(mat4));
    mat4 *k2 = malloc(sizeof(mat4));
    mat4 *out = malloc(sizeof(mat4));

    static const float k1mat[16] = {1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4};
    memcpy(k1->a_mat4, k1mat, sizeof(k1mat));

    static const float k1mat2[16] = {4, 4, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1};
    memcpy(k2->a_mat4, k1mat2, sizeof(k1mat2));

    mat4_mul(k1, k2, out);

    // 10.000000 10.000000 10.000000 10.000000
    // 20.000000 20.000000 20.000000 20.000000
    // 30.000000 30.000000 30.000000 30.000000
    // 40.000000 40.000000 40.000000 40.000000

    r2_assert("mat4 mul is wrong",
              r2_equals(out->m00, 10.) && r2_equals(out->m10, 10.) && r2_equals(out->m20, 10.) &&
                  r2_equals(out->m30, 10.) && r2_equals(out->m01, 20.) && r2_equals(out->m11, 20.) &&
                  r2_equals(out->m21, 20.) && r2_equals(out->m31, 20.) && r2_equals(out->m02, 30.) &&
                  r2_equals(out->m12, 30.) && r2_equals(out->m22, 30.) && r2_equals(out->m32, 30.) &&
                  r2_equals(out->m03, 40.) && r2_equals(out->m13, 40.) && r2_equals(out->m23, 40.) &&
                  r2_equals(out->m33, 40.));

    free(k1);
    free(k2);
    free(out);
    return 0;
}

static char *test_mat4_mul2()
{
    mat4 *k1 = malloc(sizeof(mat4));
    mat4 *k2 = malloc(sizeof(mat4));
    mat4 *out = malloc(sizeof(mat4));

    static const float k1mat[16] = {.9, .82, .1, 3, .39, 4, 1, .22, .20, 3, 0, 1, 0, 3, 1, .9};
    memcpy(k1->a_mat4, k1mat, sizeof(k1mat));

    static const float k1mat2[16] = {3, 48, 21, 3, .32, .45, .22, .3, 3, 0, .4, .4, .312, .2, 3, 2};
    memcpy(k2->a_mat4, k1mat2, sizeof(k1mat2));

    mat4_mul(k1, k2, out);

    // clang-format off
    // printf("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",
    //    out->m00, out->m10, out->m20, out->m30,
    //    out->m01, out->m11, out->m21, out->m31,
    //    out->m02, out->m12, out->m22, out->m32,
    //    out->m03, out->m13, out->m23, out->m33);
    // clang-format on

#ifdef BUILD_64
    r2_assert("mat4 mul 2 is wrong",
              r2_equals(out->m00, 4.198400) && r2_equals(out->m10, 44.168995) && r2_equals(out->m20, 28.120401) &&
                  r2_equals(out->m30, 8.986000) && r2_equals(out->m01, 5.518640) && r2_equals(out->m11, 20.563999) &&
                  r2_equals(out->m21, 10.129999) && r2_equals(out->m31, 3.21000) && r2_equals(out->m02, 1.87200) &&
                  r2_equals(out->m12, 11.150001) && r2_equals(out->m22, 7.86000) && r2_equals(out->m32, 3.500000) &&
                  r2_equals(out->m03, 4.240800) && r2_equals(out->m13, 1.530000) && r2_equals(out->m23, 3.76000) &&
                  r2_equals(out->m33, 3.10000));
#elif EMSCRIPTEN
    // 4.198400 44.168995 28.120401 8.986000
    // 5.518640 20.563999 10.129999 3.210000
    // 1.872000 11.150001 7.860000 3.500000
    // 4.240800 1.530000 3.760000 3.100000
    r2_assert("mat4 mul 2 is wrong",
              r2_equals(out->m00, 4.198400) && r2_equals(out->m10, 44.168995) && r2_equals(out->m20, 28.120401) &&
                  r2_equals(out->m30, 8.986000) && r2_equals(out->m01, 5.518640) && r2_equals(out->m11, 20.563999) &&
                  r2_equals(out->m21, 10.129999) && r2_equals(out->m31, 3.21000) && r2_equals(out->m02, 1.87200) &&
                  r2_equals(out->m12, 11.150001) && r2_equals(out->m22, 7.86000) && r2_equals(out->m32, 3.500000) &&
                  r2_equals(out->m03, 4.240800) && r2_equals(out->m13, 1.530000) && r2_equals(out->m23, 3.76000) &&
                  r2_equals(out->m33, 3.10000));
#else
    r2_assert("mat4 mul 2 is wrong",
              r2_equals(out->m00, 4.198400) && r2_equals(out->m10, 44.168999) && r2_equals(out->m20, 28.120399) &&
                  r2_equals(out->m30, 8.986000) && r2_equals(out->m01, 5.518640) && r2_equals(out->m11, 20.563999) &&
                  r2_equals(out->m21, 10.13000) && r2_equals(out->m31, 3.21000) && r2_equals(out->m02, 1.87200) &&
                  r2_equals(out->m12, 11.150001) && r2_equals(out->m22, 7.86000) && r2_equals(out->m32, 3.500000) &&
                  r2_equals(out->m03, 4.240800) && r2_equals(out->m13, 1.530000) && r2_equals(out->m23, 3.76000) &&
                  r2_equals(out->m33, 3.10000));
#endif

    free(k1);
    free(k2);
    free(out);
    return 0;
}

static char *test_mat4_mul_speed()
{
    time_t ti;
    mat4 *k1 = malloc(sizeof(mat4));
    mat4 *k2 = malloc(sizeof(mat4));
    mat4 *out = malloc(sizeof(mat4));

    static const float k1mat[16] = {.9, .82, .01, 3, .3, 4, 1, .22, .20, 3, 111.03, 1, 0, 3, 1, .9};
    memcpy(k1->a_mat4, k1mat, sizeof(k1mat));

    static const float k1mat2[16] = {3, 4.8, 21, 3, .32, .45, .22, .3, 3, 0.993, .4, .4, .312, .2, 3.99999, 2};
    memcpy(k2->a_mat4, k1mat2, sizeof(k1mat2));

    srand((unsigned)time(&ti));

    printf("Matrix Mul 4x4 10 run of 1000...\n");
    double time_taken = 0;
    clock_t t;

    for (int z = 0; z < 10; z++)
    {
        for (unsigned int x = 0; x < 10000; x++)
        {
            k1->m00 = rand();
            k1->m10 = rand();
            k1->m31 = rand();

            t = clock();
            mat4_mul(k1, k2, out);
            time_taken += ((double)clock() - t) / CLOCKS_PER_SEC;
        }
        ////////////////////////
        printf("Run %d; Time %f sec\n", z, time_taken);
        // printf("%f\n", time_taken);
        time_taken = 0;
    }

    return 0;
}

static char *test_mat3_mul()
{
    mat3 *k1 = malloc(sizeof(mat3));
    mat3 *k2 = malloc(sizeof(mat3));
    mat3 *out = malloc(sizeof(mat3));

    // clang-format off
    static const float k1mat[9] = {
        1, 0, 300,
        0, 2, 300,
        0, 0, 1
    };
    // clang-format on
    memcpy(k1->a_mat3, k1mat, sizeof(k1mat));

    // clang-format off
    static const float k1mat2[9] = {
        -1.836970198, 1, 0,
        -1, -1.836970198, 0,
        0, 0, 1
    };
    // clang-format on

    memcpy(k2->a_mat3, k1mat2, sizeof(k1mat2));

    printf("Matrix Mul 3x3 run...\n");
    mat3_mul(k1, k2, out);

    // -1.836970198, 1, 300,
    // -2, -3.67394039, 300,
    // 0, 0, 1
    r2_assert("mat3 mul is wrong",
        r2_equals(out->m00, -1.836970) && r2_equals(out->m10, 1.) && r2_equals(out->m20, 300.) &&
        r2_equals(out->m01, -2.) && r2_equals(out->m11, -3.673940) && r2_equals(out->m21, 300.) &&
        r2_equals(out->m02, 0.) && r2_equals(out->m12, 0.) && r2_equals(out->m22, 1.));

    free(k1);
    free(k2);
    free(out);
    return 0;
}

static char *r2_maths_test()
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
    r2_run_test(test_vec3_to_array);
    r2_run_test(test_vec3_normalize);
    r2_run_test(test_vec3_length);
    r2_run_test(test_vec3_equal);

    // v4
    r2_run_test(test_vec4_div);
    r2_run_test(test_vec4_normalize);
    r2_run_test(test_vec4_cross);

    // quat
    r2_run_test(test_quat_rot2q);
    r2_run_test(test_quat_from_euler);
    r2_run_test(test_quat_length);
    r2_run_test(test_quat_normalize);
    r2_run_test(test_quat_mul_quat);
    r2_run_test(test_quat_mul_vec3_x_90z);
    r2_run_test(test_quat_mul_vec3_y_90x);
    r2_run_test(test_quat_mul_vec3_y_180x);
    r2_run_test(test_quat_mat4);

    // mat
    r2_run_test(test_mat4_size);
    r2_run_test(test_mat4_transform);
    r2_run_test(test_mat4_mul);
    r2_run_test(test_mat4_mul2);
    r2_run_test(test_mat4_mul_speed);
    r2_run_test(test_mat3_mul);

    return 0;
}
