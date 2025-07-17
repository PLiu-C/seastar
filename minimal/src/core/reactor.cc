#include <seastar/core/reactor.hh>
#include <seastar/core/memory.hh>
#include <seastar/core/scheduling.hh>
#include <seastar/core/alien.hh>
#include <seastar/util/assert.hh>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <chrono>
#include <iostream>
#include <thread>

namespace seastar {

namespace {
    thread_local reactor* local_engine = nullptr;
    
    thread_local alien::instance alien_instance;
}

// Task queue implementation
reactor::task_queue::task_queue(unsigned id, sstring name, float shares)
    : _shares(shares), _id(id), _name(std::move(name)), _q(1000) {
    // Simplified task queue initialization
}

// Reactor constructor
reactor::reactor(std::shared_ptr<seastar::smp> smp, alien::instance& alien, 
                unsigned id, reactor_config cfg)
    : _smp(std::move(smp))
    , _alien(alien)  
    , _cfg(cfg)
    , _notify_eventfd(file_desc::eventfd(0, EFD_CLOEXEC))
    , _task_quota_timer(file_desc::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC))
    , _id(id)
    , _active_task_queues()
{
    local_engine = this;
    
    // Create default task queue
    _task_queues.push_back(std::make_unique<task_queue>(0, "main", 1.0f));
    _task_queues.push_back(std::make_unique<task_queue>(1, "atexit", 1.0f));
    _at_destroy_tasks = _task_queues.back().get();
    
    // Create epoll backend  
    _backend = std::make_unique<reactor_backend_epoll>(*this);
    
    // Configure memory for this CPU
    // memory::initialize_cpu_memory(_id, 256 * 1024 * 1024); // TODO: implement later
}

reactor::~reactor() {
    if (local_engine == this) {
        local_engine = nullptr;
    }
}

void reactor::configure(const reactor_config& cfg) {
    _cfg = cfg;
}

void reactor::add_task(task* t) noexcept {
    SEASTAR_ASSERT(t);
    auto& tq = *_task_queues[0]; // Use main task queue
    tq._q.push_back(t);
    if (!tq._active) {
        activate(tq);
    }
}

void reactor::add_high_priority_task(task* t) noexcept {
    add_urgent_task(t); // Same as urgent for simplicity
}

void reactor::add_urgent_task(task* t) noexcept {
    add_task(t); // Simplified - no priority distinction
}

void reactor::wakeup() {
    if (!_sleeping.load(std::memory_order_relaxed)) {
        return;
    }
    
    _sleeping.store(false, std::memory_order_relaxed);
    uint64_t one = 1;
    ::write(_notify_eventfd.get(), &one, sizeof(one));
}

void reactor::run_tasks(task_queue& tq) {
    while (!tq._q.empty()) {
        auto* t = tq._q.front();
        tq._q.pop_front();
        
        _current_task = t;
        t->run_and_dispose();
        _current_task = nullptr;
        
        ++tq._tasks_processed;
        ++_global_tasks_processed;
        
        // Simple task quota check
        if (tq._tasks_processed % 100 == 0) {
            break; // Yield after 100 tasks
        }
    }
    
    if (tq._q.empty()) {
        tq._active = false;
    }
}

bool reactor::have_more_tasks() const {
    for (const auto& tq : _task_queues) {
        if (!tq->_q.empty()) {
            return true;
        }
    }
    return false;
}

void reactor::run_some_tasks() {
    while (!_active_task_queues.empty()) {
        auto* tq = _active_task_queues.front();
        _active_task_queues.pop_front();
        
        if (!tq->_q.empty()) {
            run_tasks(*tq);
            if (!tq->_q.empty()) {
                activate(*tq);
            }
        }
    }
}

void reactor::activate(task_queue& tq) {
    if (!tq._active) {
        tq._active = true;
        _active_task_queues.push_back(&tq);
    }
}

void reactor::account_runtime(task_queue& tq, std::chrono::nanoseconds runtime) {
    // Simplified - no complex accounting
    tq._vruntime += runtime.count();
}

uint64_t reactor::pending_task_count() const {
    uint64_t count = 0;
    for (const auto& tq : _task_queues) {
        count += tq->_q.size();
    }
    return count;
}

void reactor::request_preemption() {
    // Simplified - no preemption in poll mode
}

void reactor::handle_signal(int signo) {
    switch (signo) {
        case SIGINT:
        case SIGTERM:
            std::cout << "Received shutdown signal " << signo << std::endl;
            stop();
            break;
        default:
            break;
    }
}

void reactor::stop() {
    _stopping = true;
}

void reactor::at_exit(noncopyable_function<future<void> ()> func) {
    _exit_funcs.push_back(std::move(func));
}

void reactor::run_in_background(future<void> f) {
    // Simple implementation - just ignore for minimal version
    (void)f;
}

int reactor::do_run() {
    std::cout << "Reactor starting on shard " << _id << std::endl;
    
    // Set up signal handling
    signal(SIGINT, [](int sig) { 
        if (local_engine) {
            local_engine->handle_signal(sig);
        }
    });
    signal(SIGTERM, [](int sig) { 
        if (local_engine) {
            local_engine->handle_signal(sig);
        }
    });
    
    _backend->start_tick();
    
    // Main event loop - poll mode only
    while (!_stopping) {
        // Run pending tasks
        if (have_more_tasks()) {
            run_some_tasks();
        }
        
        // Poll for events (no blocking in poll mode)
        _backend->kernel_submit_work();
        _backend->reap_kernel_completions();
        
        // In poll mode, we always use timeout=0 for non-blocking operation
        if (_cfg.force_poll) {
            _backend->wait_and_process_events(nullptr);
        }
        
        // Simple yield to prevent 100% CPU in some cases
        if (!have_more_tasks()) {
            std::this_thread::yield();
        }
    }
    
    std::cout << "Reactor stopping on shard " << _id << std::endl;
    
    // Run exit functions
    for (auto& func : _exit_funcs) {
        try {
            func().get();
        } catch (...) {
            // Ignore exit function errors
        }
    }
    
    _backend->stop_tick();
    return 0;
}

reactor& engine() {
    SEASTAR_ASSERT(local_engine);
    return *local_engine;
}

// Reactor backend epoll implementation
reactor_backend_epoll::reactor_backend_epoll(reactor& r)
    : _r(r), _epollfd(file_desc::epoll_create(EPOLL_CLOEXEC)) {
    
    // Add notification eventfd to epoll
    ::epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = nullptr;
    auto ret = ::epoll_ctl(_epollfd.get(), EPOLL_CTL_ADD, _r._notify_eventfd.get(), &event);
    SEASTAR_ASSERT(ret == 0);
}

reactor_backend_epoll::~reactor_backend_epoll() = default;

bool reactor_backend_epoll::reap_kernel_completions() {
    // Simplified - no complex kernel completions
    return false;
}

bool reactor_backend_epoll::kernel_submit_work() {
    bool result = false;
    if (_need_epoll_events.load()) {
        result = wait_and_process(0, nullptr);
    }
    return result;
}

bool reactor_backend_epoll::kernel_events_can_sleep() const {
    return !_r._cfg.force_poll;
}

void reactor_backend_epoll::wait_and_process_events(const sigset_t* active_sigmask) {
    wait_and_process(0, active_sigmask); // Always non-blocking in poll mode
}

bool reactor_backend_epoll::wait_and_process(int timeout, const sigset_t* active_sigmask) {
    std::array<epoll_event, 128> events;
    
    int nr = ::epoll_pwait(_epollfd.get(), events.data(), events.size(), timeout, active_sigmask);
    
    if (nr == -1) {
        if (errno == EINTR) {
            return false;
        }
        throw_system_error_on(true, "epoll_pwait");
    }
    
    for (int i = 0; i < nr; ++i) {
        auto& evt = events[i];
        if (evt.data.ptr == nullptr) {
            // Notification event
            char dummy[8];
            _r._notify_eventfd.read(dummy, 8);
            continue;
        }
        
        // Handle other events (simplified)
    }
    
    return nr > 0;
}

void reactor_backend_epoll::signal_received(int signo) {
    _r.handle_signal(signo);
}

void reactor_backend_epoll::start_tick() {
    // Simplified - no complex tick handling
}

void reactor_backend_epoll::stop_tick() {
    // Simplified - no complex tick handling
}

void reactor_backend_epoll::reset_preemption_monitor() {
    // Simplified - no preemption monitoring
}

void reactor_backend_epoll::request_preemption() {
    // Simplified - no preemption in poll mode
}

void reactor_backend_epoll::start_handling_signal() {
    // Simplified - basic signal handling
}

} // namespace seastar 