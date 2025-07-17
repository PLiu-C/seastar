#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/scheduling.hh>
#include <chrono>
#include <functional>

namespace seastar {

using steady_clock_type = std::chrono::steady_clock;

template <typename Clock = steady_clock_type>
class timer {
public:
    using time_point = typename Clock::time_point;
    using duration = typename Clock::duration;
    using clock = Clock;
    
private:
    using callback_t = std::function<void()>;
    callback_t _callback;
    time_point _expiry;
    bool _armed = false;
    bool _expired = false;

public:
    timer() = default;
    
    explicit timer(callback_t&& callback) : _callback(std::move(callback)) {}

    timer(const timer&) = delete;
    timer& operator=(const timer&) = delete;
    
    timer(timer&&) = default;
    timer& operator=(timer&&) = default;

    ~timer() {
        cancel();
    }

    /// Sets the callback function to be called when the timer expires.
    void set_callback(callback_t&& callback) {
        _callback = std::move(callback);
    }

    /// Arms the timer to fire after the given duration from now.
    void arm(duration delta) {
        arm(Clock::now() + delta);
    }

    /// Arms the timer to fire at the given time point.
    void arm(time_point until) {
        _expiry = until;
        _armed = true;
        _expired = false;
        // In a full implementation, this would register with the reactor
        // For now, this is simplified
    }

    /// Cancels the timer.
    bool cancel() {
        if (_armed && !_expired) {
            _armed = false;
            return true;
        }
        return false;
    }

    /// Returns true if the timer is armed.
    bool armed() const { return _armed; }

    /// Returns the expiry time point.
    time_point get_timeout() const { return _expiry; }
};

/// Low-resolution clock for efficient timers
class lowres_clock {
public:
    using rep = std::chrono::steady_clock::rep;
    using period = std::chrono::steady_clock::period;
    using duration = std::chrono::steady_clock::duration;
    using time_point = std::chrono::time_point<lowres_clock, duration>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        // Simplified: delegate to steady_clock
        // A real implementation would use a cached value updated periodically
        auto steady_now = std::chrono::steady_clock::now();
        return time_point(steady_now.time_since_epoch());
    }
};

} // namespace seastar 