#pragma once

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <system_error>
#include <seastar/util/assert.hh>

namespace seastar {

inline void throw_system_error_on(bool condition, const char* what_arg = "system call") {
    if (condition) {
        throw std::system_error(errno, std::system_category(), what_arg);
    }
}

template <typename T>
inline void throw_kernel_error(T r) {
    if (r == -1) {
        throw_system_error_on(true);
    }
}

/// File descriptor wrapper
class file_desc {
    int _fd;

public:
    file_desc() noexcept : _fd(-1) {}
    
    explicit file_desc(int fd) noexcept : _fd(fd) {}
    
    ~file_desc() {
        if (_fd != -1) {
            ::close(_fd);
        }
    }
    
    file_desc(const file_desc&) = delete;
    file_desc& operator=(const file_desc&) = delete;
    
    file_desc(file_desc&& other) noexcept : _fd(other._fd) {
        other._fd = -1;
    }
    
    file_desc& operator=(file_desc&& other) noexcept {
        if (this != &other) {
            if (_fd != -1) {
                ::close(_fd);
            }
            _fd = other._fd;
            other._fd = -1;
        }
        return *this;
    }

    int get() const noexcept { return _fd; }
    
    int release() noexcept {
        int fd = _fd;
        _fd = -1;
        return fd;
    }

    // Factory methods
    static file_desc eventfd(unsigned int initval, int flags) {
        int fd = ::eventfd(initval, flags);
        throw_kernel_error(fd);
        return file_desc(fd);
    }

    static file_desc timerfd_create(int clockid, int flags) {
        int fd = ::timerfd_create(clockid, flags);
        throw_kernel_error(fd);
        return file_desc(fd);
    }

    static file_desc epoll_create(int flags) {
        int fd = ::epoll_create1(flags);
        throw_kernel_error(fd);
        return file_desc(fd);
    }

    // I/O operations
    ssize_t read(void* buffer, size_t len) {
        auto result = ::read(_fd, buffer, len);
        throw_kernel_error(result);
        return result;
    }

    ssize_t write(const void* buffer, size_t len) {
        auto result = ::write(_fd, buffer, len);
        throw_kernel_error(result);
        return result;
    }
};

namespace posix {

// Convert duration to itimerspec for timerfd
template <typename Rep, typename Period>
struct itimerspec to_relative_itimerspec(std::chrono::duration<Rep, Period> delta, 
                                       std::chrono::duration<Rep, Period> repeat = {}) {
    struct itimerspec its = {};
    auto delta_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
    auto repeat_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(repeat);
    
    its.it_value.tv_sec = delta_ns.count() / 1000000000;
    its.it_value.tv_nsec = delta_ns.count() % 1000000000;
    its.it_interval.tv_sec = repeat_ns.count() / 1000000000;
    its.it_interval.tv_nsec = repeat_ns.count() % 1000000000;
    
    return its;
}

} // namespace posix

} // namespace seastar 