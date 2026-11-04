/*
 * Minimal assert.h for Haiku compatibility
 */

#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
    ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#endif

void __assert_fail(const char *expr, const char *file, int line, const char *func);

#ifdef __cplusplus
}
#endif

#endif /* _ASSERT_H */