/*
 * Minimal setjmp.h for Haiku compatibility
 */

#ifndef _SETJMP_H
#define _SETJMP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* jmp_buf type */
typedef long jmp_buf[16];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#ifdef __cplusplus
}
#endif

#endif /* _SETJMP_H */