#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/task.hh>
#include <seastar/core/scheduling.hh>
#include <seastar/core/smp.hh>
#include <seastar/core/semaphore.hh>
#include <seastar/core/timer.hh>
#include <seastar/core/circular_buffer.hh>
#include <seastar/core/reactor_config.hh>
#include <seastar/core/posix.hh>
#include <seastar/core/memory.hh>
#include <seastar/util/noncopyable_function.hh>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <sys/epoll.h>

namespace seastar {

namespace internal {
class poller;
}

// Forward declarations
class reactor_backend;
class thread_pool;

namespace alien {
class instance;
}

/// The main event loop and scheduler
class reactor {
public:
    using poller = internal::poller;

public:
    struct task_queue {
        explicit task_queue(unsigned id, sstring name, float shares);
        
        int64_t _vruntime = 0;
        float _shares;
        bool _active = false;
        uint8_t _id;
        uint64_t _tasks_processed = 0;
        circular_buffer<task*> _q;
        sstring _name;
    };

    // Core reactor data
    std::shared_ptr<seastar::smp> _smp;
    alien::instance& _alien;
    reactor_config _cfg;
    file_desc _notify_eventfd;
    file_desc _task_quota_timer;
    std::unique_ptr<reactor_backend> _backend;
    
    // Task scheduling
    std::vector<std::unique_ptr<task_queue>> _task_queues;
    circular_buffer_fixed_capacity<task_queue*, 16> _active_task_queues;
    task_queue* _at_destroy_tasks;
    task* _current_task = nullptr;
    
    // State
    unsigned _id = 0;
    bool _stopping = false;
    bool _stopped = false;
    std::atomic<bool> _sleeping = {false};
    uint64_t _global_tasks_processed = 0;
    
    // Pollers
    std::vector<poller*> _pollers;
    
    // Exit functions
    std::vector<noncopyable_function<future<void> ()>> _exit_funcs;

public:
    reactor(std::shared_ptr<seastar::smp> smp, alien::instance& alien, 
            unsigned id, reactor_config cfg);
    ~reactor();

    reactor(const reactor&) = delete;
    reactor& operator=(const reactor&) = delete;

    /// Configure the reactor
    void configure(const reactor_config& cfg);

    /// Get current task
    task* current_task() const { return _current_task; }
    
    /// Set current task
    void set_current_task(task* t) { _current_task = t; }

    /// Add a task to be scheduled
    void add_task(task* t) noexcept;
    
    /// Add a high priority task
    void add_high_priority_task(task* t) noexcept;

    /// Add an urgent task
    void add_urgent_task(task* t) noexcept;

    /// Get the CPU/shard ID
    unsigned cpu_id() const { return _id; }
    
    /// Get SMP instance
    const seastar::smp& smp() const noexcept { return *_smp; }

    /// Wake up the reactor if it's sleeping
    void wakeup();

    /// Run the reactor main loop
    int do_run();

    /// Check if reactor is stopping
    bool stopping() const { return _stopping; }

    /// Stop the reactor
    void stop();

    /// Register an exit function to be called when shutting down
    void at_exit(noncopyable_function<future<void> ()> func);

    /// Run a future in the background
    void run_in_background(future<void> f);

private:
    void run_tasks(task_queue& tq);
    bool have_more_tasks() const;
    void run_some_tasks();
    void activate(task_queue& tq);
    void account_runtime(task_queue& tq, std::chrono::nanoseconds runtime);
    uint64_t pending_task_count() const;
    void request_preemption();
public:
    void handle_signal(int signo);
    
    friend class task;
    friend void schedule(task* t) noexcept;
    friend void schedule_urgent(task* t) noexcept;
};

/// Get the current reactor instance
reactor& engine();

/// Schedule a task for execution
inline void schedule(task* t) noexcept {
    engine().add_task(t);
}

/// Schedule a task for urgent execution  
inline void schedule_urgent(task* t) noexcept {
    engine().add_urgent_task(t);
}

/// Schedule a task with checking
inline void schedule_checked(task* t) noexcept {
    schedule(t);
}

namespace internal {

/// Base class for pollers
class poller {
public:
    virtual ~poller() = default;
    virtual bool poll() = 0;
    virtual bool pure_poll() = 0;
    virtual bool try_enter_interrupt_mode() = 0;
    virtual void exit_interrupt_mode() = 0;
};

} // namespace internal

/// Reactor backend interface (simplified)
class reactor_backend {
public:
    virtual ~reactor_backend() = default;
    
    /// Reap completed kernel operations
    virtual bool reap_kernel_completions() = 0;
    
    /// Submit work to kernel
    virtual bool kernel_submit_work() = 0;
    
    /// Check if kernel events can sleep
    virtual bool kernel_events_can_sleep() const = 0;
    
    /// Wait for and process events
    virtual void wait_and_process_events(const sigset_t* active_sigmask = nullptr) = 0;
    
    /// Signal received
    virtual void signal_received(int signo) = 0;
    
    /// Start tick processing
    virtual void start_tick() = 0;
    
    /// Stop tick processing
    virtual void stop_tick() = 0;
    
    /// Reset preemption monitor
    virtual void reset_preemption_monitor() = 0;
    
    /// Request preemption
    virtual void request_preemption() = 0;
    
    /// Start handling signals
    virtual void start_handling_signal() = 0;
};

/// Simple epoll-based reactor backend for poll mode
class reactor_backend_epoll : public reactor_backend {
    reactor& _r;
    file_desc _epollfd;
    std::atomic<bool> _need_epoll_events = false;

public:
    explicit reactor_backend_epoll(reactor& r);
    virtual ~reactor_backend_epoll() override;

    virtual bool reap_kernel_completions() override;
    virtual bool kernel_submit_work() override;
    virtual bool kernel_events_can_sleep() const override;
    virtual void wait_and_process_events(const sigset_t* active_sigmask) override;
    virtual void signal_received(int signo) override;
    virtual void start_tick() override;
    virtual void stop_tick() override;
    virtual void reset_preemption_monitor() override;
    virtual void request_preemption() override;
    virtual void start_handling_signal() override;

private:
    bool wait_and_process(int timeout, const sigset_t* active_sigmask);
};

} // namespace seastar 