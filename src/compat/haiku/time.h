/*
 * time.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 */

#ifndef _TIME_H
#define _TIME_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure clock_t is defined */
#ifndef _CLOCK_T_DEFINED
typedef long clock_t;
#define _CLOCK_T_DEFINED
#endif

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H */