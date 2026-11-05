/*
 * wctype.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 */

#ifndef _WCTYPE_H
#define _WCTYPE_H

#include_next <wctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure wint_t is defined (may be missing from system header on some configurations) */
#ifndef _WINT_T_DEFINED
typedef long wint_t;
#define _WINT_T_DEFINED
#endif

/* Ensure wctype_t and wctrans_t are defined */
#ifndef _WCTYPE_T_DEFINED
typedef const int *wctype_t;
#define _WCTYPE_T_DEFINED
#endif

#ifndef _WCTRANS_T_DEFINED
typedef const int *wctrans_t;
#define _WCTRANS_T_DEFINED
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WCTYPE_H */