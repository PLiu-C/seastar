#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/shard_id.hh>
#include <functional>

namespace seastar {

/// SMP (Symmetric Multi-Processing) functionality
class smp {
public:
    /// Returns the number of processing units (cores) available
    static unsigned count;
    
    /// Returns the current shard (core) ID
    static shard_id this_shard_id() noexcept;
    
    /// Submit a task to run on a specific shard
    template <typename Func>
    static future<std::invoke_result_t<Func>> submit_to(shard_id id, Func&& func);
    
    /// Submit a task to run on all shards
    template <typename Func>
    static future<void> invoke_on_all(Func&& func);
    
    /// Initialize SMP subsystem
    static void configure(unsigned nr_cpus);
    
    /// Cleanup SMP subsystem
    static void cleanup();
};

/// Get current shard ID (convenience function)
inline shard_id this_shard_id() noexcept {
    return smp::this_shard_id();
}

/// Submit task to specific shard (convenience function)
template <typename Func>
inline auto submit_to(shard_id id, Func&& func) {
    return smp::submit_to(id, std::forward<Func>(func));
}

/// Invoke on all shards (convenience function)
template <typename Func>
inline auto invoke_on_all(Func&& func) {
    return smp::invoke_on_all(std::forward<Func>(func));
}

namespace internal {

/// Initialize alien instance for current thread
void smp_init_alien_instance();

} // namespace internal

} // namespace seastar 