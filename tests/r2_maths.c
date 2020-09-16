#include "../r2_maths.h"
#include "../r2_unit.h"
#include <stdio.h>
#include <stdlib.h>

static char *test_vec2_add()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {10.f, 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_add(&v1, &v2, v3);
    r2_assert("Vec2 add failed", (v3->x == 20.f && v3->y == 20.f));
    free(v3);
    return 0;
}

static char *test_vec2_sub()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {10.f, 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_sub(&v1, &v2, v3);
    r2_assert("Vec2 sub failed", (v3->x == 0.f && v3->y == 0.f));
    free(v3);
    return 0;
}

static char *test_vec2_div()
{
    vec2 v1 = {10.f, 10.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div(&v1, 2., v3);
    r2_assert("Vec2 div failed", (v3->x == 5.f && v3->y == 5.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_zero()
{
    vec2 v1 = {10.f, 10.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
    vec2_div(&v1, 0, v3);
    r2_assert("Vec2 div zero failed", (v3->x == 10.f && v3->y == 10.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_vec2()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {5.f, 2.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div_vec2(&v1, &v2, v3);
    r2_assert("Vec2 div failed", (v3->x == 2.f && v3->y == 5.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_vec2_zero()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {0.f, 0.f};
    vec2 *v3 = malloc(sizeof(vec2));
    vec2_div_vec2(&v1, &v2, v3);
    r2_assert("Vec2 div zero failed", (v3->x == 10.f && v3->y == 10.f));
    free(v3);
    return 0;
}

static char *test_new_vec4_ptr()
{
    vec4 *v4 = calloc(1, sizeof(vec4));
    r2_assert("Vec4 as pointer", (v4->x == 0.f && v4->y == 0.f && v4->z == 0.f && v4->w == 0.f));
    free(v4);
    return 0;
}

static char *test_vec2_dot()
{
    vec2 v1 = {3.f, 3.f};
    vec2 v2 = {-3.f, -3.f};
    float theta = vec2_dot(&v1, &v2);
    r2_assert("vec2 dot was wonky", theta == -18.);
    return 0;
}

static char *test_vec3_add()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3 v2 = {-3.f, -3.f, -3.f};
    vec4 *out = malloc(sizeof(vec4));
    vec3_add(&v1, &v2, out);
    r2_assert("vec3 add is wrong", (out->x == 0.f && out->y == 0.f && out->z == 0.f));
    free(out);
    return 0;
}

static char *test_vec3_mul_clobber()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3_mul(&v1, 3, &v1);
    r2_assert("vec3 mul is wrong", (v1.x == 9.f && v1.y == 9.f && v1.z == 9.f));
    return 0;
}

static char *test_vec3_cross()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3 v2 = {-30.f, 30.f, -30.f};
    vec4 *out = malloc(sizeof(vec4));
    vec3_cross(&v1, &v2, out);
    r2_assert("vec3 cross is wrong", (out->x == -180.f && out->y == 0.f && out->z == 180.f));
    free(out);
    return 0;
}

static char *test_vec3_equal()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3 v2 = {-30.f, 30.f, -30.f};
    r2_assert("vec3 equal is wrong", !vec3_equals(&v1, &v2));
    return 0;
}

static char *test_vec3_to_array()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    float v2[3] = {0., 0., 0.};
    vec3_to_array(&v1, v2);
    r2_assert("vec3 to array is wrong", (v2[0] == 3. && v2[1] == 3. && v2[2] == 3.));
    return 0;
}

static char *test_vec3_normalize()
{
    vec3 v1 = {10.f, 0.f, 0.f};
    vec4 *out = malloc(sizeof(vec3));
    vec3_normalize(&v1, out);
    r2_assert("vec3 normalize is wrong", (out->x == 1.f && out->y == 0.f && out->z == 0.f));
    free(out);
    return 0;
}

// Magnitude, Length...
static char *test_vec3_length()
{
    vec3 v1 = {10.f, 0.f, 0.f};
    float l = vec3_length(&v1);
    r2_assert("vec3 length is wrong", l == 10.f);
    return 0;
}

static char *test_vec4_div()
{
    vec3 v1 = {10.f, 10.f, 12.f, 12.f};
    vec4 *out = malloc(sizeof(vec4));
    vec4_div(&v1, 2., out);
    r2_assert("vec4 div is wrong", out->x == 5.f && out->y == 5.f && out->z == 6.f && out->w == 6.f);
    free(out);
    return 0;
}

static char *test_quat_rot2q()
{
    quat *out = malloc(sizeof(quat));

    vec3 axis = {0, 1, 0};
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
    vec3 v1 = {M_PI / 2., 0.f, 0.f};
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
    quat q1 = {0.000000, 0.850904, 0.000000, 0.525322};
    float actual = quat_length(&q1);
    r2_assert("quat length is wrong", r2_equals(actual, 1.));
    return 0;
}

static char *test_quat_normalize()
{
    quat q1 = {.5f, .3f, .2f, .1f};
    quat *out = malloc(sizeof(quat));
    quat_normalize(&q1, out);
    r2_assert("quat normalize is wrong", r2_equals(out->x, .800641) && r2_equals(out->y, 0.480384) &&
                                             r2_equals(out->z, 0.320256) && r2_equals(out->w, 0.160128));
    free(out);
    return 0;
}

static char *test_quat_mul_quat()
{
    vec3 v1 = {0.f, M_PI / 2, 0.f};
    quat *q1 = malloc(sizeof(quat));
    quat_from_euler(&v1, q1);

    vec3 v2 = {M_PI / 4, 0.f, 0.f};
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
    //  --->x  1
    //  |
    //  |
    //  y
    // -1
    vec3 v1 = {0.f, 0.f, M_PI / 2};
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {1.f, 0.f, 0.f};
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
    vec3 v1 = {M_PI / 2, 0.f, 0.f};
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {0.f, 1.f, 0.f};
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
    vec3 v1 = {M_PI, 0.f, 0.f};
    quat *q = malloc(sizeof(quat));
    quat_from_euler(&v1, q);

    vec3 v = {0.f, 1.f, 0.f};
    quat *result = malloc(sizeof(vec3));

    quat_mul_vec3(q, &v, result);

    r2_assert("quat mul vec3 is wrong (y 180x)", r2_equals(result->x, 0.) && r2_equals(result->y, -1.) &&
                                                     r2_equals(result->z, 0.) && r2_equals(result->w, 0.));

    free(q);
    free(result);
    return 0;
}

static char *r2_maths_test()
{
    r2_run_test(test_vec2_add);
    r2_run_test(test_vec2_sub);
    r2_run_test(test_vec2_div);
    r2_run_test(test_vec2_div_zero);
    r2_run_test(test_vec2_div_vec2);
    r2_run_test(test_vec2_div_vec2_zero);
    r2_run_test(test_vec2_dot);

    r2_run_test(test_vec3_add);
    r2_run_test(test_vec3_mul_clobber);
    r2_run_test(test_vec3_cross);
    r2_run_test(test_vec3_to_array);
    r2_run_test(test_vec3_normalize);
    r2_run_test(test_vec3_length);

    r2_run_test(test_vec4_div);

    r2_run_test(test_quat_rot2q);
    r2_run_test(test_quat_from_euler);
    r2_run_test(test_quat_length);
    r2_run_test(test_quat_normalize);
    r2_run_test(test_quat_mul_quat);
    r2_run_test(test_quat_mul_vec3_x_90z);
    r2_run_test(test_quat_mul_vec3_y_90x);
    r2_run_test(test_quat_mul_vec3_y_180x);

    r2_run_test(test_new_vec4_ptr);
    return 0;
}
