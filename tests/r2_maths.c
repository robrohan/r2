#include "../r2_maths.h"
#include "../r2_unit.h"
#include <stdio.h>
#include <stdlib.h>

static char *test_vec2_add()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {10.f, 10.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
    vec2_add(&v1, &v2, v3);
    r2_assert("Vec2 add failed", (v3->x == 20.f && v3->y == 20.f));
    free(v3);
    return 0;
}

static char *test_vec2_sub()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {10.f, 10.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
    vec2_sub(&v1, &v2, v3);
    r2_assert("Vec2 sub failed", (v3->x == 0.f && v3->y == 0.f));
    free(v3);
    return 0;
}

static char *test_vec2_div()
{
    vec2 v1 = {10.f, 10.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
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
    vec2 *v3 = calloc(1, sizeof(vec2));
    vec2_div_vec2(&v1, &v2, v3);
    r2_assert("Vec2 div failed", (v3->x == 2.f && v3->y == 5.f));
    free(v3);
    return 0;
}

static char *test_vec2_div_vec2_zero()
{
    vec2 v1 = {10.f, 10.f};
    vec2 v2 = {0.f, 0.f};
    vec2 *v3 = calloc(1, sizeof(vec2));
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
    vec4 *out = calloc(1, sizeof(vec4));
    vec3_add(&v1, &v2, out);
    r2_assert("vec3 add is wrong", (out->x == 0.f && out->y == 0.f && out->z == 0.f));
    return 0;
}

static char *test_vec3_cross()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3 v2 = {-30.f, 30.f, -30.f};
    vec4 *out = calloc(1, sizeof(vec4));
    vec3_cross(&v1, &v2, out);
    r2_assert("vec3 cross is wrong", (out->x == -180.f && out->y == 0.f && out->z == 180.f));
    return 0;
}

static char *test_vec3_equal()
{
    vec3 v1 = {3.f, 3.f, 3.f};
    vec3 v2 = {-30.f, 30.f, -30.f};
    r2_assert("vec3 equal is wrong", !vec3_equal(&v1, &v2));
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

static char *test_vec4_div()
{
    vec3 v1 = {10.f, 10.f, 12.f, 12.f};
    vec4 *out = calloc(1, sizeof(vec4));
    vec4_div(&v1, 2., out);
    r2_assert("vec4 div is wrong", out->x == 5.f && out->y == 5.f && out->z == 6.f && out->w == 6.f);
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
    r2_run_test(test_vec3_cross);
    r2_run_test(test_vec3_to_array);

    r2_run_test(test_vec4_div);

    r2_run_test(test_new_vec4_ptr);
    return 0;
}
