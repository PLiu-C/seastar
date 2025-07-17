#include <seastar/util/assert.hh>
#include <cstdlib>
#include <iostream>

namespace seastar {

[[noreturn]] void abort_on_assertion_failure() noexcept {
    std::cerr << "Seastar assertion failed. Aborting.\n" << std::flush;
    std::abort();
}

} // namespace seastar 