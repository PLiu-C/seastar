#include <seastar/util/later.hh>
#include <seastar/core/reactor.hh>

namespace seastar {

future<void> yield() noexcept {
    // For the minimal implementation, just return a ready future
    // This doesn't actually yield but provides the interface
    return make_ready_future();
}

bool need_preempt() noexcept {
    // For simplicity, we don't implement complex preemption detection
    // Just return false for now
    return false;
}

} 