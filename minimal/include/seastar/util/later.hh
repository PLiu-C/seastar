#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>

namespace seastar {

/// \brief Returns a ready future.
inline
future<void> now() {
    return make_ready_future();
}

/// \brief Returns a future which is not ready but is scheduled to resolve soon.
///
/// Schedules a future to run "soon". yield() can be used to break long-but-finite
/// loops into pieces. Note that if nothing else is runnable,
/// It will not check for I/O, and so an infinite loop with yield() will just
/// burn CPU.
future<void> yield() noexcept;

/// Check if we need to preempt the current task
bool need_preempt() noexcept;

/// Yield the cpu if the task quota is exhausted.
///
/// Check if the current continuation is preempted and yield if so. Otherwise
/// return a ready future.
inline
future<void> maybe_yield() noexcept {
    if (need_preempt()) {
        return yield();
    } else {
        return make_ready_future();
    }
}

} 