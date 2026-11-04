/*
 * Minimal locale.h for Haiku compatibility
 * Provides basic definitions needed by libstdc++ when system headers are missing
 */

#ifndef _LOCALE_H
#define _LOCALE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Locale categories */
#define LC_CTYPE     0
#define LC_NUMERIC   1
#define LC_TIME      2
#define LC_COLLATE   3
#define LC_MONETARY  4
#define LC_MESSAGES  5
#define LC_ALL       6

/* Locale structure */
struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char  int_frac_digits;
    char  frac_digits;
    char  p_cs_precedes;
    char  p_sep_by_space;
    char  n_cs_precedes;
    char  n_sep_by_space;
    char  p_sign_posn;
    char  n_sign_posn;
    char  int_p_cs_precedes;
    char  int_p_sep_by_space;
    char  int_n_cs_precedes;
    char  int_n_sep_by_space;
    char  int_p_sign_posn;
    char  int_n_sign_posn;
};

/* Locale functions */
char *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

/* Newlocale/freelocale/uselocale (POSIX 2008) */
typedef struct __locale_t *locale_t;
locale_t newlocale(int category_mask, const char *locale, locale_t base);
void freelocale(locale_t loc);
locale_t uselocale(locale_t newloc);

/* Locale category masks */
#define LC_CTYPE_MASK     (1 << LC_CTYPE)
#define LC_NUMERIC_MASK   (1 << LC_NUMERIC)
#define LC_TIME_MASK      (1 << LC_TIME)
#define LC_COLLATE_MASK   (1 << LC_COLLATE)
#define LC_MONETARY_MASK  (1 << LC_MONETARY)
#define LC_MESSAGES_MASK  (1 << LC_MESSAGES)
#define LC_ALL_MASK       (LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | \
                           LC_COLLATE_MASK | LC_MONETARY_MASK | LC_MESSAGES_MASK)

#ifdef __cplusplus
}
#endif

#endif /* _LOCALE_H */