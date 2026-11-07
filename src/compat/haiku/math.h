/*
 * math.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 * Provides C99/C11 math functions missing from Haiku's libroot using compiler builtins
 */

#ifndef _MATH_H
#define _MATH_H

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure double_t and float_t are defined (C99) */
#ifndef _DOUBLE_T_DEFINED
typedef double double_t;
#define _DOUBLE_T_DEFINED
#endif

#ifndef _FLOAT_T_DEFINED
typedef float float_t;
#define _FLOAT_T_DEFINED
#endif

/* C99/C11 math functions missing from Haiku's libroot */
/* Implemented using GCC/Clang compiler builtins */
/* Use PLUTO_HAVE_* guards to avoid conflicts with system headers */

#if defined(__GNUC__) || defined(__clang__)

/* frexp, ldexp, modf - usually available but ensure declarations */
#ifndef PLUTO_HAVE_FREXP
#define PLUTO_HAVE_FREXP
static inline double frexp(double x, int *exp) { return __builtin_frexp(x, exp); }
static inline float frexpf(float x, int *exp) { return __builtin_frexpf(x, exp); }
static inline long double frexpl(long double x, int *exp) { return __builtin_frexpl(x, exp); }
#endif

#ifndef PLUTO_HAVE_LDEXP
#define PLUTO_HAVE_LDEXP
static inline double ldexp(double x, int exp) { return __builtin_ldexp(x, exp); }
static inline float ldexpf(float x, int exp) { return __builtin_ldexpf(x, exp); }
static inline long double ldexpl(long double x, int exp) { return __builtin_ldexpl(x, exp); }
#endif

#ifndef PLUTO_HAVE_MODF
#define PLUTO_HAVE_MODF
static inline double modf(double x, double *iptr) { return __builtin_modf(x, iptr); }
static inline float modff(float x, float *iptr) { return __builtin_modff(x, iptr); }
static inline long double modfl(long double x, long double *iptr) { return __builtin_modfl(x, iptr); }
#endif

/* acosh, asinh, atanh */
#ifndef PLUTO_HAVE_ACOSH
#define PLUTO_HAVE_ACOSH
static inline double acosh(double x) { return __builtin_acosh(x); }
static inline float acoshf(float x) { return __builtin_acoshf(x); }
static inline long double acoshl(long double x) { return __builtin_acoshl(x); }
#endif

#ifndef PLUTO_HAVE_ASINH
#define PLUTO_HAVE_ASINH
static inline double asinh(double x) { return __builtin_asinh(x); }
static inline float asinhf(float x) { return __builtin_asinhf(x); }
static inline long double asinhl(long double x) { return __builtin_asinhl(x); }
#endif

#ifndef PLUTO_HAVE_ATANH
#define PLUTO_HAVE_ATANH
static inline double atanh(double x) { return __builtin_atanh(x); }
static inline float atanhf(float x) { return __builtin_atanhf(x); }
static inline long double atanhl(long double x) { return __builtin_atanhl(x); }
#endif

/* cbrt */
#ifndef PLUTO_HAVE_CBRT
#define PLUTO_HAVE_CBRT
static inline double cbrt(double x) { return __builtin_cbrt(x); }
static inline float cbrtf(float x) { return __builtin_cbrtf(x); }
static inline long double cbrtl(long double x) { return __builtin_cbrtl(x); }
#endif

/* copysign */
#ifndef PLUTO_HAVE_COPYSIGN
#define PLUTO_HAVE_COPYSIGN
static inline double copysign(double x, double y) { return __builtin_copysign(x, y); }
static inline float copysignf(float x, float y) { return __builtin_copysignf(x, y); }
static inline long double copysignl(long double x, long double y) { return __builtin_copysignl(x, y); }
#endif

/* erf, erfc */
#ifndef PLUTO_HAVE_ERF
#define PLUTO_HAVE_ERF
static inline double erf(double x) { return __builtin_erf(x); }
static inline float erff(float x) { return __builtin_erff(x); }
static inline long double erfl(long double x) { return __builtin_erfl(x); }
static inline double erfc(double x) { return __builtin_erfc(x); }
static inline float erfcf(float x) { return __builtin_erfcf(x); }
static inline long double erfcl(long double x) { return __builtin_erfcl(x); }
#endif

/* exp2, expm1 */
#ifndef PLUTO_HAVE_EXP2
#define PLUTO_HAVE_EXP2
static inline double exp2(double x) { return __builtin_exp2(x); }
static inline float exp2f(float x) { return __builtin_exp2f(x); }
static inline long double exp2l(long double x) { return __builtin_exp2l(x); }
#endif

#ifndef PLUTO_HAVE_EXPM1
#define PLUTO_HAVE_EXPM1
static inline double expm1(double x) { return __builtin_expm1(x); }
static inline float expm1f(float x) { return __builtin_expm1f(x); }
static inline long double expm1l(long double x) { return __builtin_expm1l(x); }
#endif

/* fdim, fma, fmax, fmin */
#ifndef PLUTO_HAVE_FDIM
#define PLUTO_HAVE_FDIM
static inline double fdim(double x, double y) { return __builtin_fdim(x, y); }
static inline float fdimf(float x, float y) { return __builtin_fdimf(x, y); }
static inline long double fdiml(long double x, long double y) { return __builtin_fdiml(x, y); }
#endif

#ifndef PLUTO_HAVE_FMA
#define PLUTO_HAVE_FMA
static inline double fma(double x, double y, double z) { return __builtin_fma(x, y, z); }
static inline float fmaf(float x, float y, float z) { return __builtin_fmaf(x, y, z); }
static inline long double fmal(long double x, long double y, long double z) { return __builtin_fmal(x, y, z); }
#endif

#ifndef PLUTO_HAVE_FMAX
#define PLUTO_HAVE_FMAX
static inline double fmax(double x, double y) { return __builtin_fmax(x, y); }
static inline float fmaxf(float x, float y) { return __builtin_fmaxf(x, y); }
static inline long double fmaxl(long double x, long double y) { return __builtin_fmaxl(x, y); }
#endif

#ifndef PLUTO_HAVE_FMIN
#define PLUTO_HAVE_FMIN
static inline double fmin(double x, double y) { return __builtin_fmin(x, y); }
static inline float fminf(float x, float y) { return __builtin_fminf(x, y); }
static inline long double fminl(long double x, long double y) { return __builtin_fminl(x, y); }
#endif

/* hypot */
#ifndef PLUTO_HAVE_HYPOT
#define PLUTO_HAVE_HYPOT
static inline double hypot(double x, double y) { return __builtin_hypot(x, y); }
static inline float hypotf(float x, float y) { return __builtin_hypotf(x, y); }
static inline long double hypotl(long double x, long double y) { return __builtin_hypotl(x, y); }
#endif

/* ilogb */
#ifndef PLUTO_HAVE_ILOGB
#define PLUTO_HAVE_ILOGB
static inline int ilogb(double x) { return __builtin_ilogb(x); }
static inline int ilogbf(float x) { return __builtin_ilogbf(x); }
static inline int ilogbl(long double x) { return __builtin_ilogbl(x); }
#endif

/* lgamma */
#ifndef PLUTO_HAVE_LGAMMA
#define PLUTO_HAVE_LGAMMA
static inline double lgamma(double x) { return __builtin_lgamma(x); }
static inline float lgammaf(float x) { return __builtin_lgammaf(x); }
static inline long double lgammal(long double x) { return __builtin_lgammal(x); }
#endif

/* llrint, llround */
#ifndef PLUTO_HAVE_LLRINT
#define PLUTO_HAVE_LLRINT
static inline long long llrint(double x) { return __builtin_llrint(x); }
static inline long long llrintf(float x) { return __builtin_llrintf(x); }
static inline long long llrintl(long double x) { return __builtin_llrintl(x); }
#endif

#ifndef PLUTO_HAVE_LLROUND
#define PLUTO_HAVE_LLROUND
static inline long long llround(double x) { return __builtin_llround(x); }
static inline long long llroundf(float x) { return __builtin_llroundf(x); }
static inline long long llroundl(long double x) { return __builtin_llroundl(x); }
#endif

/* log1p, log2, logb */
#ifndef PLUTO_HAVE_LOG1P
#define PLUTO_HAVE_LOG1P
static inline double log1p(double x) { return __builtin_log1p(x); }
static inline float log1pf(float x) { return __builtin_log1pf(x); }
static inline long double log1pl(long double x) { return __builtin_log1pl(x); }
#endif

#ifndef PLUTO_HAVE_LOG2
#define PLUTO_HAVE_LOG2
static inline double log2(double x) { return __builtin_log2(x); }
static inline float log2f(float x) { return __builtin_log2f(x); }
static inline long double log2l(long double x) { return __builtin_log2l(x); }
#endif

#ifndef PLUTO_HAVE_LOGB
#define PLUTO_HAVE_LOGB
static inline double logb(double x) { return __builtin_logb(x); }
static inline float logbf(float x) { return __builtin_logbf(x); }
static inline long double logbl(long double x) { return __builtin_logbl(x); }
#endif

/* lrint, lround */
#ifndef PLUTO_HAVE_LRINT
#define PLUTO_HAVE_LRINT
static inline long lrint(double x) { return __builtin_lrint(x); }
static inline long lrintf(float x) { return __builtin_lrintf(x); }
static inline long lrintl(long double x) { return __builtin_lrintl(x); }
#endif

#ifndef PLUTO_HAVE_LROUND
#define PLUTO_HAVE_LROUND
static inline long lround(double x) { return __builtin_lround(x); }
static inline long lroundf(float x) { return __builtin_lroundf(x); }
static inline long lroundl(long double x) { return __builtin_lroundl(x); }
#endif

/* nan */
#ifndef PLUTO_HAVE_NAN
#define PLUTO_HAVE_NAN
static inline double nan(const char *tagp) { return __builtin_nan(tagp); }
static inline float nanf(const char *tagp) { return __builtin_nanf(tagp); }
static inline long double nanl(const char *tagp) { return __builtin_nanl(tagp); }
#endif

/* nearbyint */
#ifndef PLUTO_HAVE_NEARBYINT
#define PLUTO_HAVE_NEARBYINT
static inline double nearbyint(double x) { return __builtin_nearbyint(x); }
static inline float nearbyintf(float x) { return __builtin_nearbyintf(x); }
static inline long double nearbyintl(long double x) { return __builtin_nearbyintl(x); }
#endif

/* nextafter, nexttoward */
#ifndef PLUTO_HAVE_NEXTAFTER
#define PLUTO_HAVE_NEXTAFTER
static inline double nextafter(double x, double y) { return __builtin_nextafter(x, y); }
static inline float nextafterf(float x, float y) { return __builtin_nextafterf(x, y); }
static inline long double nextafterl(long double x, long double y) { return __builtin_nextafterl(x, y); }
#endif

#ifndef PLUTO_HAVE_NEXTTOWARD
#define PLUTO_HAVE_NEXTTOWARD
static inline double nexttoward(double x, long double y) { return __builtin_nexttoward(x, y); }
static inline float nexttowardf(float x, long double y) { return __builtin_nexttowardf(x, y); }
static inline long double nexttowardl(long double x, long double y) { return __builtin_nexttowardl(x, y); }
#endif

/* remainder, remquo */
#ifndef PLUTO_HAVE_REMAINDER
#define PLUTO_HAVE_REMAINDER
static inline double remainder(double x, double y) { return __builtin_remainder(x, y); }
static inline float remainderf(float x, float y) { return __builtin_remainderf(x, y); }
static inline long double remainderl(long double x, long double y) { return __builtin_remainderl(x, y); }
#endif

#ifndef PLUTO_HAVE_REMQUO
#define PLUTO_HAVE_REMQUO
static inline double remquo(double x, double y, int *quo) { return __builtin_remquo(x, y, quo); }
static inline float remquof(float x, float y, int *quo) { return __builtin_remquof(x, y, quo); }
static inline long double remquol(long double x, long double y, int *quo) { return __builtin_remquol(x, y, quo); }
#endif

/* rint, round */
#ifndef PLUTO_HAVE_RINT
#define PLUTO_HAVE_RINT
static inline double rint(double x) { return __builtin_rint(x); }
static inline float rintf(float x) { return __builtin_rintf(x); }
static inline long double rintl(long double x) { return __builtin_rintl(x); }
#endif

#ifndef PLUTO_HAVE_ROUND
#define PLUTO_HAVE_ROUND
static inline double round(double x) { return __builtin_round(x); }
static inline float roundf(float x) { return __builtin_roundf(x); }
static inline long double roundl(long double x) { return __builtin_roundl(x); }
#endif

/* scalbln, scalbn */
#ifndef PLUTO_HAVE_SCALBLN
#define PLUTO_HAVE_SCALBLN
static inline double scalbln(double x, long n) { return __builtin_scalbln(x, n); }
static inline float scalblnf(float x, long n) { return __builtin_scalblnf(x, n); }
static inline long double scalblnl(long double x, long n) { return __builtin_scalblnl(x, n); }
#endif

#ifndef PLUTO_HAVE_SCALBN
#define PLUTO_HAVE_SCALBN
static inline double scalbn(double x, int n) { return __builtin_scalbn(x, n); }
static inline float scalbnf(float x, int n) { return __builtin_scalbnf(x, n); }
static inline long double scalbnl(long double x, int n) { return __builtin_scalbnl(x, n); }
#endif

/* tgamma */
#ifndef PLUTO_HAVE_TGAMMA
#define PLUTO_HAVE_TGAMMA
static inline double tgamma(double x) { return __builtin_tgamma(x); }
static inline float tgammaf(float x) { return __builtin_tgammaf(x); }
static inline long double tgammal(long double x) { return __builtin_tgammal(x); }
#endif

/* trunc */
#ifndef PLUTO_HAVE_TRUNC
#define PLUTO_HAVE_TRUNC
static inline double trunc(double x) { return __builtin_trunc(x); }
static inline float truncf(float x) { return __builtin_truncf(x); }
static inline long double truncl(long double x) { return __builtin_truncl(x); }
#endif

#else
/* Non-GCC/Clang: declare as extern (link against libm if available) */
#ifndef PLUTO_HAVE_FREXP
#define PLUTO_HAVE_FREXP
double frexp(double x, int *exp);
float frexpf(float x, int *exp);
long double frexpl(long double x, int *exp);
#endif

#ifndef PLUTO_HAVE_LDEXP
#define PLUTO_HAVE_LDEXP
double ldexp(double x, int exp);
float ldexpf(float x, int exp);
long double ldexpl(long double x, int exp);
#endif

#ifndef PLUTO_HAVE_MODF
#define PLUTO_HAVE_MODF
double modf(double x, double *iptr);
float modff(float x, float *iptr);
long double modfl(long double x, long double *iptr);
#endif

#ifndef PLUTO_HAVE_ACOSH
#define PLUTO_HAVE_ACOSH
double acosh(double x);
float acoshf(float x);
long double acoshl(long double x);
#endif

#ifndef PLUTO_HAVE_ASINH
#define PLUTO_HAVE_ASINH
double asinh(double x);
float asinhf(float x);
long double asinhl(long double x);
#endif

#ifndef PLUTO_HAVE_ATANH
#define PLUTO_HAVE_ATANH
double atanh(double x);
float atanhf(float x);
long double atanhl(long double x);
#endif

#ifndef PLUTO_HAVE_CBRT
#define PLUTO_HAVE_CBRT
double cbrt(double x);
float cbrtf(float x);
long double cbrtl(long double x);
#endif

#ifndef PLUTO_HAVE_COPYSIGN
#define PLUTO_HAVE_COPYSIGN
double copysign(double x, double y);
float copysignf(float x, float y);
long double copysignl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_ERF
#define PLUTO_HAVE_ERF
double erf(double x);
float erff(float x);
long double erfl(long double x);
double erfc(double x);
float erfcf(float x);
long double erfcl(long double x);
#endif

#ifndef PLUTO_HAVE_EXP2
#define PLUTO_HAVE_EXP2
double exp2(double x);
float exp2f(float x);
long double exp2l(long double x);
#endif

#ifndef PLUTO_HAVE_EXPM1
#define PLUTO_HAVE_EXPM1
double expm1(double x);
float expm1f(float x);
long double expm1l(long double x);
#endif

#ifndef PLUTO_HAVE_FDIM
#define PLUTO_HAVE_FDIM
double fdim(double x, double y);
float fdimf(float x, float y);
long double fdiml(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_FMA
#define PLUTO_HAVE_FMA
double fma(double x, double y, double z);
float fmaf(float x, float y, float z);
long double fmal(long double x, long double y, long double z);
#endif

#ifndef PLUTO_HAVE_FMAX
#define PLUTO_HAVE_FMAX
double fmax(double x, double y);
float fmaxf(float x, float y);
long double fmaxl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_FMIN
#define PLUTO_HAVE_FMIN
double fmin(double x, double y);
float fminf(float x, float y);
long double fminl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_HYPOT
#define PLUTO_HAVE_HYPOT
double hypot(double x, double y);
float hypotf(float x, float y);
long double hypotl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_ILOGB
#define PLUTO_HAVE_ILOGB
int ilogb(double x);
int ilogbf(float x);
int ilogbl(long double x);
#endif

#ifndef PLUTO_HAVE_LGAMMA
#define PLUTO_HAVE_LGAMMA
double lgamma(double x);
float lgammaf(float x);
long double lgammal(long double x);
#endif

#ifndef PLUTO_HAVE_LLRINT
#define PLUTO_HAVE_LLRINT
long long llrint(double x);
long long llrintf(float x);
long long llrintl(long double x);
#endif

#ifndef PLUTO_HAVE_LLROUND
#define PLUTO_HAVE_LLROUND
long long llround(double x);
long long llroundf(float x);
long long llroundl(long double x);
#endif

#ifndef PLUTO_HAVE_LOG1P
#define PLUTO_HAVE_LOG1P
double log1p(double x);
float log1pf(float x);
long double log1pl(long double x);
#endif

#ifndef PLUTO_HAVE_LOG2
#define PLUTO_HAVE_LOG2
double log2(double x);
float log2f(float x);
long double log2l(long double x);
#endif

#ifndef PLUTO_HAVE_LOGB
#define PLUTO_HAVE_LOGB
double logb(double x);
float logbf(float x);
long double logbl(long double x);
#endif

#ifndef PLUTO_HAVE_LRINT
#define PLUTO_HAVE_LRINT
long lrint(double x);
long lrintf(float x);
long lrintl(long double x);
#endif

#ifndef PLUTO_HAVE_LROUND
#define PLUTO_HAVE_LROUND
long lround(double x);
long lroundf(float x);
long lroundl(long double x);
#endif

#ifndef PLUTO_HAVE_NAN
#define PLUTO_HAVE_NAN
double nan(const char *tagp);
float nanf(const char *tagp);
long double nanl(const char *tagp);
#endif

#ifndef PLUTO_HAVE_NEARBYINT
#define PLUTO_HAVE_NEARBYINT
double nearbyint(double x);
float nearbyintf(float x);
long double nearbyintl(long double x);
#endif

#ifndef PLUTO_HAVE_NEXTAFTER
#define PLUTO_HAVE_NEXTAFTER
double nextafter(double x, double y);
float nextafterf(float x, float y);
long double nextafterl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_NEXTTOWARD
#define PLUTO_HAVE_NEXTTOWARD
double nexttoward(double x, long double y);
float nexttowardf(float x, long double y);
long double nexttowardl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_REMAINDER
#define PLUTO_HAVE_REMAINDER
double remainder(double x, double y);
float remainderf(float x, float y);
long double remainderl(long double x, long double y);
#endif

#ifndef PLUTO_HAVE_REMQUO
#define PLUTO_HAVE_REMQUO
double remquo(double x, double y, int *quo);
float remquof(float x, float y, int *quo);
long double remquol(long double x, long double y, int *quo);
#endif

#ifndef PLUTO_HAVE_RINT
#define PLUTO_HAVE_RINT
double rint(double x);
float rintf(float x);
long double rintl(long double x);
#endif

#ifndef PLUTO_HAVE_ROUND
#define PLUTO_HAVE_ROUND
double round(double x);
float roundf(float x);
long double roundl(long double x);
#endif

#ifndef PLUTO_HAVE_SCALBLN
#define PLUTO_HAVE_SCALBLN
double scalbln(double x, long n);
float scalblnf(float x, long n);
long double scalblnl(long double x, long n);
#endif

#ifndef PLUTO_HAVE_SCALBN
#define PLUTO_HAVE_SCALBN
double scalbn(double x, int n);
float scalbnf(float x, int n);
long double scalbnl(long double x, int n);
#endif

#ifndef PLUTO_HAVE_TGAMMA
#define PLUTO_HAVE_TGAMMA
double tgamma(double x);
float tgammaf(float x);
long double tgammal(long double x);
#endif

#ifndef PLUTO_HAVE_TRUNC
#define PLUTO_HAVE_TRUNC
double trunc(double x);
float truncf(float x);
long double truncl(long double x);
#endif

#endif /* __GNUC__ || __clang__ */

#ifdef __cplusplus
}
#endif

#endif /* _MATH_H */