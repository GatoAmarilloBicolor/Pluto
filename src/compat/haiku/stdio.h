/*
 * Minimal stdio.h for Haiku compatibility
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard I/O streams */
struct _IO_FILE;
typedef struct _IO_FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* File operations */
FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);

/* Formatted I/O */
int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vfprintf(FILE *stream, const char *format, __gnuc_va_list arg);
int vprintf(const char *format, __gnuc_va_list arg);
int vsprintf(char *str, const char *format, __gnuc_va_list arg);
int vsnprintf(char *str, size_t size, const char *format, __gnuc_va_list arg);

/* Character I/O */
int fgetc(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
int getc(FILE *stream);
int getchar(void);
int putc(int c, FILE *stream);
int putchar(int c);
int ungetc(int c, FILE *stream);

/* Direct I/O */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

/* Positioning */
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fsetpos(FILE *stream, const fpos_t *pos);

/* Error handling */
int ferror(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);

/* File positioning */
typedef long fpos_t;

/* Other */
int remove(const char *path);
int rename(const char *old, const char *new);
FILE *tmpfile(void);
char *tmpnam(char *s);
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
void setbuf(FILE *stream, char *buf);

/* vprintf family */
int vfscanf(FILE *stream, const char *format, __gnuc_va_list arg);
int vscanf(const char *format, __gnuc_va_list arg);
int vsscanf(const char *s, const char *format, __gnuc_va_list arg);

/* scanf family */
int fscanf(FILE *stream, const char *format, ...);
int scanf(const char *format, ...);
int sscanf(const char *s, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_H */