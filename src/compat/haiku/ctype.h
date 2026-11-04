/*
 * Minimal ctype.h for Haiku compatibility
 */

#ifndef _CTYPE_H
#define _CTYPE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Character classification functions */
int isalnum(int c);
int isalpha(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);

/* Character conversion functions */
int tolower(int c);
int toupper(int c);

/* Character classification functions (C99) */
int isascii(int c);
int toascii(int c);
int _tolower(int c);
int _toupper(int c);

/* Wide character classification */
#include <wctype.h>

#ifdef __cplusplus
}
#endif

#endif /* _CTYPE_H */