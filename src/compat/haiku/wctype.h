/*
 * Minimal wctype.h for Haiku compatibility
 */

#ifndef _WCTYPE_H
#define _WCTYPE_H

#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wide character classification */
typedef const int *wctype_t;

wctype_t wctype(const char *property);
int iswctype(wint_t wc, wctype_t desc);

/* Wide character mapping */
typedef const int *wctrans_t;

wctrans_t wctrans(const char *property);
wint_t towctrans(wint_t wc, wctrans_t desc);

/* Wide character case mapping */
wint_t towlower(wint_t wc);
wint_t towupper(wint_t wc);

/* Standard wide character classes */
#define wctype_alnum  ((wctype_t)0)
#define wctype_alpha  ((wctype_t)1)
#define wctype_blank  ((wctype_t)2)
#define wctype_cntrl  ((wctype_t)3)
#define wctype_digit  ((wctype_t)4)
#define wctype_graph  ((wctype_t)5)
#define wctype_lower  ((wctype_t)6)
#define wctype_print  ((wctype_t)7)
#define wctype_punct  ((wctype_t)8)
#define wctype_space  ((wctype_t)9)
#define wctype_upper  ((wctype_t)10)
#define wctype_xdigit ((wctype_t)11)

#ifdef __cplusplus
}
#endif

#endif /* _WCTYPE_H */