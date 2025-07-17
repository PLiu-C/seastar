#pragma once

#include <chrono>

namespace seastar {

/// Configuration for the reactor
struct reactor_config {
    /// Task quota duration - how long tasks run before yielding
    std::chrono::nanoseconds task_quota = std::chrono::microseconds(500);
    
    /// Maximum poll time for epoll  
    std::chrono::nanoseconds max_poll_time = std::chrono::microseconds(100);
    
    /// Handle SIGINT signal
    bool handle_sigint = true;
    
    /// Auto handle SIGINT/SIGTERM
    bool auto_handle_sigint_sigterm = true;
    
    /// Maximum task backlog
    unsigned max_task_backlog = 1000;
    
    /// Force poll mode (no sleeping)
    bool force_poll = true;
};

} // namespace seastar 