#pragma once

#include <seastar/core/scheduling.hh>
#include <utility>

namespace seastar {

class task {
protected:
    scheduling_group _sg;

    // Task destruction is performed by run_and_dispose() via a concrete type,
    // so no need for a virtual destructor here. Derived classes that implement
    // run_and_dispose() should be declared final to avoid losing concrete type
    // information via inheritance.
    ~task() = default;

    scheduling_group set_scheduling_group(scheduling_group new_sg) noexcept {
        return std::exchange(_sg, new_sg);
    }

public:
    explicit task(scheduling_group sg = current_scheduling_group()) noexcept : _sg(sg) {}
    
    virtual void run_and_dispose() noexcept = 0;
    
    /// Returns the next task which is waiting for this task to complete execution, or nullptr.
    virtual task* waiting_task() noexcept = 0;
    
    scheduling_group group() const { return _sg; }
};

void schedule(task* t) noexcept;
void schedule_checked(task* t) noexcept;
void schedule_urgent(task* t) noexcept;

} // namespace seastar 