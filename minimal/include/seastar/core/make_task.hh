#pragma once

#include <seastar/core/future.hh>
#include <seastar/core/task.hh>
#include <seastar/core/scheduling.hh>

namespace seastar {

template <typename Func>
class lambda_task final : public task {
    Func _func;
    using futurator = futurize<std::invoke_result_t<Func>>;
    typename futurator::promise_type _result;
    
public:
    lambda_task(scheduling_group sg, const Func& func) : task(sg), _func(func) {}
    lambda_task(scheduling_group sg, Func&& func) : task(sg), _func(std::move(func)) {}
    
    typename futurator::type get_future() noexcept { return _result.get_future(); }
    
    virtual void run_and_dispose() noexcept override {
        futurator::invoke(_func).forward_to(std::move(_result));
        delete this;
    }
    
    virtual task* waiting_task() noexcept override {
        return _result.waiting_task();
    }
};

template <typename Func>
inline lambda_task<Func>* make_task(Func&& func) noexcept {
    return new lambda_task<Func>(current_scheduling_group(), std::forward<Func>(func));
}

template <typename Func>
inline lambda_task<Func>* make_task(scheduling_group sg, Func&& func) noexcept {
    return new lambda_task<Func>(sg, std::forward<Func>(func));
}

} // namespace seastar 