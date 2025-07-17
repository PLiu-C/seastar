#include <seastar/core/scheduling.hh>
#include <seastar/core/future.hh>
#include <seastar/util/assert.hh>
#include <array>

namespace seastar {

namespace {
    thread_local scheduling_group current_sg(0);
    std::array<bool, max_scheduling_groups()> groups_in_use = {};
    unsigned next_group_id = 1; // 0 is reserved for default
}

namespace internal {

unsigned scheduling_group_index(scheduling_group sg) noexcept {
    return sg.id();
}

scheduling_group scheduling_group_from_index(unsigned index) noexcept {
    SEASTAR_ASSERT(index < max_scheduling_groups());
    return scheduling_group(index);
}

unsigned long scheduling_group_key_id(scheduling_group_key key) noexcept {
    return key._id;
}

template<typename T>
T* scheduling_group_get_specific_ptr(scheduling_group sg, scheduling_group_key key) noexcept {
    return nullptr; // Simplified - no group specific data
}

} // namespace internal

scheduling_group current_scheduling_group() noexcept {
    return current_sg;
}

scheduling_group default_scheduling_group() {
    return scheduling_group(0);
}

future<scheduling_group> create_scheduling_group(sstring name, float shares) noexcept {
    try {
        if (next_group_id >= max_scheduling_groups()) {
            throw std::runtime_error("Too many scheduling groups");
        }
        
        unsigned id = next_group_id++;
        groups_in_use[id] = true;
        
        auto sg = scheduling_group(id);
        return make_ready_future<scheduling_group>(std::move(sg));
    } catch (...) {
        return make_exception_future<scheduling_group>(std::current_exception());
    }
}

// Set current scheduling group (internal function)
void set_current_scheduling_group(scheduling_group sg) noexcept {
    current_sg = sg;
}

} // namespace seastar 