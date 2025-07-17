#pragma once

#include <chrono>
#include <functional>
#include <seastar/core/sstring.hh>

namespace seastar {

constexpr unsigned max_scheduling_groups() { return 16; }

template <typename T>
class future;

class reactor;

class scheduling_group;
class scheduling_group_key {
public:
    unsigned _id = 0;
    scheduling_group_key() = default;
    explicit scheduling_group_key(unsigned id) : _id(id) {}
};

using sched_clock = std::chrono::steady_clock;

namespace internal {

// Returns an index between 0 and max_scheduling_groups()
unsigned scheduling_group_index(scheduling_group sg) noexcept;
scheduling_group scheduling_group_from_index(unsigned index) noexcept;

unsigned long scheduling_group_key_id(scheduling_group_key) noexcept;

template<typename T>
T* scheduling_group_get_specific_ptr(scheduling_group sg, scheduling_group_key key) noexcept;

}

/// A scheduling group that can be used to control execution priority
class scheduling_group {
    unsigned _id;
    
public:
    scheduling_group() : _id(0) {}  // Default constructor
    explicit scheduling_group(unsigned id) : _id(id) {}
    
    bool operator==(const scheduling_group& other) const {
        return _id == other._id;
    }
    
    bool operator!=(const scheduling_group& other) const {
        return !(*this == other);
    }
    
    unsigned id() const { return _id; }
};

/// Returns the current scheduling group
scheduling_group current_scheduling_group() noexcept;

/// Default scheduling group for most tasks
extern scheduling_group default_scheduling_group();

/// Creates a new scheduling group
future<scheduling_group> create_scheduling_group(sstring name, float shares) noexcept;

} // namespace seastar 