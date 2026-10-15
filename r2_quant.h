
/* r2_quant - v0.0 - public domain quantized (int8/int16/int4) inference math
    no warranty implied; use at your own risk

    Built in the style of: https://github.com/nothings/stb

    Written for quantized neural-net inference on constrained/embedded
    targets (the motivating case is an ESP32-S3/Xtensa CNN). Every function
    here operates on flat caller-owned buffers plus an element count `n`,
    mirroring r2_maths.h's vecn_* generic layer - it deliberately does NOT
    depend on r2_maths.h or its vec2/vec3/vec4/mat types. Nothing in this
    file allocates memory; all buffers are caller-owned.

    Quantization scheme: symmetric only (signed range, single per-tensor
    float scale, implicit zero-point 0). int8/int16 avoid the most-negative
    two's-complement value (-128/-32768) so `scale = max_abs / QMAX` stays
    exact and reversible in both directions, and so `-q` is always safely
    representable. int4 (packed 2 values/byte) follows the same convention,
    using -7..7 rather than the full -8..7, to keep negation/round-trip
    logic consistent across all three widths.

    Accumulator widths matter: int8*int8 sums safely into int32_t (max
    product 127*127=16129). int16*int16 does NOT fit int32_t - two products
    near +/-32767 already overflow it (32767*32767 ~= 1.07e9, and int32_t
    maxes out at ~2.1e9) - so the int16 kernels use int64_t accumulators.
    int4*int4 (max product 49) is safe in int32_t for any realistic width.

    Do this:
       #define R2_QUANT_IMPLEMENTATION
    before you include this file in *one* C or C++ file to create the
    implementation.

    You can then include without the define to just use the declarations.

LICENSE
    See r2_maths.h for license information (this file is released under
    the same terms).
*/

#ifndef R2_QUANT_H
#define R2_QUANT_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#define R2_I8_QMIN (-127)
#define R2_I8_QMAX (127)
#define R2_I16_QMIN (-32767)
#define R2_I16_QMAX (32767)
#define R2_I4_QMIN (-7)
#define R2_I4_QMAX (7)

#ifndef R2_QUANT_EPSILON
#define R2_QUANT_EPSILON 1e-8f
#endif

    /** Quantize `in[0..n)` into `out`, returns the scale used (max_abs/127, or 1.0 for an all-zero input). */
    static float r2_quantize_i8(const float *in, int n, int8_t *out);
    /** Dequantize `in[0..n)` (with the given scale) into `out`. */
    static void r2_dequantize_i8(const int8_t *in, int n, float scale, float *out);
    /** Raw int32 dot product - caller applies scales. Safe for any n (max product 16129). */
    static int32_t vecn_dot_i8(const int8_t *v1, const int8_t *v2, int n);
    /** Dot product rescaled to float via s1*s2. */
    static float vecn_dot_i8_f(const int8_t *v1, float s1, const int8_t *v2, float s2, int n);
    /** m1(r1 x c1) * m2(r2 x c2) -> out(r1 x c2), raw int32 accumulator. Requires c1 == r2. */
    static void mat_mul_i8(const int8_t *m1, const int8_t *m2, unsigned r1, unsigned c1, unsigned r2, unsigned c2,
                           int32_t *out);
    /** Same as mat_mul_i8, rescaled to float via s1*s2. */
    static void mat_mul_i8_f(const int8_t *m1, float s1, const int8_t *m2, float s2, unsigned r1, unsigned c1,
                             unsigned r2, unsigned c2, float *out);
    /** Same as mat_mul_i8, but requantizes the result back to int8 (for chaining into the next layer). */
    static void mat_mul_i8_requant(const int8_t *m1, float s1, const int8_t *m2, float s2, unsigned r1, unsigned c1,
                                   unsigned r2, unsigned c2, int8_t *out, float *out_scale);

    /** Quantize `in[0..n)` into `out`, returns the scale used (max_abs/32767, or 1.0 for an all-zero input). */
    static float r2_quantize_i16(const float *in, int n, int16_t *out);
    /** Dequantize `in[0..n)` (with the given scale) into `out`. */
    static void r2_dequantize_i16(const int16_t *in, int n, float scale, float *out);
    /** Raw int64 dot product - caller applies scales. */
    static int64_t vecn_dot_i16(const int16_t *v1, const int16_t *v2, int n);
    /** Dot product rescaled to float via s1*s2. */
    static float vecn_dot_i16_f(const int16_t *v1, float s1, const int16_t *v2, float s2, int n);
    /** m1(r1 x c1) * m2(r2 x c2) -> out(r1 x c2), raw int64 accumulator. Requires c1 == r2. */
    static void mat_mul_i16(const int16_t *m1, const int16_t *m2, unsigned r1, unsigned c1, unsigned r2, unsigned c2,
                            int64_t *out);
    /** Same as mat_mul_i16, rescaled to float via s1*s2. */
    static void mat_mul_i16_f(const int16_t *m1, float s1, const int16_t *m2, float s2, unsigned r1, unsigned c1,
                              unsigned r2, unsigned c2, float *out);

    /* ---------------- u4 groups (asymmetric 4-bit weights) ----------------
       Unsigned 4-bit codes (0..15) for weights stored with a per-group
       scale and zero point: w = s_g * (q - z_g). The kernel only computes
       the raw per-group sums sum(q * x) against int8 activations; the
       caller applies s_g, z_g (via sum(x) per group) and any row scale.
       Layout, per 32-element block: 16 bytes, byte j = q[j] | q[j+16] << 4,
       so a block's low nibbles are group 2b and its high nibbles group 2b+1
       (one SIMD register each after a mask/shift, no shuffles). */

    /** Pack `n` (multiple of 32) codes 0..15 into n/2 bytes, block layout above. */
    static void r2_pack_u4g16(const uint8_t *q, int n, uint8_t *out);
    /** Inverse of r2_pack_u4g16. */
    static void r2_unpack_u4g16(const uint8_t *packed, int n, uint8_t *out);
    /** One 32-element block of r2_dot_u4g16: *d0, *d1 = the two group sums. For fused
        kernels that apply per-group scales in the same loop (no store/reload, no call).
        PIE: packed and x must be 16-byte aligned. */
    static void r2_dot_u4g16_block(const uint8_t *packed, const int8_t *x, int32_t *d0, int32_t *d1);
    /** out[g] = sum of q[i] * x[i] over each group of 16 (n/16 outputs, n multiple of 32). */
    static void r2_dot_u4g16(const uint8_t *packed, const int8_t *x, int n, int32_t *out);

    /** Number of packed bytes needed to hold `n` int4 elements: (n+1)/2. */
    static size_t r2_i4_packed_bytes(int n);
    /** Pack `n` already-quantized int4 values (each defensively clamped to -7..7) into `out`. */
    static void r2_pack_i4(const int8_t *in, int n, uint8_t *out);
    /** Unpack `n` int4 values from `packed` into `out` (as int8_t, sign-extended). */
    static void r2_unpack_i4(const uint8_t *packed, int n, int8_t *out);
    /** Quantize `in[0..n)`, pack into `out`, returns the scale used. */
    static float r2_quantize_i4(const float *in, int n, uint8_t *out);
    /** Dequantize `n` packed int4 values (with the given scale) into `out`. */
    static void r2_dequantize_i4(const uint8_t *packed, int n, float scale, float *out);
    /** Raw int32 dot product over two packed int4 buffers - caller applies scales. */
    static int32_t vecn_dot_i4(const uint8_t *packed1, const uint8_t *packed2, int n);
    /** Dot product rescaled to float via s1*s2. */
    static float vecn_dot_i4_f(const uint8_t *packed1, float s1, const uint8_t *packed2, float s2, int n);
    /** m1(r1 x c1) * m2(r2 x c2) (both packed int4) -> out(r1 x c2), raw int32 accumulator. Requires c1 == r2. */
    static void mat_mul_i4(const uint8_t *packed_m1, const uint8_t *packed_m2, unsigned r1, unsigned c1, unsigned r2,
                           unsigned c2, int32_t *out);

#ifdef R2_QUANT_IMPLEMENTATION

#include <math.h>

#include "r2_simd.h"

    /* round-to-nearest, ties away from zero (avoids depending on lrintf) */
    static int32_t r2__round_i32(float x)
    {
        return (int32_t)(x >= 0.f ? x + 0.5f : x - 0.5f);
    }

    static float r2_quantize_i8(const float *in, int n, int8_t *out)
    {
        float max_abs = 0.f;
        int i;
        for (i = 0; i < n; i++)
        {
            float a = fabsf(in[i]);
            if (a > max_abs)
                max_abs = a;
        }
        float scale = (max_abs < R2_QUANT_EPSILON) ? 1.0f : (max_abs / (float)R2_I8_QMAX);
        for (i = 0; i < n; i++)
        {
            int32_t qi = r2__round_i32(in[i] / scale);
            if (qi > R2_I8_QMAX)
                qi = R2_I8_QMAX;
            if (qi < R2_I8_QMIN)
                qi = R2_I8_QMIN;
            out[i] = (int8_t)qi;
        }
        return scale;
    }

    static void r2_dequantize_i8(const int8_t *in, int n, float scale, float *out)
    {
        int i;
        for (i = 0; i < n; i++)
            out[i] = (float)in[i] * scale;
    }

#if defined(R2_SIMD_XTENSA_PIE)
    /* ESP32-S3 PIE ("cop_ai", coprocessor 3) int8 dot product. The caller
       must have set CPENABLE bit 3 (0x8) or the first EE.* instruction
       raises Coprocessor3Disabled.

       EE.VLD.128 ignores address bits [3:0], so the SIMD path only runs
       when both pointers are 16-byte aligned; anything else (and any tail
       past the last 32-byte step) goes through the scalar loop, so the
       usual (v1, v2, n) contract holds for every input. The accumulator
       is the 40-bit ACCX; ACCX_0 (its low 32 bits) is exact for any sum
       that fits int32_t, which every caller of an int8 dot already needs.

       A branch loop rather than LOOPNEZ on purpose: Xtensa zero-overhead
       loops don't nest, and once this is inlined into a row loop GCC may
       have turned that row loop into a hardware loop it doesn't know this
       asm clobbers. Verified bit-exact against the scalar loop on silicon
       (Chestnut apps/alanbench, ESP32-S3 rev 0.2). */
    static int32_t r2_xtensa_pie_dot_i8(const int8_t *v1, const int8_t *v2, int n)
    {
        int32_t sum = 0;
        int i = 0;
        if (n >= 32 && ((((uintptr_t)v1) | ((uintptr_t)v2)) & 15u) == 0)
        {
            const int8_t *a = v1, *b = v2;
            int pairs = n >> 5; /* 32-byte steps: two 16-lane MACs each */
            __asm__ volatile(
                "ee.zero.accx\n"
                "ee.vld.128.ip q0, %[a], 16\n"
                "ee.vld.128.ip q1, %[b], 16\n"
                "addi %[k], %[k], -1\n"
                "beqz %[k], 2f\n"
                "1:\n"
                "ee.vld.128.ip q2, %[a], 16\n"
                "ee.vld.128.ip q3, %[b], 16\n"
                "ee.vmulas.s8.accx q0, q1\n"
                "ee.vld.128.ip q0, %[a], 16\n"
                "ee.vld.128.ip q1, %[b], 16\n"
                "ee.vmulas.s8.accx q2, q3\n"
                "addi %[k], %[k], -1\n"
                "bnez %[k], 1b\n"
                "2:\n"
                "ee.vld.128.ip q2, %[a], 16\n"
                "ee.vld.128.ip q3, %[b], 16\n"
                "ee.vmulas.s8.accx q0, q1\n"
                "ee.vmulas.s8.accx q2, q3\n"
                "rur.accx_0 %[s]\n"
                : [a] "+r"(a), [b] "+r"(b), [k] "+r"(pairs), [s] "=r"(sum)
                :
                : "memory");
            i = (n >> 5) << 5;
        }
        for (; i < n; i++)
            sum += (int32_t)v1[i] * (int32_t)v2[i];
        return sum;
    }
#endif

    static int32_t vecn_dot_i8(const int8_t *v1, const int8_t *v2, int n)
    {
#if defined(R2_SIMD_AVX2)
        __m256i acc = _mm256_setzero_si256();
        int i = 0;
        for (; i + 32 <= n; i += 32)
        {
            __m256i a = _mm256_loadu_si256((const __m256i *)(v1 + i));
            __m256i b = _mm256_loadu_si256((const __m256i *)(v2 + i));
            __m256i a_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(a));
            __m256i a_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(a, 1));
            __m256i b_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(b));
            __m256i b_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(b, 1));
            acc = _mm256_add_epi32(acc, _mm256_madd_epi16(a_lo, b_lo));
            acc = _mm256_add_epi32(acc, _mm256_madd_epi16(a_hi, b_hi));
        }
        int32_t buf[8];
        _mm256_storeu_si256((__m256i *)buf, acc);
        int32_t sum = buf[0] + buf[1] + buf[2] + buf[3] + buf[4] + buf[5] + buf[6] + buf[7];
        for (; i < n; i++)
            sum += (int32_t)v1[i] * (int32_t)v2[i];
        return sum;

#elif defined(R2_SIMD_SSE41)
        __m128i acc = _mm_setzero_si128();
        int i = 0;
        for (; i + 16 <= n; i += 16)
        {
            __m128i a = _mm_loadu_si128((const __m128i *)(v1 + i));
            __m128i b = _mm_loadu_si128((const __m128i *)(v2 + i));
            __m128i a_lo = _mm_cvtepi8_epi16(a);
            __m128i a_hi = _mm_cvtepi8_epi16(_mm_srli_si128(a, 8));
            __m128i b_lo = _mm_cvtepi8_epi16(b);
            __m128i b_hi = _mm_cvtepi8_epi16(_mm_srli_si128(b, 8));
            acc = _mm_add_epi32(acc, _mm_madd_epi16(a_lo, b_lo));
            acc = _mm_add_epi32(acc, _mm_madd_epi16(a_hi, b_hi));
        }
        int32_t buf[4];
        _mm_storeu_si128((__m128i *)buf, acc);
        int32_t sum = buf[0] + buf[1] + buf[2] + buf[3];
        for (; i < n; i++)
            sum += (int32_t)v1[i] * (int32_t)v2[i];
        return sum;

#elif defined(R2_SIMD_NEON)
        int32x4_t acc = vdupq_n_s32(0);
        int i = 0;
        for (; i + 16 <= n; i += 16)
        {
            int8x16_t a = vld1q_s8(v1 + i);
            int8x16_t b = vld1q_s8(v2 + i);
            acc = vpadalq_s16(acc, vmull_s8(vget_low_s8(a), vget_low_s8(b)));
            acc = vpadalq_s16(acc, vmull_s8(vget_high_s8(a), vget_high_s8(b)));
        }
#if defined(__aarch64__)
        int32_t sum = vaddvq_s32(acc);
#else
        int32_t t[4];
        vst1q_s32(t, acc);
        int32_t sum = t[0] + t[1] + t[2] + t[3];
#endif
        for (; i < n; i++)
            sum += (int32_t)v1[i] * (int32_t)v2[i];
        return sum;

#elif defined(R2_SIMD_XTENSA_PIE)
        return r2_xtensa_pie_dot_i8(v1, v2, n);

#else
        int32_t sum = 0;
        int i;
        for (i = 0; i < n; i++)
            sum += (int32_t)v1[i] * (int32_t)v2[i];
        return sum;
#endif
    }

    static float vecn_dot_i8_f(const int8_t *v1, float s1, const int8_t *v2, float s2, int n)
    {
        return (float)vecn_dot_i8(v1, v2, n) * s1 * s2;
    }

    static void mat_mul_i8(const int8_t *m1, const int8_t *m2, unsigned r1, unsigned c1, unsigned r2, unsigned c2,
                           int32_t *out)
    {
        if (c1 != r2)
            return;

        unsigned i, j, k;
        for (i = 0; i < r1; i++)
        {
            int32_t *R2_RESTRICT out_row = out + i * c2;
            for (j = 0; j < c2; j++)
                out_row[j] = 0;
            for (k = 0; k < c1; k++)
            {
                int32_t a = (int32_t)m1[i * c1 + k];
                const int8_t *R2_RESTRICT m2_row = m2 + k * c2;
                for (j = 0; j < c2; j++)
                    out_row[j] += a * (int32_t)m2_row[j];
            }
        }
    }

    static void mat_mul_i8_f(const int8_t *m1, float s1, const int8_t *m2, float s2, unsigned r1, unsigned c1,
                             unsigned r2, unsigned c2, float *out)
    {
        if (c1 != r2)
            return;

        float scale = s1 * s2;
        unsigned i, j, k;
        for (i = 0; i < r1; i++)
        {
            for (j = 0; j < c2; j++)
            {
                int32_t acc = 0;
                for (k = 0; k < c1; k++)
                    acc += (int32_t)m1[i * c1 + k] * (int32_t)m2[k * c2 + j];
                out[i * c2 + j] = (float)acc * scale;
            }
        }
    }

    static void mat_mul_i8_requant(const int8_t *m1, float s1, const int8_t *m2, float s2, unsigned r1, unsigned c1,
                                   unsigned r2, unsigned c2, int8_t *out, float *out_scale)
    {
        if (c1 != r2)
        {
            *out_scale = 1.0f;
            return;
        }

        float scale = s1 * s2;
        unsigned i, j, k;

        float max_abs = 0.f;
        for (i = 0; i < r1; i++)
        {
            for (j = 0; j < c2; j++)
            {
                int32_t acc = 0;
                for (k = 0; k < c1; k++)
                    acc += (int32_t)m1[i * c1 + k] * (int32_t)m2[k * c2 + j];
                float av = fabsf((float)acc * scale);
                if (av > max_abs)
                    max_abs = av;
            }
        }

        float out_s = (max_abs < R2_QUANT_EPSILON) ? 1.0f : (max_abs / (float)R2_I8_QMAX);
        *out_scale = out_s;

        for (i = 0; i < r1; i++)
        {
            for (j = 0; j < c2; j++)
            {
                int32_t acc = 0;
                for (k = 0; k < c1; k++)
                    acc += (int32_t)m1[i * c1 + k] * (int32_t)m2[k * c2 + j];
                float v = (float)acc * scale;
                int32_t qi = r2__round_i32(v / out_s);
                if (qi > R2_I8_QMAX)
                    qi = R2_I8_QMAX;
                if (qi < R2_I8_QMIN)
                    qi = R2_I8_QMIN;
                out[i * c2 + j] = (int8_t)qi;
            }
        }
    }

    static float r2_quantize_i16(const float *in, int n, int16_t *out)
    {
        float max_abs = 0.f;
        int i;
        for (i = 0; i < n; i++)
        {
            float a = fabsf(in[i]);
            if (a > max_abs)
                max_abs = a;
        }
        float scale = (max_abs < R2_QUANT_EPSILON) ? 1.0f : (max_abs / (float)R2_I16_QMAX);
        for (i = 0; i < n; i++)
        {
            int32_t qi = r2__round_i32(in[i] / scale);
            if (qi > R2_I16_QMAX)
                qi = R2_I16_QMAX;
            if (qi < R2_I16_QMIN)
                qi = R2_I16_QMIN;
            out[i] = (int16_t)qi;
        }
        return scale;
    }

    static void r2_dequantize_i16(const int16_t *in, int n, float scale, float *out)
    {
        int i;
        for (i = 0; i < n; i++)
            out[i] = (float)in[i] * scale;
    }

#if defined(R2_SIMD_XTENSA_PIE)
    /* No PIE kernel for int16 yet: a correct scalar body so defining
       R2_XTENSA_PIE never leaves an unresolved symbol. */
    static int64_t r2_xtensa_pie_dot_i16(const int16_t *v1, const int16_t *v2, int n)
    {
        int64_t sum = 0;
        int i;
        for (i = 0; i < n; i++)
            sum += (int64_t)v1[i] * (int64_t)v2[i];
        return sum;
    }
#endif

    static int64_t vecn_dot_i16(const int16_t *v1, const int16_t *v2, int n)
    {
#if defined(R2_SIMD_AVX2)
        int64_t sum = 0;
        int i = 0;
        for (; i + 16 <= n; i += 16)
        {
            __m256i a = _mm256_loadu_si256((const __m256i *)(v1 + i));
            __m256i b = _mm256_loadu_si256((const __m256i *)(v2 + i));
            __m256i prod = _mm256_madd_epi16(a, b);
            int32_t buf[8];
            _mm256_storeu_si256((__m256i *)buf, prod);
            int k;
            for (k = 0; k < 8; k++)
                sum += (int64_t)buf[k];
        }
        for (; i < n; i++)
            sum += (int64_t)v1[i] * (int64_t)v2[i];
        return sum;

#elif defined(R2_SIMD_SSE41)
        int64_t sum = 0;
        int i = 0;
        for (; i + 8 <= n; i += 8)
        {
            __m128i a = _mm_loadu_si128((const __m128i *)(v1 + i));
            __m128i b = _mm_loadu_si128((const __m128i *)(v2 + i));
            __m128i prod = _mm_madd_epi16(a, b);
            int32_t buf[4];
            _mm_storeu_si128((__m128i *)buf, prod);
            int k;
            for (k = 0; k < 4; k++)
                sum += (int64_t)buf[k];
        }
        for (; i < n; i++)
            sum += (int64_t)v1[i] * (int64_t)v2[i];
        return sum;

#elif defined(R2_SIMD_NEON)
        int64x2_t acc = vdupq_n_s64(0);
        int i = 0;
        for (; i + 8 <= n; i += 8)
        {
            int16x8_t a = vld1q_s16(v1 + i);
            int16x8_t b = vld1q_s16(v2 + i);
            int32x4_t prod_lo = vmull_s16(vget_low_s16(a), vget_low_s16(b));
            int32x4_t prod_hi = vmull_s16(vget_high_s16(a), vget_high_s16(b));
            acc = vpadalq_s32(acc, prod_lo);
            acc = vpadalq_s32(acc, prod_hi);
        }
#if defined(__aarch64__)
        int64_t sum = vgetq_lane_s64(acc, 0) + vgetq_lane_s64(acc, 1);
#else
        int64_t t[2];
        vst1q_s64(t, acc);
        int64_t sum = t[0] + t[1];
#endif
        for (; i < n; i++)
            sum += (int64_t)v1[i] * (int64_t)v2[i];
        return sum;

#elif defined(R2_SIMD_XTENSA_PIE)
        return r2_xtensa_pie_dot_i16(v1, v2, n);

#else
        int64_t sum = 0;
        int i;
        for (i = 0; i < n; i++)
            sum += (int64_t)v1[i] * (int64_t)v2[i];
        return sum;
#endif
    }

    static float vecn_dot_i16_f(const int16_t *v1, float s1, const int16_t *v2, float s2, int n)
    {
        return (float)vecn_dot_i16(v1, v2, n) * s1 * s2;
    }

    static void mat_mul_i16(const int16_t *m1, const int16_t *m2, unsigned r1, unsigned c1, unsigned r2, unsigned c2,
                            int64_t *out)
    {
        if (c1 != r2)
            return;

        unsigned i, j, k;
        for (i = 0; i < r1; i++)
        {
            int64_t *R2_RESTRICT out_row = out + i * c2;
            for (j = 0; j < c2; j++)
                out_row[j] = 0;
            for (k = 0; k < c1; k++)
            {
                int64_t a = (int64_t)m1[i * c1 + k];
                const int16_t *R2_RESTRICT m2_row = m2 + k * c2;
                for (j = 0; j < c2; j++)
                    out_row[j] += a * (int64_t)m2_row[j];
            }
        }
    }

    static void mat_mul_i16_f(const int16_t *m1, float s1, const int16_t *m2, float s2, unsigned r1, unsigned c1,
                              unsigned r2, unsigned c2, float *out)
    {
        if (c1 != r2)
            return;
        float scale = s1 * s2;
        unsigned i, j, k;
        for (i = 0; i < r1; i++)
        {
            for (j = 0; j < c2; j++)
            {
                int64_t acc = 0;
                for (k = 0; k < c1; k++)
                    acc += (int64_t)m1[i * c1 + k] * (int64_t)m2[k * c2 + j];
                out[i * c2 + j] = (float)acc * scale;
            }
        }
    }

    static int8_t r2__nibble_decode(uint8_t byte, int hi)
    {
        int8_t v = hi ? (int8_t)(byte >> 4) : (int8_t)(byte & 0x0Fu);
        if (v & 0x08)
            v = (int8_t)(v - 16); /* sign-extend 4-bit -> 8-bit */
        return v;
    }

    static uint8_t r2__nibble_encode(int8_t lo, int8_t hi)
    {
        return (uint8_t)((((uint8_t)hi & 0x0Fu) << 4) | ((uint8_t)lo & 0x0Fu));
    }

    static int8_t r2__i4_at(const uint8_t *packed, int idx)
    {
        return r2__nibble_decode(packed[idx >> 1], idx & 1);
    }

    static int8_t r2__i4_clamp(int32_t v)
    {
        if (v > R2_I4_QMAX)
            v = R2_I4_QMAX;
        if (v < R2_I4_QMIN)
            v = R2_I4_QMIN;
        return (int8_t)v;
    }

    static size_t r2_i4_packed_bytes(int n)
    {
        return (size_t)((n + 1) / 2);
    }

    static void r2_pack_i4(const int8_t *in, int n, uint8_t *out)
    {
        int i;
        for (i = 0; i + 1 < n; i += 2)
            out[i >> 1] = r2__nibble_encode(r2__i4_clamp(in[i]), r2__i4_clamp(in[i + 1]));
        if (n & 1)
            out[n >> 1] = r2__nibble_encode(r2__i4_clamp(in[n - 1]), 0);
    }

    static void r2_unpack_i4(const uint8_t *packed, int n, int8_t *out)
    {
        int i;
        for (i = 0; i + 1 < n; i += 2)
        {
            uint8_t b = packed[i >> 1];
            out[i] = r2__nibble_decode(b, 0);
            out[i + 1] = r2__nibble_decode(b, 1);
        }
        if (n & 1)
            out[n - 1] = r2__nibble_decode(packed[n >> 1], 0);
    }

    static float r2_quantize_i4(const float *in, int n, uint8_t *out)
    {
        float max_abs = 0.f;
        int i;
        for (i = 0; i < n; i++)
        {
            float a = fabsf(in[i]);
            if (a > max_abs)
                max_abs = a;
        }
        float scale = (max_abs < R2_QUANT_EPSILON) ? 1.0f : (max_abs / (float)R2_I4_QMAX);
        for (i = 0; i + 1 < n; i += 2)
        {
            int8_t lo = r2__i4_clamp(r2__round_i32(in[i] / scale));
            int8_t hi = r2__i4_clamp(r2__round_i32(in[i + 1] / scale));
            out[i >> 1] = r2__nibble_encode(lo, hi);
        }
        if (n & 1)
        {
            int8_t lo = r2__i4_clamp(r2__round_i32(in[n - 1] / scale));
            out[n >> 1] = r2__nibble_encode(lo, 0);
        }
        return scale;
    }

    static void r2_dequantize_i4(const uint8_t *packed, int n, float scale, float *out)
    {
        int i;
        for (i = 0; i + 1 < n; i += 2)
        {
            uint8_t b = packed[i >> 1];
            out[i] = (float)r2__nibble_decode(b, 0) * scale;
            out[i + 1] = (float)r2__nibble_decode(b, 1) * scale;
        }
        if (n & 1)
            out[n - 1] = (float)r2__nibble_decode(packed[n >> 1], 0) * scale;
    }

    static int32_t vecn_dot_i4(const uint8_t *p1, const uint8_t *p2, int n)
    {
        int32_t sum = 0;
        int i;
        for (i = 0; i + 1 < n; i += 2)
        {
            uint8_t b1 = p1[i >> 1], b2 = p2[i >> 1];
            sum += (int32_t)r2__nibble_decode(b1, 0) * r2__nibble_decode(b2, 0) +
                   (int32_t)r2__nibble_decode(b1, 1) * r2__nibble_decode(b2, 1);
        }
        if (n & 1)
        {
            uint8_t b1 = p1[n >> 1], b2 = p2[n >> 1];
            sum += (int32_t)r2__nibble_decode(b1, 0) * r2__nibble_decode(b2, 0);
        }
        return sum;
    }

    static float vecn_dot_i4_f(const uint8_t *p1, float s1, const uint8_t *p2, float s2, int n)
    {
        return (float)vecn_dot_i4(p1, p2, n) * s1 * s2;
    }

    static void mat_mul_i4(const uint8_t *packed_m1, const uint8_t *packed_m2, unsigned r1, unsigned c1, unsigned r2,
                           unsigned c2, int32_t *out)
    {
        if (c1 != r2)
            return;
        unsigned i, j, k;
        for (i = 0; i < r1; i++)
        {
            int32_t *R2_RESTRICT out_row = out + i * c2;
            for (j = 0; j < c2; j++)
                out_row[j] = 0;
            for (k = 0; k < c1; k++)
            {
                int32_t a = (int32_t)r2__i4_at(packed_m1, i * c1 + k);
                for (j = 0; j < c2; j++)
                    out_row[j] += a * (int32_t)r2__i4_at(packed_m2, k * c2 + j);
            }
        }
    }


    /* ---------------- u4 groups ---------------- */

    static void r2_pack_u4g16(const uint8_t *q, int n, uint8_t *out)
    {
        int b, j;
        for (b = 0; b < n; b += 32)
            for (j = 0; j < 16; j++)
                *out++ = (uint8_t)((q[b + j] & 15u) | ((q[b + 16 + j] & 15u) << 4));
    }

    static void r2_unpack_u4g16(const uint8_t *packed, int n, uint8_t *out)
    {
        int b, j;
        for (b = 0; b < n; b += 32, packed += 16)
            for (j = 0; j < 16; j++)
            {
                out[b + j] = packed[j] & 15u;
                out[b + 16 + j] = packed[j] >> 4;
            }
    }

    static void r2__dot_u4g16_scalar(const uint8_t *packed, const int8_t *x, int n, int32_t *out)
    {
        int b, j;
        for (b = 0; b < n; b += 32, packed += 16, x += 32)
        {
            int32_t lo = 0, hi = 0;
            for (j = 0; j < 16; j++)
            {
                lo += (int32_t)(packed[j] & 15u) * x[j];
                hi += (int32_t)(packed[j] >> 4) * x[16 + j];
            }
            *out++ = lo;
            *out++ = hi;
        }
    }

    static void r2_dot_u4g16_block(const uint8_t *packed, const int8_t *x, int32_t *d0, int32_t *d1)
    {
#if defined(R2_SIMD_XTENSA_PIE)
        static const uint8_t r2__blk_mask = 15u;
        int32_t a, b;
        const uint8_t *pk = packed;
        const int8_t *px = x;
        __asm__ volatile(
            "ssai 4\n"
            "ee.vldbc.8 q7, %[m]\n"
            "ee.vld.128.ip q0, %[pk], 16\n"
            "ee.vld.128.ip q4, %[px], 16\n"
            "ee.vld.128.ip q5, %[px], 16\n"
            "ee.andq q1, q0, q7\n"
            "ee.vsr.32 q2, q0\n"
            "ee.andq q2, q2, q7\n"
            "ee.zero.accx\n"
            "ee.vmulas.s8.accx q1, q4\n"
            "rur.accx_0 %[a]\n"
            "ee.zero.accx\n"
            "ee.vmulas.s8.accx q2, q5\n"
            "rur.accx_0 %[b]\n"
            : [pk] "+r"(pk), [px] "+r"(px), [a] "=&r"(a), [b] "=&r"(b)
            : [m] "r"(&r2__blk_mask)
            : "memory");
        *d0 = a;
        *d1 = b;
#elif defined(R2_SIMD_NEON) && defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)
        uint8x16_t v = vld1q_u8(packed);
        int8x16_t lo = vreinterpretq_s8_u8(vandq_u8(v, vdupq_n_u8(15)));
        int8x16_t hi = vreinterpretq_s8_u8(vshrq_n_u8(v, 4));
        *d0 = vaddvq_s32(vdotq_s32(vdupq_n_s32(0), lo, vld1q_s8(x)));
        *d1 = vaddvq_s32(vdotq_s32(vdupq_n_s32(0), hi, vld1q_s8(x + 16)));
#else
        int32_t lo = 0, hi = 0;
        int j;
        for (j = 0; j < 16; j++)
        {
            lo += (int32_t)(packed[j] & 15u) * x[j];
            hi += (int32_t)(packed[j] >> 4) * x[16 + j];
        }
        *d0 = lo;
        *d1 = hi;
#endif
    }

    static void r2_dot_u4g16(const uint8_t *packed, const int8_t *x, int n, int32_t *out)
    {
#if defined(R2_SIMD_NEON) && defined(__aarch64__) && defined(__ARM_FEATURE_DOTPROD)
        const uint8x16_t m4 = vdupq_n_u8(15);
        int b;
        for (b = 0; b < n; b += 32, packed += 16, x += 32)
        {
            uint8x16_t v = vld1q_u8(packed);
            int8x16_t lo = vreinterpretq_s8_u8(vandq_u8(v, m4));
            int8x16_t hi = vreinterpretq_s8_u8(vshrq_n_u8(v, 4));
            *out++ = vaddvq_s32(vdotq_s32(vdupq_n_s32(0), lo, vld1q_s8(x)));
            *out++ = vaddvq_s32(vdotq_s32(vdupq_n_s32(0), hi, vld1q_s8(x + 16)));
        }
#elif defined(R2_SIMD_XTENSA_PIE)
        /* Needs CPENABLE bit 3 (see r2_xtensa_pie_dot_i8). One block per
           iteration: split nibbles with AND / 32-bit-lane shift right by 4
           (SAR) / AND -- bits shifted in from the neighbouring byte are
           masked off -- then one 16-lane MAC per group into ACCX. */
        if (n >= 32 && ((((uintptr_t)packed) | ((uintptr_t)x)) & 15u) == 0)
        {
            static const uint8_t r2__nib_mask = 15u;
            const uint8_t *pk = packed;
            const int8_t *px = x;
            int32_t *po = out;
            int k = n >> 5;
            int32_t t;
            __asm__ volatile(
                "ssai 4\n"
                "ee.vldbc.8 q7, %[m]\n"
                "1:\n"
                "ee.vld.128.ip q0, %[pk], 16\n"
                "ee.vld.128.ip q4, %[px], 16\n"
                "ee.vld.128.ip q5, %[px], 16\n"
                "ee.andq q1, q0, q7\n"
                "ee.vsr.32 q2, q0\n"
                "ee.andq q2, q2, q7\n"
                "ee.zero.accx\n"
                "ee.vmulas.s8.accx q1, q4\n"
                "rur.accx_0 %[t]\n"
                "s32i %[t], %[po], 0\n"
                "ee.zero.accx\n"
                "ee.vmulas.s8.accx q2, q5\n"
                "rur.accx_0 %[t]\n"
                "s32i %[t], %[po], 4\n"
                "addi %[po], %[po], 8\n"
                "addi %[k], %[k], -1\n"
                "bnez %[k], 1b\n"
                : [pk] "+r"(pk), [px] "+r"(px), [po] "+r"(po), [k] "+r"(k), [t] "=&r"(t)
                : [m] "r"(&r2__nib_mask)
                /* SAR is written (ssai) but can't be named as a clobber;
                   GCC only uses SAR inside a single shift pattern (ssl/ssr
                   immediately followed by the shift), never live across an
                   asm statement, so that's safe. */
                : "memory");
            return;
        }
        r2__dot_u4g16_scalar(packed, x, n, out);
#else
        r2__dot_u4g16_scalar(packed, x, n, out);
#endif
    }

#endif /* R2_QUANT_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* R2_QUANT_H */

/*
   revision history:
    0.0   (2026-08-01) Initial int8 quantized ops
*/
