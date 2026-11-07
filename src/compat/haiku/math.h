/*
 * math.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 * Provides C99/C11 math functions missing from Haiku's libroot using compiler builtins
 */

#ifndef _MATH_H
#define _MATH_H

#include <math.h>

#if defined(__cplusplus)
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
/* Declared here (no static inline bodies) to avoid conflicting with
 * system headers that already declare them as extern. Linker resolves
 * against libroot on Haiku. */

#if defined(__GNUC__) || defined(__clang__)

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

#if defined(__cplusplus)
}
#endif

#endif /* _MATH_H */
