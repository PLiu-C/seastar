#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/timer.hh>
#include <chrono>

namespace seastar {

/// Returns a future which completes after a specified time has elapsed.
template <typename Clock = steady_clock_type, typename Rep, typename Period>
future<void> sleep(std::chrono::duration<Rep, Period> dur) {
    struct sleeper {
        promise<void> done;
        timer<Clock> tmr;
        
        sleeper(std::chrono::duration<Rep, Period> dur)
            : tmr([this] { done.set_value(); })
        {
            tmr.arm(dur);
        }
    };
    
    auto s = std::make_unique<sleeper>(dur);
    future<void> fut = s->done.get_future();
    return fut.finally([s = std::move(s)] {});
}

} // namespace seastar 