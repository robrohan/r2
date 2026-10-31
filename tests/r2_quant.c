#define R2_QUANT_IMPLEMENTATION
#include "../r2_quant.h"

#include <math.h>
#include <stdlib.h>

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

///////////////////////////////////////////////////////////////
// Exact integer checks against a widened reference. Lengths 0..100 hit
// every SIMD main-loop / scalar-tail split, and the values include the
// extremes. Run the suite with SIMD_FORCE_SCALAR=1 too so both the SIMD
// and the scalar kernels are checked.

static uint32_t r2q_seed = 0xC0FFEE11u;
static uint32_t r2q_rand(void)
{
    r2q_seed ^= r2q_seed << 13;
    r2q_seed ^= r2q_seed >> 17;
    r2q_seed ^= r2q_seed << 5;
    return r2q_seed;
}

/* Random int8 in [-127, 127]; one in four is an extreme */
static int8_t r2q_rand_i8(void)
{
    uint32_t r = r2q_rand();
    if ((r & 3) == 0)
        return (r & 4) ? R2_I8_QMAX : R2_I8_QMIN;
    return (int8_t)((int32_t)(r >> 8) % 255 - 127);
}

static int16_t r2q_rand_i16(void)
{
    uint32_t r = r2q_rand();
    if ((r & 3) == 0)
        return (r & 4) ? R2_I16_QMAX : R2_I16_QMIN;
    return (int16_t)((int32_t)(r >> 8) % 65535 - 32767);
}

static int8_t r2q_rand_i4(void)
{
    return (int8_t)((int32_t)(r2q_rand() >> 8) % 15 - 7);
}

static const char *test_vecn_dot_i8_exact_all_lengths(void)
{
    /* +1 offsets so the SIMD loads are unaligned too */
    int8_t a[129], b[129];
    for (int n = 0; n <= 128; n++)
    {
        for (int rep = 0; rep < 4; rep++)
        {
            int64_t want = 0;
            for (int i = 0; i < n + 1; i++)
            {
                a[i] = r2q_rand_i8();
                b[i] = r2q_rand_i8();
            }
            for (int i = 0; i < n; i++)
                want += (int64_t)a[i + 1] * b[i + 1];
            r2_assert("vecn_dot_i8 differs from reference", vecn_dot_i8(a + 1, b + 1, n) == want);
        }
    }
    return 0;
}

static const char *test_vecn_dot_i8_all_min(void)
{
    /* -128 is outside the quantizer's range but a legal int8_t input;
       -128 * -128 = 16384 must still accumulate exactly */
    int8_t a[100];
    for (int i = 0; i < 100; i++)
        a[i] = -128;
    for (int n = 0; n <= 100; n++)
        r2_assert("vecn_dot_i8 wrong for -128", vecn_dot_i8(a, a, n) == 16384 * n);
    return 0;
}

static const char *test_vecn_dot_i16_exact_all_lengths(void)
{
    int16_t a[101], b[101];
    for (int n = 0; n <= 100; n++)
    {
        for (int rep = 0; rep < 4; rep++)
        {
            int64_t want = 0;
            for (int i = 0; i < n + 1; i++)
            {
                a[i] = r2q_rand_i16();
                b[i] = r2q_rand_i16();
            }
            for (int i = 0; i < n; i++)
                want += (int64_t)a[i + 1] * b[i + 1];
            r2_assert("vecn_dot_i16 differs from reference", vecn_dot_i16(a + 1, b + 1, n) == want);
        }
    }
    return 0;
}

static const char *test_vecn_dot_i16_all_min(void)
{
    /* -32768 is a legal int16_t. The SSE/AVX kernels use madd_epi16,
       which adds two products into an int32 lane: 2 * (-32768)^2 = 2^31
       wraps to INT32_MIN. */
    int16_t a[32];
    for (int i = 0; i < 32; i++)
        a[i] = -32768;
    for (int n = 0; n <= 32; n++)
        r2_assert("vecn_dot_i16 wrong for -32768 (madd_epi16 overflow?)",
                  vecn_dot_i16(a, a, n) == (int64_t)1073741824 * n);
    return 0;
}

static const char *test_dot_f_is_scaled_raw(void)
{
    int8_t a8[37], b8[37];
    int16_t a16[37], b16[37];
    int8_t a4[37], b4[37];
    uint8_t p1[19], p2[19];
    for (int i = 0; i < 37; i++)
    {
        a8[i] = r2q_rand_i8();
        b8[i] = r2q_rand_i8();
        a16[i] = r2q_rand_i16();
        b16[i] = r2q_rand_i16();
        a4[i] = r2q_rand_i4();
        b4[i] = r2q_rand_i4();
    }
    r2_pack_i4(a4, 37, p1);
    r2_pack_i4(b4, 37, p2);
    r2_assert("vecn_dot_i8_f != raw * s1 * s2",
              vecn_dot_i8_f(a8, 0.5f, b8, 0.25f, 37) == (float)vecn_dot_i8(a8, b8, 37) * 0.5f * 0.25f);
    r2_assert("vecn_dot_i16_f != raw * s1 * s2",
              vecn_dot_i16_f(a16, 0.5f, b16, 0.25f, 37) == (float)vecn_dot_i16(a16, b16, 37) * 0.5f * 0.25f);
    r2_assert("vecn_dot_i4_f != raw * s1 * s2",
              vecn_dot_i4_f(p1, 0.5f, p2, 0.25f, 37) == (float)vecn_dot_i4(p1, p2, 37) * 0.5f * 0.25f);
    return 0;
}

static const char *test_vecn_dot_i4_exact_all_lengths(void)
{
    int8_t a[64], b[64];
    uint8_t p1[33], p2[33];
    for (int n = 0; n <= 64; n++)
    {
        int32_t want = 0;
        for (int i = 0; i < n; i++)
        {
            a[i] = r2q_rand_i4();
            b[i] = r2q_rand_i4();
            want += (int32_t)a[i] * b[i];
        }
        /* junk in the unused high nibble of an odd-n tail must not count */
        p1[n >> 1] = 0xF0;
        p2[n >> 1] = 0xF0;
        r2_pack_i4(a, n, p1);
        r2_pack_i4(b, n, p2);
        if (n & 1)
        {
            p1[n >> 1] |= 0x70;
            p2[n >> 1] |= 0x70;
        }
        r2_assert("vecn_dot_i4 differs from reference", vecn_dot_i4(p1, p2, n) == want);
    }
    return 0;
}

static const char *test_i4_unpack_every_byte(void)
{
    /* every byte decodes to two sign-extended nibbles, low nibble first */
    for (int byte = 0; byte < 256; byte++)
    {
        uint8_t p = (uint8_t)byte;
        int8_t out[2];
        int lo = byte & 0xF, hi = byte >> 4;
        r2_unpack_i4(&p, 2, out);
        r2_assert("unpack low nibble wrong", out[0] == (lo >= 8 ? lo - 16 : lo));
        r2_assert("unpack high nibble wrong", out[1] == (hi >= 8 ? hi - 16 : hi));
    }
    return 0;
}

static const char *test_i4_pack_every_pair(void)
{
    /* every pair in -8..7 round-trips, with -8 clamped to -7 */
    for (int lo = -8; lo <= 7; lo++)
    {
        for (int hi = -8; hi <= 7; hi++)
        {
            int8_t in[2] = {(int8_t)lo, (int8_t)hi}, out[2];
            uint8_t p;
            r2_pack_i4(in, 2, &p);
            r2_unpack_i4(&p, 2, out);
            r2_assert("pack/unpack low wrong", out[0] == (lo < -7 ? -7 : lo));
            r2_assert("pack/unpack high wrong", out[1] == (hi < -7 ? -7 : hi));
        }
    }
    return 0;
}

static const char *test_quantize_i4_odd_n(void)
{
    float in[5] = {7.f, -3.5f, 0.f, 1.f, -7.f};
    float out[5];
    uint8_t p[3];
    float s = r2_quantize_i4(in, 5, p);
    r2_assert("quantize_i4 scale wrong", s == 1.f);
    r2_dequantize_i4(p, 5, s, out);
    /* -3.5 rounds away from zero to -4 */
    r2_assert("quantize_i4 odd n wrong",
              out[0] == 7.f && out[1] == -4.f && out[2] == 0.f && out[3] == 1.f && out[4] == -7.f);
    r2_assert("quantize_i4 odd n high nibble not zero", (p[2] >> 4) == 0);
    return 0;
}

static const char *test_quantize_rounding_and_error(void)
{
    /* every value comes back within half a step, for each width */
    float in[257], dq[257];
    int8_t q8[257];
    int16_t q16[257];
    uint8_t q4[129];
    for (int i = 0; i < 257; i++)
        in[i] = r2q_test_val(i) * ((i % 3) ? 1.f : 0.001f);

    float sc8 = r2_quantize_i8(in, 257, q8);
    r2_dequantize_i8(q8, 257, sc8, dq);
    for (int i = 0; i < 257; i++)
        r2_assert("i8 error > scale/2", fabsf(dq[i] - in[i]) <= sc8 * 0.5f + 1e-5f);

    float s16 = r2_quantize_i16(in, 257, q16);
    r2_dequantize_i16(q16, 257, s16, dq);
    for (int i = 0; i < 257; i++)
        r2_assert("i16 error > scale/2", fabsf(dq[i] - in[i]) <= s16 * 0.5f + 1e-5f);

    float s4 = r2_quantize_i4(in, 257, q4);
    r2_dequantize_i4(q4, 257, s4, dq);
    for (int i = 0; i < 257; i++)
        r2_assert("i4 error > scale/2", fabsf(dq[i] - in[i]) <= s4 * 0.5f + 1e-4f);
    return 0;
}

static const char *test_quantize_ties_away_from_zero(void)
{
    /* scale = 127/127 = 1, so x.5 values are exact ties */
    float in[5] = {127.f, 0.5f, -0.5f, 2.5f, -2.5f};
    int8_t q[5];
    r2_quantize_i8(in, 5, q);
    r2_assert("tie rounding wrong", q[1] == 1 && q[2] == -1 && q[3] == 3 && q[4] == -3);
    return 0;
}

static const char *test_quantize_empty(void)
{
    float in[1] = {5.f};
    int8_t q8[1] = {9};
    int16_t q16[1] = {9};
    uint8_t q4[1] = {9};
    r2_assert("i8 n=0 scale", r2_quantize_i8(in, 0, q8) == 1.f);
    r2_assert("i16 n=0 scale", r2_quantize_i16(in, 0, q16) == 1.f);
    r2_assert("i4 n=0 scale", r2_quantize_i4(in, 0, q4) == 1.f);
    r2_assert("n=0 wrote output", q8[0] == 9 && q16[0] == 9 && q4[0] == 9);
    return 0;
}

static const char *test_mat_mul_int_exact(void)
{
    /* every integer matmul, several shapes, against a widened reference */
    static const unsigned shapes[][3] = {{1, 1, 1}, {1, 7, 1}, {3, 5, 2}, {4, 4, 4}, {7, 3, 9}, {2, 33, 3}};
    int8_t a8[300], b8[300];
    int16_t a16[300], b16[300];
    int8_t a4[300], b4[300];
    uint8_t p1[150], p2[150];
    int32_t o32[100];
    int64_t o64[100];
    for (unsigned s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++)
    {
        unsigned r = shapes[s][0], c = shapes[s][1], k = shapes[s][2];
        for (unsigned i = 0; i < r * c; i++)
        {
            a8[i] = r2q_rand_i8();
            a16[i] = r2q_rand_i16();
            a4[i] = r2q_rand_i4();
        }
        for (unsigned i = 0; i < c * k; i++)
        {
            b8[i] = r2q_rand_i8();
            b16[i] = r2q_rand_i16();
            b4[i] = r2q_rand_i4();
        }
        r2_pack_i4(a4, (int)(r * c), p1);
        r2_pack_i4(b4, (int)(c * k), p2);
        for (unsigned i = 0; i < r; i++)
        {
            for (unsigned j = 0; j < k; j++)
            {
                int64_t w8 = 0, w16 = 0, w4 = 0;
                for (unsigned x = 0; x < c; x++)
                {
                    w8 += (int64_t)a8[i * c + x] * b8[x * k + j];
                    w16 += (int64_t)a16[i * c + x] * b16[x * k + j];
                    w4 += (int64_t)a4[i * c + x] * b4[x * k + j];
                }
                mat_mul_i8(a8, b8, r, c, c, k, o32);
                r2_assert("mat_mul_i8 differs from reference", o32[i * k + j] == w8);
                mat_mul_i16(a16, b16, r, c, c, k, o64);
                r2_assert("mat_mul_i16 differs from reference", o64[i * k + j] == w16);
                mat_mul_i4(p1, p2, r, c, c, k, o32);
                r2_assert("mat_mul_i4 differs from reference", o32[i * k + j] == w4);
            }
        }
    }
    return 0;
}

static const char *test_mat_mul_f_is_scaled_raw(void)
{
    int8_t a8[15], b8[10];
    int16_t a16[15], b16[10];
    int32_t raw32[6];
    int64_t raw64[6];
    float f[6];
    for (int i = 0; i < 15; i++)
    {
        a8[i] = r2q_rand_i8();
        a16[i] = r2q_rand_i16();
    }
    for (int i = 0; i < 10; i++)
    {
        b8[i] = r2q_rand_i8();
        b16[i] = r2q_rand_i16();
    }
    mat_mul_i8(a8, b8, 3, 5, 5, 2, raw32);
    mat_mul_i8_f(a8, 0.5f, b8, 0.125f, 3, 5, 5, 2, f);
    for (int i = 0; i < 6; i++)
        r2_assert("mat_mul_i8_f != raw * s1 * s2", f[i] == (float)raw32[i] * (0.5f * 0.125f));
    mat_mul_i16(a16, b16, 3, 5, 5, 2, raw64);
    mat_mul_i16_f(a16, 0.5f, b16, 0.125f, 3, 5, 5, 2, f);
    for (int i = 0; i < 6; i++)
        r2_assert("mat_mul_i16_f != raw * s1 * s2", f[i] == (float)raw64[i] * (0.5f * 0.125f));
    return 0;
}

static const char *test_mat_mul_size_mismatch_untouched(void)
{
    /* c1 != r2: every matmul leaves out alone */
    int8_t a8[6] = {1, 2, 3, 4, 5, 6};
    int16_t a16[6] = {1, 2, 3, 4, 5, 6};
    uint8_t p[3] = {0x11, 0x11, 0x11};
    int32_t o32[4] = {-1, -1, -1, -1};
    int64_t o64[4] = {-1, -1, -1, -1};
    float of[4] = {-1, -1, -1, -1};
    int8_t o8[4] = {-1, -1, -1, -1};
    float out_scale = 0.f;
    mat_mul_i8(a8, a8, 2, 3, 2, 3, o32);
    mat_mul_i4(p, p, 2, 3, 2, 3, o32);
    mat_mul_i16(a16, a16, 2, 3, 2, 3, o64);
    mat_mul_i8_f(a8, 1.f, a8, 1.f, 2, 3, 2, 3, of);
    mat_mul_i16_f(a16, 1.f, a16, 1.f, 2, 3, 2, 3, of);
    mat_mul_i8_requant(a8, 1.f, a8, 1.f, 2, 3, 2, 3, o8, &out_scale);
    for (int i = 0; i < 4; i++)
    {
        r2_assert("mismatch wrote int32 out", o32[i] == -1);
        r2_assert("mismatch wrote int64 out", o64[i] == -1);
        r2_assert("mismatch wrote float out", of[i] == -1.f);
        r2_assert("mismatch wrote int8 out", o8[i] == -1);
    }
    r2_assert("requant mismatch scale should be 1", out_scale == 1.f);
    return 0;
}

static const char *test_mat_mul_i8_requant_zero(void)
{
    /* an all-zero product gives scale 1 and zeros, not a divide by zero */
    int8_t a[4] = {0, 0, 0, 0}, b[4] = {1, 2, 3, 4}, out[4] = {9, 9, 9, 9};
    float out_scale = 0.f;
    mat_mul_i8_requant(a, 1.f, b, 1.f, 2, 2, 2, 2, out, &out_scale);
    r2_assert("requant zero scale wrong", out_scale == 1.f);
    r2_assert("requant zero output wrong", out[0] == 0 && out[1] == 0 && out[2] == 0 && out[3] == 0);
    return 0;
}

static const char *test_mat_mul_i8_requant_hits_qmax(void)
{
    /* the largest-magnitude output is requantized to exactly +-127 */
    int8_t a[4], b[4], out[4];
    float out_scale;
    int max_abs = 0;
    for (int i = 0; i < 4; i++)
    {
        a[i] = r2q_rand_i8();
        b[i] = r2q_rand_i8();
    }
    mat_mul_i8_requant(a, 1.f, b, 1.f, 2, 2, 2, 2, out, &out_scale);
    for (int i = 0; i < 4; i++)
        if (abs(out[i]) > max_abs)
            max_abs = abs(out[i]);
    r2_assert("requant max output should be 127", max_abs == R2_I8_QMAX);
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

    // exact integer kernels (SIMD and scalar)
    r2_run_test(test_vecn_dot_i8_exact_all_lengths);
    r2_run_test(test_vecn_dot_i8_all_min);
    r2_run_test(test_vecn_dot_i16_exact_all_lengths);
    r2_run_test(test_vecn_dot_i16_all_min);
    r2_run_test(test_vecn_dot_i4_exact_all_lengths);
    r2_run_test(test_dot_f_is_scaled_raw);
    r2_run_test(test_mat_mul_int_exact);
    r2_run_test(test_mat_mul_f_is_scaled_raw);
    r2_run_test(test_mat_mul_size_mismatch_untouched);
    r2_run_test(test_mat_mul_i8_requant_zero);
    r2_run_test(test_mat_mul_i8_requant_hits_qmax);

    // i4 packing, every value
    r2_run_test(test_i4_unpack_every_byte);
    r2_run_test(test_i4_pack_every_pair);

    // quantizers
    r2_run_test(test_quantize_i4_odd_n);
    r2_run_test(test_quantize_rounding_and_error);
    r2_run_test(test_quantize_ties_away_from_zero);
    r2_run_test(test_quantize_empty);

    return 0;
}
