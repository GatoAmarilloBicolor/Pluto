/*
 * Minimal stddef.h for Haiku compatibility
 */

#ifndef _STDDEF_H
#define _STDDEF_H

typedef long ptrdiff_t;
typedef unsigned long size_t;
typedef long wchar_t;

#define NULL ((void*)0)
#define offsetof(t, d) __builtin_offsetof(t, d)

#endif /* _STDDEF_H */