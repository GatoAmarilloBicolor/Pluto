/*
 * Minimal time.h for Haiku compatibility
 */

#ifndef _TIME_H
#define _TIME_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Time types */
typedef long time_t;
typedef long suseconds_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long tm_gmtoff;
    const char *tm_zone;
};

/* Clock IDs */
#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3

/* Functions */
time_t time(time_t *t);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *tm);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
char *asctime(const struct tm *tm);
char *ctime(const time_t *timer);
clock_t clock(void);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clk_id, struct timespec *tp);
int clock_settime(int clk_id, const struct timespec *tp);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H */