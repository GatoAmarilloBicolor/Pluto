#include "cancel.h"

#include <atomic>

namespace pluto {

namespace {
std::atomic<bool> g_cancel(false);
}

void
RequestCancel()
{
	g_cancel.store(true, std::memory_order_relaxed);
}

void
ClearCancel()
{
	g_cancel.store(false, std::memory_order_relaxed);
}

bool
IsCancelRequested()
{
	return g_cancel.load(std::memory_order_relaxed);
}

} // namespace pluto
