#include <seastar/core/future.hh>

namespace seastar {

// Broken promise implementation
broken_promise::broken_promise() : std::logic_error("broken promise") {}

// Template instantiations for common future types
template class future<void>;
template class future<int>;
template class future<std::string>;

template class promise<void>;
template class promise<int>;
template class promise<std::string>;

} // namespace seastar 