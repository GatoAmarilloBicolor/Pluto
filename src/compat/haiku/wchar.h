/*
 * wchar.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 */

#ifndef _WCHAR_H
#define _WCHAR_H

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure mbstate_t is defined */
#ifndef _MBSTATE_T_DEFINED
typedef struct {
    int __count;
    wchar_t __value;
} mbstate_t;
#define _MBSTATE_T_DEFINED
#endif

/* Ensure wint_t is defined */
#ifndef _WINT_T_DEFINED
typedef long wint_t;
#define _WINT_T_DEFINED
#endif

/* Ensure WEOF is defined */
#ifndef WEOF
#define WEOF ((wint_t)(-1))
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WCHAR_H */