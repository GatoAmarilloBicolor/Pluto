#pragma once

#include <curl/curl.h>
#include <unistd.h>
#include <fcntl.h>

namespace pluto {

// RAII guard for curl_slist with OOM-safe append.
struct CurlSlist {
    curl_slist* list = nullptr;
    CurlSlist() = default;
    ~CurlSlist() { if (list) curl_slist_free_all(list); }

    CurlSlist(const CurlSlist&) = delete;
    CurlSlist& operator=(const CurlSlist&) = delete;

    CurlSlist(CurlSlist&& other) noexcept : list(other.list) { other.list = nullptr; }
    CurlSlist& operator=(CurlSlist&& other) noexcept {
        if (this != &other) {
            if (list) curl_slist_free_all(list);
            list = other.list;
            other.list = nullptr;
        }
        return *this;
    }

    // Append header; returns false on OOM (list unchanged).
    bool append(const char* s) {
        curl_slist* next = curl_slist_append(list, s);
        if (!next) return false;
        list = next;
        return true;
    }

    operator curl_slist*() const { return list; }
    curl_slist* get() const { return list; }
    void release() { list = nullptr; }
};

// RAII guard for file descriptor.
struct FdGuard {
    int fd = -1;
    FdGuard() = default;
    explicit FdGuard(int f) : fd(f) {}
    ~FdGuard() { if (fd >= 0) close(fd); }

    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

    FdGuard(FdGuard&& other) noexcept : fd(other.fd) { other.fd = -1; }
    FdGuard& operator=(FdGuard&& other) noexcept {
        if (this != &other) {
            if (fd >= 0) close(fd);
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }

    int get() const { return fd; }
    int release() { int f = fd; fd = -1; return f; }
    void reset(int f = -1) { if (fd >= 0) close(fd); fd = f; }
};

// Set FD_CLOEXEC on a file descriptor; returns false on error.
inline bool set_cloexec(int fd) {
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

// Create a pipe with FD_CLOEXEC on both ends; returns false on error.
inline bool pipe_cloexec(int pipefd[2]) {
    if (pipe(pipefd) != 0) return false;
    if (!set_cloexec(pipefd[0]) || !set_cloexec(pipefd[1])) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }
    return true;
}

}  // namespace pluto