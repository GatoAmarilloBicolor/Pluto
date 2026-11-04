/*
 * Minimal wchar.h for Haiku compatibility
 * Provides basic definitions needed by libstdc++ when system headers are missing
 */

#ifndef _WCHAR_H
#define _WCHAR_H

#include <stddef.h>

#if !defined(__cplusplus) || defined(_GLIBCXX_INCLUDE_WCHAR_H) || defined(_GNU_SOURCE) || defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE) || defined(_POSIX_C_SOURCE)

#ifdef __cplusplus
extern "C" {
#endif

/* Wide character type */
#ifndef _WCHAR_T_DEFINED
typedef __WCHAR_TYPE__ wchar_t;
#define _WCHAR_T_DEFINED
#endif

/* Wide integer type */
typedef long wint_t;

/* Wide EOF */
#define WEOF ((wint_t)(-1))

/* Wide character classification functions */
wint_t btowc(int c);
int wctob(wint_t c);

/* Wide string functions */
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n);
wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
wchar_t *wcsncat(wchar_t *dest, const wchar_t *src, size_t n);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
size_t wcslen(const wchar_t *s);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
size_t wcsspn(const wchar_t *s, const wchar_t *accept);
size_t wcscspn(const wchar_t *s, const wchar_t *reject);
wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept);
wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);
wchar_t *wcstok(wchar_t *s, const wchar_t *delim, wchar_t **ptr);

/* Wide character conversion */
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps);

/* Wide character I/O */
struct _IO_FILE;
typedef struct _IO_FILE FILE;

wint_t fgetwc(FILE *stream);
wchar_t *fgetws(wchar_t *ws, int n, FILE *stream);
wint_t fputwc(wchar_t wc, FILE *stream);
int fputws(const wchar_t *ws, FILE *stream);
wint_t getwc(FILE *stream);
wint_t getwchar(void);
wint_t putwc(wchar_t wc, FILE *stream);
wint_t putwchar(wchar_t wc);
wint_t ungetwc(wint_t wc, FILE *stream);

/* Wide character formatted I/O */
int fwscanf(FILE *stream, const wchar_t *format, ...);
int wscanf(const wchar_t *format, ...);
int swscanf(const wchar_t *s, const wchar_t *format, ...);
int vfwscanf(FILE *stream, const wchar_t *format, __gnuc_va_list arg);
int vwscanf(const wchar_t *format, __gnuc_va_list arg);
int vswscanf(const wchar_t *s, const wchar_t *format, __gnuc_va_list arg);
int fwprintf(FILE *stream, const wchar_t *format, ...);
int wprintf(const wchar_t *format, ...);
int swprintf(wchar_t *s, size_t n, const wchar_t *format, ...);
int vfwprintf(FILE *stream, const wchar_t *format, __gnuc_va_list arg);
int vwprintf(const wchar_t *format, __gnuc_va_list arg);
int vswprintf(wchar_t *s, size_t n, const wchar_t *format, __gnuc_va_list arg);

/* Wide character numeric conversion */
long int wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
long long int wcstoll(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long int wcstoul(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long long int wcstoull(const wchar_t *nptr, wchar_t **endptr, int base);
double wcstod(const wchar_t *nptr, wchar_t **endptr);
float wcstof(const wchar_t *nptr, wchar_t **endptr);
long double wcstold(const wchar_t *nptr, wchar_t **endptr);

/* Wide character time functions */
wchar_t *wcsftime(wchar_t *s, size_t maxsize, const wchar_t *format, const struct tm *timeptr);

#ifdef __cplusplus
}
#endif

#endif /* C++ or feature test macros */

#endif /* _WCHAR_H */