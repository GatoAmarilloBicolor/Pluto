/*
 * Minimal pthread.h for Haiku compatibility
 * Provides basic POSIX threading definitions needed by libstdc++
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stddef.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Thread identifier */
typedef unsigned long pthread_t;

/* Thread attribute */
typedef struct {
    int __detachstate;
    int __schedpolicy;
    struct sched_param __schedparam;
    int __inheritsched;
    int __scope;
    size_t __guardsize;
    int __stackaddr_set;
    void *__stackaddr;
    size_t __stacksize;
} pthread_attr_t;

/* Mutex */
typedef struct {
    int __lock;
    int __count;
    int __owner;
    int __kind;
    int __nusers;
    void *__spins;
    void *__list;
} pthread_mutex_t;

typedef struct {
    int __mutexkind;
    int __pshared;
    int __robust;
} pthread_mutexattr_t;

/* Condition variable */
typedef struct {
    int __lock;
    int __futex;
    int __total_seq;
    int __wakeup_seq;
    int __woken_seq;
    void *__mutex;
    int __nwaiters;
    int __broadcast_seq;
} pthread_cond_t;

typedef struct {
    int __pshared;
    int __clock;
} pthread_condattr_t;

/* Read-write lock */
typedef struct {
    int __lock;
    int __nr_readers;
    int __readers_wakeup;
    int __writer_wakeup;
    int __nr_readers_queued;
    int __nr_writers_queued;
    int __writer;
    int __shared;
    int __pad1;
    int __pad2;
} pthread_rwlock_t;

typedef struct {
    int __pshared;
    int __kind;
} pthread_rwlockattr_t;

/* Spinlock */
typedef int pthread_spinlock_t;

/* Barrier */
typedef struct {
    int __lock;
    int __count;
    int __total;
    int __waiting;
    int __generation;
} pthread_barrier_t;

typedef struct {
    int __pshared;
} pthread_barrierattr_t;

/* Once control */
typedef int pthread_once_t;
#define PTHREAD_ONCE_INIT 0

/* Key for thread-specific data */
typedef unsigned int pthread_key_t;

/* Thread functions */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
void pthread_exit(void *retval);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);

/* Thread attributes */
int pthread_attr_init(pthread_attr_t *attr);
int pthread_attr_destroy(pthread_attr_t *attr);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);
int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize);

/* Mutex functions */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Mutex attributes */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);

/* Condition variable functions */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime);

/* Condition attributes */
int pthread_condattr_init(pthread_condattr_t *attr);
int pthread_condattr_destroy(pthread_condattr_t *attr);
int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);
int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared);
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id);
int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id);

/* Read-write lock functions */
int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);

/* Read-write lock attributes */
int pthread_rwlockattr_init(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr);
int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *attr, int pshared);
int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *attr, int *pshared);
int pthread_rwlockattr_setkind_np(pthread_rwlockattr_t *attr, int kind);
int pthread_rwlockattr_getkind_np(const pthread_rwlockattr_t *attr, int *kind);

/* Spinlock functions */
int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
int pthread_spin_destroy(pthread_spinlock_t *lock);
int pthread_spin_lock(pthread_spinlock_t *lock);
int pthread_spin_trylock(pthread_spinlock_t *lock);
int pthread_spin_unlock(pthread_spinlock_t *lock);

/* Barrier functions */
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr,
                         unsigned count);
int pthread_barrier_destroy(pthread_barrier_t *barrier);
int pthread_barrier_wait(pthread_barrier_t *barrier);

/* Barrier attributes */
int pthread_barrierattr_init(pthread_barrierattr_t *attr);
int pthread_barrierattr_destroy(pthread_barrierattr_t *attr);
int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared);
int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared);

/* Once control */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));

/* Thread-specific data */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

/* Thread cancellation */
int pthread_setcancelstate(int state, int *oldstate);
int pthread_setcanceltype(int type, int *oldtype);
void pthread_testcancel(void);

/* Mutex types */
#define PTHREAD_MUTEX_NORMAL         0
#define PTHREAD_MUTEX_RECURSIVE      1
#define PTHREAD_MUTEX_ERRORCHECK     2
#define PTHREAD_MUTEX_DEFAULT        PTHREAD_MUTEX_NORMAL

/* Mutex attributes */
#define PTHREAD_MUTEX_ROBUST         1
#define PTHREAD_MUTEX_STALLED        2

/* Detach state */
#define PTHREAD_CREATE_JOINABLE      0
#define PTHREAD_CREATE_DETACHED      1

/* Scope */
#define PTHREAD_SCOPE_SYSTEM         0
#define PTHREAD_SCOPE_PROCESS        1

/* Inherit scheduler */
#define PTHREAD_INHERIT_SCHED        0
#define PTHREAD_EXPLICIT_SCHED       1

/* Cancellation */
#define PTHREAD_CANCEL_ENABLE        0
#define PTHREAD_CANCEL_DISABLE       1
#define PTHREAD_CANCEL_DEFERRED      0
#define PTHREAD_CANCEL_ASYNCHRONOUS  1
#define PTHREAD_CANCELED             ((void *)-1)

/* Read-write lock kinds */
#define PTHREAD_RWLOCK_DEFAULT_NP    0
#define PTHREAD_RWLOCK_PREFER_READER_NP  1
#define PTHREAD_RWLOCK_PREFER_WRITER_NP  2
#define PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP  3

/* Spinlock */
#define PTHREAD_PROCESS_PRIVATE      0
#define PTHREAD_PROCESS_SHARED       1

/* Error codes */
#define EPERM            1
#define EINVAL           22
#define EBUSY            16
#define EAGAIN           11
#define EDEADLK          35
#define ETIMEDOUT        110

/* Scheduling */
struct sched_param {
    int sched_priority;
};

int pthread_getschedparam(pthread_t thread, int *policy, struct sched_param *param);
int pthread_setschedparam(pthread_t thread, int policy, const struct sched_param *param);
int pthread_getconcurrency(void);
int pthread_setconcurrency(int level);

/* Thread ID */
pid_t pthread_getthreadid_np(pthread_t thread);

#ifdef __cplusplus
}
#endif

#endif /* _PTHREAD_H */