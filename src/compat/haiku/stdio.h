/*
 * stdio.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure __gnuc_va_list is available (needed by libstdc++) */
#ifndef __GNUC_VA_LIST
#include <stdarg.h>
typedef __builtin_va_list __gnuc_va_list;
#define __GNUC_VA_LIST
#endif

/* Ensure fpos_t is defined */
#ifndef _FPOS_T_DEFINED
typedef long fpos_t;
#define _FPOS_T_DEFINED
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */