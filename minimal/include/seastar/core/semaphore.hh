#pragma once

#include <seastar/core/future.hh>
#include <queue>

namespace seastar {

class semaphore {
private:
    long _count;
    std::queue<promise<void>> _waiters;

public:
    explicit semaphore(long count) : _count(count) {}

    /// Returns the number of available units
    long current() const noexcept { return _count; }

    /// Return the number of waiters
    size_t waiters() const noexcept { return _waiters.size(); }

    /// Returns true if the semaphore is broken
    bool is_broken() const noexcept { return false; }  // Simplified - no break support

    /// Consume the given number of units from the semaphore
    /// 
    /// If sufficient units are immediately available, returns a ready future.
    /// Otherwise, returns a future that will become ready when sufficient units are available.
    future<void> wait(size_t nr = 1) {
        if (_count >= static_cast<long>(nr)) {
            _count -= nr;
            return make_ready_future();
        }
        
        auto pr = promise<void>();
        auto fut = pr.get_future();
        _waiters.push(std::move(pr));
        return fut;
    }

    /// Signal the semaphore
    ///
    /// This makes units available to waiters.
    void signal(size_t nr = 1) noexcept {
        _count += nr;
        
        while (!_waiters.empty() && _count > 0) {
            auto pr = std::move(_waiters.front());
            _waiters.pop();
            --_count;
            pr.set_value();
        }
    }

    /// Consume the given number of units without waiting
    ///
    /// Returns false if there are insufficient units available.
    bool try_wait(size_t nr = 1) noexcept {
        if (_count >= static_cast<long>(nr)) {
            _count -= nr;
            return true;
        }
        return false;
    }
};

} // namespace seastar 