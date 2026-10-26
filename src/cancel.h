#pragma once

#include <signal.h>

// Thread-safe cooperative cancel flag used by the streaming provider to
// abort in-flight requests on SIGINT. Set by the signal handler, checked
// from curl's progress callback, cleared by the REPL before each turn.

namespace pluto {

// Use volatile sig_atomic_t for async-signal-safe access.
// This is the only type guaranteed safe for signal handlers (C++ standard).
extern volatile sig_atomic_t g_cancel_flag;

inline void RequestCancel() {
    g_cancel_flag = 1;
}

inline void ClearCancel() {
    g_cancel_flag = 0;
}

inline bool IsCancelRequested() {
    return g_cancel_flag != 0;
}

}  // namespace pluto
