#pragma once

namespace seastar { namespace internal {

// Null implementation with the same public interface as the real detector
class cpu_stall_detector {
public:
    explicit cpu_stall_detector(cpu_stall_detector_config cfg = {}) {}
    ~cpu_stall_detector() = default;

    // --- Methods called by reactor (empty implementations) ---
    void start_task_run(sched_clock::time_point) {}
    void end_task_run(sched_clock::time_point) {}
    void start_sleep() {}
    void end_sleep() {}
    void update_config(cpu_stall_detector_config) {}
    cpu_stall_detector_config get_config() const { return {}; }
    static int signal_number() { return SIGRTMIN + 1; }

    using clock_type = thread_cputime_clock;

    // Note: on_signal, generate_trace, etc. are not needed
    // because the parts of the reactor using them (signal handler)
    // will be conditionally compiled out.
};

// Factory function returning the Null detector
inline std::unique_ptr<cpu_stall_detector> make_cpu_stall_detector(cpu_stall_detector_config cfg = {}) {
    return std::make_unique<cpu_stall_detector>(cfg);
}

}} // namespace seastar::internal