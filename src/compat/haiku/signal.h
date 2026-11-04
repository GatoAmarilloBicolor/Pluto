/*
 * Minimal signal.h for Haiku compatibility
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Signal types */
typedef int sig_atomic_t;

/* Signal numbers */
#define SIGHUP     1
#define SIGINT     2
#define SIGQUIT    3
#define SIGILL     4
#define SIGTRAP    5
#define SIGABRT    6
#define SIGBUS     7
#define SIGFPE     8
#define SIGKILL    9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGSTKFLT 16
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20
#define SIGTTIN   21
#define SIGTTOU   22
#define SIGURG    23
#define SIGXCPU   24
#define SIGXFSZ   25
#define SIGVTALRM 26
#define SIGPROF   27
#define SIGWINCH  28
#define SIGIO     29
#define SIGPWR    30
#define SIGSYS    31

#define NSIG      32

/* Signal actions */
#define SIG_DFL   ((void (*)(int))0)
#define SIG_IGN   ((void (*)(int))1)
#define SIG_ERR   ((void (*)(int))-1)

/* Function */
void (*signal(int sig, void (*func)(int)))(int);
int raise(int sig);

#ifdef __cplusplus
}
#endif

#endif /* _SIGNAL_H */