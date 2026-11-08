#ifndef MOCK_CANCEL_H
#define MOCK_CANCEL_H

#include <signal.h>

namespace pluto {

using CancelFlag = volatile sig_atomic_t;

inline void RequestCancel() {}
inline void ClearCancel() {}
inline bool IsCancelRequested() { return false; }

}  // namespace pluto

#endif