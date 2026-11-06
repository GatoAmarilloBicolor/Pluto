/*
 * pthread.h for Haiku compatibility
 * Includes real system header then adds Haiku-specific overrides
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure sched_param is fully defined */
#ifndef _SCHED_PARAM_DEFINED
struct sched_param {
    int sched_priority;
};
#define _SCHED_PARAM_DEFINED
#endif

/* Ensure clockid_t is defined */
#ifndef _CLOCKID_T_DEFINED
typedef int clockid_t;
#define _CLOCKID_T_DEFINED
#endif

/* Ensure pid_t is defined */
#ifndef _PID_T_DEFINED
typedef int pid_t;
#define _PID_T_DEFINED
#endif

/* Missing pthread functions needed by libstdc++ */
#ifndef _PTHREAD_CANCEL_DECLARED
int pthread_cancel(pthread_t thread);
#define _PTHREAD_CANCEL_DECLARED
#endif

#ifndef _SCHED_YIELD_DECLARED
int sched_yield(void);
#define _SCHED_YIELD_DECLARED
#endif

/* Missing pthread initializer macros */
#ifndef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER {0}
#endif

#ifndef PTHREAD_COND_INITIALIZER
#define PTHREAD_COND_INITIALIZER {0}
#endif

/* Thread ID function */
#ifndef _PTHREAD_GETTHREADID_NP_DECLARED
pid_t pthread_getthreadid_np(pthread_t thread);
#define _PTHREAD_GETTHREADID_NP_DECLARED
#endif

/* pthread_condattr clock functions */
#ifndef _PTHREAD_CONDATTR_CLOCK_DECLARED
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id);
int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id);
#define _PTHREAD_CONDATTR_CLOCK_DECLARED
#endif

#ifdef __cplusplus
}
#endif

#endif /* _PTHREAD_H */