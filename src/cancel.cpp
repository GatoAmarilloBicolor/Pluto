#include "cancel.h"

#include <atomic>

namespace pluto {

volatile sig_atomic_t g_cancel_flag = 0;

} // namespace pluto