// RAII file lock using flock(2).
// Safe for concurrent pluto instances accessing shared config/state files.

#pragma once

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <ctime>
#include <string>

namespace pluto {

class ScopedFileLock {
public:
	// Open (or create) lock_path and acquire an exclusive lock.
	// If timeout_ms > 0, retry with 10ms sleeps until timeout.
	// If timeout_ms == 0, block indefinitely (LOCK_EX).
	explicit ScopedFileLock(const std::string& lock_path, int timeout_ms = 2000)
		: fd_(-1), locked_(false)
	{
		fd_ = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0644);
		if (fd_ < 0) return;

		if (timeout_ms <= 0) {
			// Blocking lock.
			locked_ = (::flock(fd_, LOCK_EX) == 0);
			return;
		}

		// Non-blocking with retry.
		struct timespec ts_start;
		clock_gettime(CLOCK_MONOTONIC, &ts_start);

		for (;;) {
			if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) {
				locked_ = true;
				return;
			}
			if (errno != EWOULDBLOCK) return;

			struct timespec ts_now;
			clock_gettime(CLOCK_MONOTONIC, &ts_now);
			int elapsed_ms = (int)((ts_now.tv_sec - ts_start.tv_sec) * 1000
				+ (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000);
			if (elapsed_ms >= timeout_ms) return;

			// Sleep 10ms.
			struct timespec sl = { 0, 10000000 };
			nanosleep(&sl, nullptr);
		}
	}

	~ScopedFileLock() {
		if (fd_ >= 0) {
			if (locked_) ::flock(fd_, LOCK_UN);
			::close(fd_);
		}
	}

	bool locked() const { return locked_; }

	// Non-copyable, non-movable.
	ScopedFileLock(const ScopedFileLock&) = delete;
	ScopedFileLock& operator=(const ScopedFileLock&) = delete;

private:
	int fd_;
	bool locked_;
};

}  // namespace pluto
