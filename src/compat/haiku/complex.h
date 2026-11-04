/*
 * Minimal complex.h for Haiku compatibility
 */

#ifndef _COMPLEX_H
#define _COMPLEX_H

#ifdef __cplusplus
#include <complex>
#else
#include <math.h>

#define complex _Complex
#define _Complex_I (0.0f + 1.0fi)
#define I _Complex_I

#define cabs(z)       cabsf(z)
#define cacos(z)      cacosf(z)
#define cacosh(z)     cacoshf(z)
#define carg(z)       cargf(z)
#define casin(z)      casinf(z)
#define casinh(z)     casinhf(z)
#define catan(z)      catanf(z)
#define catanh(z)     catanhf(z)
#define ccos(z)       ccosf(z)
#define ccosh(z)      ccoshf(z)
#define cexp(z)       cexpf(z)
#define cimag(z)      cimagf(z)
#define clog(z)       clogf(z)
#define conj(z)       conjf(z)
#define cpow(z)       cpowf(z)
#define cproj(z)      cprojf(z)
#define creal(z)      crealf(z)
#define csin(z)       csinf(z)
#define csinh(z)      csinhf(z)
#define csqrt(z)      csqrtf(z)
#define ctan(z)       ctanf(z)
#define ctanh(z)      ctanhf(z)

float cabsf(float complex z);
float complex cacosf(float complex z);
float complex cacoshf(float complex z);
float cargf(float complex z);
float complex casinf(float complex z);
float complex casinhf(float complex z);
float complex catanf(float complex z);
float complex catanhf(float complex z);
float complex ccosf(float complex z);
float complex ccoshf(float complex z);
float complex cexpf(float complex z);
float cimagf(float complex z);
float complex clogf(float complex z);
float complex conjf(float complex z);
float complex cpowf(float complex z, float complex w);
float complex cprojf(float complex z);
float crealf(float complex z);
float complex csinf(float complex z);
float complex csinhf(float complex z);
float complex csqrtf(float complex z);
float complex ctanf(float complex z);
float complex ctanhf(float complex z);

#endif

#endif /* _COMPLEX_H */