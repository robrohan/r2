
/* r2_simd - v0.0 - public domain platform/SIMD detection helper
    no warranty implied; use at your own risk

    This header is internal plumbing for r2_quant.h (and, in the future,
    possibly r2_maths.h). It defines no functions - only preprocessor
    architecture detection and compile-time SIMD backend selection - so it
    has no implementation-guard split and is always safe/cheap to include
    repeatedly.

    You are not expected to #include this directly. r2_quant.h's
    implementation block includes it for you.

    Backend selection is compile-time only (a chain of #ifdef/#elif), never
    a runtime function-pointer dispatch - this matches r2_maths.h's existing
    HAVE_BLAS precedent.

    Priority order: an explicit R2_SIMD_FORCE_SCALAR override always wins
    (useful for CI, to make sure the portable-C reference path actually
    runs and not just compiles); then AVX2; then SSE4.1; then NEON; then
    Xtensa PIE (only if the consumer explicitly defines R2_XTENSA_PIE - this
    one is never auto-detected, since ESP32-S3 build environments vary in
    whether the vendor's TIE/PIE intrinsic headers are even present); then
    the portable-C scalar fallback, which is always correct everywhere.

LICENSE
    See r2_maths.h for license information (this file is released under
    the same terms).
*/

#ifndef R2_SIMD_H
#define R2_SIMD_H

/* restrict is C99, not a keyword in C++. r2_maths.h keeps its own private
   copy of this same macro - not shared, to avoid coupling the two headers
   together (see r2_quant.h's file-layout note). */
#ifdef __cplusplus
    #define R2_RESTRICT
#else
    #define R2_RESTRICT restrict
#endif

/* ---------------- architecture detection ---------------- */

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    #define R2_ARCH_X86 1
#endif

#if defined(__aarch64__) || defined(__arm__) || defined(_M_ARM64)
    #define R2_ARCH_ARM 1
#endif

#if defined(__xtensa__)
    #define R2_ARCH_XTENSA 1
#endif

/* ---------------- backend selection ----------------
   Exactly one of R2_SIMD_AVX2 / R2_SIMD_SSE41 / R2_SIMD_NEON /
   R2_SIMD_XTENSA_PIE is ever defined; none of them means the portable-C
   scalar kernel is used. Kernels in r2_quant.h test only these normalized
   macros, never raw compiler predefines directly, so "what triggers a
   backend" lives in exactly one place. */

#if defined(R2_SIMD_FORCE_SCALAR)
    #define R2_SIMD_LEVEL 0
    #define R2_SIMD_BACKEND_NAME "scalar-forced"

#elif defined(R2_ARCH_X86) && defined(__AVX2__)
    #define R2_SIMD_LEVEL 3
    #define R2_SIMD_AVX2 1
    #define R2_SIMD_BACKEND_NAME "avx2"
    #include <immintrin.h>

#elif defined(R2_ARCH_X86) && defined(__SSE4_1__)
    #define R2_SIMD_LEVEL 2
    #define R2_SIMD_SSE41 1
    #define R2_SIMD_BACKEND_NAME "sse4.1"
    #include <smmintrin.h>

#elif defined(R2_ARCH_ARM) && (defined(__aarch64__) || defined(__ARM_NEON))
    #define R2_SIMD_LEVEL 2
    #define R2_SIMD_NEON 1
    #define R2_SIMD_BACKEND_NAME "neon"
    #include <arm_neon.h>

#elif defined(R2_ARCH_XTENSA) && defined(R2_XTENSA_PIE)
    #define R2_SIMD_LEVEL 2
    #define R2_SIMD_XTENSA_PIE 1
    #define R2_SIMD_BACKEND_NAME "xtensa-pie"
    /* R2_XTENSA_PIE is a consumer-supplied opt-in (e.g. Chestnut's Makefile
       would define it) - it is never auto-detected from __xtensa__ alone,
       because it's unverified whether a given Xtensa toolchain even ships
       the TIE/PIE intrinsic headers. Kernel bodies for this backend are a
       follow-up, not implemented yet - see r2_quant.h's dispatch chains,
       which #error out if this macro is ever active. */

#else
    #define R2_SIMD_LEVEL 0
    #define R2_SIMD_BACKEND_NAME "scalar"
#endif

#endif /* R2_SIMD_H */
