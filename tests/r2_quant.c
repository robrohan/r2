#define R2_QUANT_IMPLEMENTATION
#include "../r2_quant.h"

#include <math.h>

#include "../r2_unit.h"

static float r2q_test_val(int i)
{
    return sinf((float)i * 0.9173f) * 97.0f;
}

static float r2q_max_abs(const float *a, int n)
{
    float m = 0.f;
    int i;
    for (i = 0; i < n; i++)
    {
        float v = fabsf(a[i]);
        if (v > m)
            m = v;
    }
    return m;
}

static const char *test_quantize_i8_roundtrip(void)
{
    float in[16], dq[16];
    int8_t q[16];
    int i;
    for (i = 0; i < 16; i++)
        in[i] = r2q_test_val(i);

    float scale = r2_quantize_i8(in, 16, q);
    r2_dequantize_i8(q, 16, scale, dq);

    for (i = 0; i < 16; i++)
        r2_assert("i8 round-trip error exceeds half a quant step", fabsf(in[i] - dq[i]) <= scale / 2.f + 1e-4f);
    return 0;
}

static const char *test_quantize_i16_roundtrip(void)
{
    float in[16], dq[16];
    int16_t q[16];
    int i;
    for (i = 0; i < 16; i++)
        in[i] = r2q_test_val(i);

    float scale = r2_quantize_i16(in, 16, q);
    r2_dequantize_i16(q, 16, scale, dq);

    for (i = 0; i < 16; i++)
        r2_assert("i16 round-trip error exceeds half a quant step", fabsf(in[i] - dq[i]) <= scale / 2.f + 1e-4f);
    return 0;
}

static const char *test_quantize_i4_roundtrip(void)
{
    float in[16], dq[16];
    uint8_t packed[8]; /* r2_i4_packed_bytes(16) == 8 */
    int i;
    for (i = 0; i < 16; i++)
        in[i] = r2q_test_val(i);

    float scale = r2_quantize_i4(in, 16, packed);
    r2_dequantize_i4(packed, 16, scale, dq);

    for (i = 0; i < 16; i++)
        r2_assert("i4 round-trip error exceeds half a quant step", fabsf(in[i] - dq[i]) <= scale / 2.f + 1e-4f);
    return 0;
}

static const char *test_quantize_i8_boundary(void)
{
    float in[4] = {0.f, 0.f, 50.f, 0.f};
    int8_t q[4];
    float scale = r2_quantize_i8(in, 4, q);
    r2_assert("i8 max-magnitude element should quantize to exactly QMAX", q[2] == R2_I8_QMAX);
    r2_assert("i8 scale should be max_abs/QMAX", fabsf(scale - 50.f / (float)R2_I8_QMAX) < 1e-6f);
    return 0;
}

static const char *test_quantize_i8_all_zero(void)
{
    float in[4] = {0.f, 0.f, 0.f, 0.f};
    int8_t q[4] = {1, 1, 1, 1};
    float scale = r2_quantize_i8(in, 4, q);
    r2_assert("all-zero input should produce scale 1.0", scale == 1.0f);
    r2_assert("all-zero input should produce all-zero output", q[0] == 0 && q[1] == 0 && q[2] == 0 && q[3] == 0);
    return 0;
}

static const char *test_quantize_i16_boundary(void)
{
    float in[4] = {0.f, -1234.5f, 0.f, 0.f};
    int16_t q[4];
    r2_quantize_i16(in, 4, q);
    r2_assert("i16 max-magnitude element should quantize to exactly -QMAX", q[1] == -R2_I16_QMAX);
    return 0;
}

static const char *test_i4_packed_bytes_sizing(void)
{
    r2_assert("packed_bytes(0)", r2_i4_packed_bytes(0) == 0);
    r2_assert("packed_bytes(1)", r2_i4_packed_bytes(1) == 1);
    r2_assert("packed_bytes(2)", r2_i4_packed_bytes(2) == 1);
    r2_assert("packed_bytes(3)", r2_i4_packed_bytes(3) == 2);
    r2_assert("packed_bytes(4)", r2_i4_packed_bytes(4) == 2);
    r2_assert("packed_bytes(5)", r2_i4_packed_bytes(5) == 3);
    return 0;
}

static const char *test_i4_pack_defensive_clamp(void)
{
    int8_t weird[2] = {8, -9}; /* out of the -7..7 contract */
    uint8_t packed[1];
    r2_pack_i4(weird, 2, packed);
    int8_t back[2];
    r2_unpack_i4(packed, 2, back);
    r2_assert("r2_pack_i4 should clamp +8 to +7", back[0] == R2_I4_QMAX);
    r2_assert("r2_pack_i4 should clamp -9 to -7", back[1] == R2_I4_QMIN);
    return 0;
}

static const char *test_i4_odd_n_roundtrip_no_leak(void)
{
    int8_t src[5] = {7, -7, 3, -2, 5};
    uint8_t packed[3];
    r2_pack_i4(src, 5, packed);
    packed[2] |= 0xF0u;

    int8_t back[5] = {0};
    r2_unpack_i4(packed, 5, back);
    r2_assert("i4 odd-n round-trip mismatch",
              back[0] == 7 && back[1] == -7 && back[2] == 3 && back[3] == -2 && back[4] == 5);
    return 0;
}

static const char *test_i4_single_element(void)
{
    int8_t src[1] = {5};
    uint8_t packed[1];
    r2_pack_i4(src, 1, packed);
    int8_t back[1] = {0};
    r2_unpack_i4(packed, 1, back);
    r2_assert("i4 n=1 round-trip mismatch", back[0] == 5);
    return 0;
}

static const char *test_vecn_dot_i8_vs_float_reference(void)
{
    float f1[32], f2[32];
    int8_t q1[32], q2[32];
    int i;
    for (i = 0; i < 32; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 100);
    }
    float s1 = r2_quantize_i8(f1, 32, q1);
    float s2 = r2_quantize_i8(f2, 32, q2);

    float ref = 0.f;
    for (i = 0; i < 32; i++)
        ref += f1[i] * f2[i];

    float got = vecn_dot_i8_f(q1, s1, q2, s2, 32);
    float tol = 0.5f * 32.f * (s1 * r2q_max_abs(f2, 32) + s2 * r2q_max_abs(f1, 32)) + 1.f;
    r2_assert("vecn_dot_i8_f diverges from float reference beyond tolerance", fabsf(got - ref) <= tol);
    return 0;
}

static const char *test_vecn_dot_i16_vs_float_reference(void)
{
    float f1[32], f2[32];
    int16_t q1[32], q2[32];
    int i;
    for (i = 0; i < 32; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 100);
    }
    float s1 = r2_quantize_i16(f1, 32, q1);
    float s2 = r2_quantize_i16(f2, 32, q2);

    float ref = 0.f;
    for (i = 0; i < 32; i++)
        ref += f1[i] * f2[i];

    float got = vecn_dot_i16_f(q1, s1, q2, s2, 32);
    float tol = 0.5f * 32.f * (s1 * r2q_max_abs(f2, 32) + s2 * r2q_max_abs(f1, 32)) + 1.f;
    r2_assert("vecn_dot_i16_f diverges from float reference beyond tolerance", fabsf(got - ref) <= tol);
    return 0;
}

static const char *test_vecn_dot_i4_vs_float_reference(void)
{
    float f1[32], f2[32];
    uint8_t q1[16], q2[16];
    int i;
    for (i = 0; i < 32; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 100);
    }
    float s1 = r2_quantize_i4(f1, 32, q1);
    float s2 = r2_quantize_i4(f2, 32, q2);

    float ref = 0.f;
    for (i = 0; i < 32; i++)
        ref += f1[i] * f2[i];

    float got = vecn_dot_i4_f(q1, s1, q2, s2, 32);
    float tol = 0.5f * 32.f * (s1 * r2q_max_abs(f2, 32) + s2 * r2q_max_abs(f1, 32)) + 5.f;
    r2_assert("vecn_dot_i4_f diverges from float reference beyond tolerance", fabsf(got - ref) <= tol);
    return 0;
}

static const char *test_mat_mul_i8_vs_float_reference(void)
{
    float f1[16], f2[16]; /* 4x4 * 4x4 */
    int8_t q1[16], q2[16];
    int i;
    for (i = 0; i < 16; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 50);
    }
    float s1 = r2_quantize_i8(f1, 16, q1);
    float s2 = r2_quantize_i8(f2, 16, q2);

    float ref[16] = {0};
    unsigned r, c, k;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            for (k = 0; k < 4; k++)
                ref[r * 4 + c] += f1[r * 4 + k] * f2[k * 4 + c];

    float got[16];
    mat_mul_i8_f(q1, s1, q2, s2, 4, 4, 4, 4, got);

    float tol = 0.5f * 4.f * (s1 * r2q_max_abs(f2, 16) + s2 * r2q_max_abs(f1, 16)) + 1.f;
    for (i = 0; i < 16; i++)
        r2_assert("mat_mul_i8_f diverges from float reference beyond tolerance", fabsf(got[i] - ref[i]) <= tol);
    return 0;
}

static const char *test_mat_mul_i16_vs_float_reference(void)
{
    float f1[16], f2[16]; /* 4x4 * 4x4 */
    int16_t q1[16], q2[16];
    int i;
    for (i = 0; i < 16; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 50);
    }
    float s1 = r2_quantize_i16(f1, 16, q1);
    float s2 = r2_quantize_i16(f2, 16, q2);

    float ref[16] = {0};
    unsigned r, c, k;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            for (k = 0; k < 4; k++)
                ref[r * 4 + c] += f1[r * 4 + k] * f2[k * 4 + c];

    float got[16];
    mat_mul_i16_f(q1, s1, q2, s2, 4, 4, 4, 4, got);

    float tol = 0.5f * 4.f * (s1 * r2q_max_abs(f2, 16) + s2 * r2q_max_abs(f1, 16)) + 1.f;
    for (i = 0; i < 16; i++)
        r2_assert("mat_mul_i16_f diverges from float reference beyond tolerance", fabsf(got[i] - ref[i]) <= tol);

    int64_t raw[16];
    mat_mul_i16(q1, q2, 4, 4, 4, 4, raw);
    for (i = 0; i < 16; i++)
    {
        float rescaled = (float)raw[i] * s1 * s2;
        r2_assert("mat_mul_i16 raw accumulator disagrees with mat_mul_i16_f", fabsf(rescaled - got[i]) < 1e-3f);
    }
    return 0;
}

static const char *test_mat_mul_i4_vs_float_reference(void)
{
    float f1[16], f2[16]; /* 4x4 * 4x4 */
    uint8_t q1[8], q2[8];
    int i;
    for (i = 0; i < 16; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 50);
    }
    float s1 = r2_quantize_i4(f1, 16, q1);
    float s2 = r2_quantize_i4(f2, 16, q2);

    float ref[16] = {0};
    unsigned r, c, k;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            for (k = 0; k < 4; k++)
                ref[r * 4 + c] += f1[r * 4 + k] * f2[k * 4 + c];

    int32_t raw[16];
    mat_mul_i4(q1, q2, 4, 4, 4, 4, raw);

    float scale = s1 * s2;
    float tol = 0.5f * 4.f * (s1 * r2q_max_abs(f2, 16) + s2 * r2q_max_abs(f1, 16)) + 5.f;
    for (i = 0; i < 16; i++)
    {
        float got = (float)raw[i] * scale;
        r2_assert("mat_mul_i4 diverges from float reference beyond tolerance", fabsf(got - ref[i]) <= tol);
    }
    return 0;
}

static const char *test_vecn_dot_i16_int32_would_overflow(void)
{
    int16_t v[8] = {32767, -32767, 32767, -32767, 32767, -32767, 32767, -32767};
    int64_t got = vecn_dot_i16(v, v, 8);
    int64_t expect = 8LL * 32767LL * 32767LL;
    r2_assert("vecn_dot_i16 does not match the int64-widened reference (overflow?)", got == expect);
    r2_assert("expected value itself must exceed INT32_MAX to be a meaningful regression test", expect > 2147483647LL);
    return 0;
}

static const char *test_mat_mul_i8_requant_chaining(void)
{
    float f1[16], f2[16];
    int8_t q1[16], q2[16];
    int i;
    for (i = 0; i < 16; i++)
    {
        f1[i] = r2q_test_val(i);
        f2[i] = r2q_test_val(i + 50);
    }
    float s1 = r2_quantize_i8(f1, 16, q1);
    float s2 = r2_quantize_i8(f2, 16, q2);

    float unrequant[16];
    mat_mul_i8_f(q1, s1, q2, s2, 4, 4, 4, 4, unrequant);

    int8_t requantized[16];
    float out_scale;
    mat_mul_i8_requant(q1, s1, q2, s2, 4, 4, 4, 4, requantized, &out_scale);

    float dq[16];
    r2_dequantize_i8(requantized, 16, out_scale, dq);

    for (i = 0; i < 16; i++)
        r2_assert("requantize->dequantize chain diverges beyond one requant step",
                  fabsf(dq[i] - unrequant[i]) <= out_scale + 1e-3f);
    return 0;
}

static const char *r2_quant_test(void)
{
    r2_run_test(test_quantize_i8_roundtrip);
    r2_run_test(test_quantize_i16_roundtrip);
    r2_run_test(test_quantize_i4_roundtrip);

    r2_run_test(test_quantize_i8_boundary);
    r2_run_test(test_quantize_i8_all_zero);
    r2_run_test(test_quantize_i16_boundary);

    r2_run_test(test_i4_packed_bytes_sizing);
    r2_run_test(test_i4_pack_defensive_clamp);
    r2_run_test(test_i4_odd_n_roundtrip_no_leak);
    r2_run_test(test_i4_single_element);

    r2_run_test(test_vecn_dot_i8_vs_float_reference);
    r2_run_test(test_vecn_dot_i16_vs_float_reference);
    r2_run_test(test_vecn_dot_i4_vs_float_reference);
    r2_run_test(test_mat_mul_i8_vs_float_reference);
    r2_run_test(test_mat_mul_i16_vs_float_reference);
    r2_run_test(test_mat_mul_i4_vs_float_reference);

    r2_run_test(test_vecn_dot_i16_int32_would_overflow);

    r2_run_test(test_mat_mul_i8_requant_chaining);

    return 0;
}
