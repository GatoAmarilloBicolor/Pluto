#pragma once

// Minimal structured logging. Controlled by the PLUTO_LOG environment
// variable. When PLUTO_LOG=debug, all log calls emit to stderr with a
// tag prefix. When unset or any other value, only warnings and errors
// are shown.
//
// Usage:
//   pluto::log_debug("mcp", "connected %d tools", n);
//   pluto::log_warn("curl", "retry in %ds", delay);
//   pluto::log_error("provider", "HTTP %d", code);

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace pluto {

namespace detail {

inline bool debug_enabled() {
	// Re-check the env var each time so that setenv("PLUTO_LOG", "debug")
	// at runtime takes effect without restarting the process.
	const char* v = std::getenv("PLUTO_LOG");
	if (!v) return false;
	return v[0] == 'd' && v[1] == 'e' && v[2] == 'b'
	    && v[3] == 'u' && v[4] == 'g' && v[5] == '\0';
}

inline void log_impl(const char* level, const char* tag,
                      const char* fmt, va_list ap) {
	std::fprintf(stderr, "[pluto/%s] %s: ", tag, level);
	std::vfprintf(stderr, fmt, ap);
	std::fprintf(stderr, "\n");
}

}  // namespace detail

inline void log_debug(const char* tag, const char* fmt, ...) {
	if (!detail::debug_enabled()) return;
	va_list ap;
	va_start(ap, fmt);
	detail::log_impl("debug", tag, fmt, ap);
	va_end(ap);
}

inline void log_warn(const char* tag, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	detail::log_impl("warn", tag, fmt, ap);
	va_end(ap);
}

inline void log_error(const char* tag, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	detail::log_impl("error", tag, fmt, ap);
	va_end(ap);
}

}  // namespace pluto
