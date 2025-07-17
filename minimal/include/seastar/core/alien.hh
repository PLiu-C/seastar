#pragma once

#include <functional>

namespace seastar {

namespace alien {

/// Simple alien instance for minimal implementation
class instance {
public:
    /// Submit a function to be executed in the seastar context
    void submit(std::function<void()> func) {
        // Simplified - just execute immediately for minimal version
        func();
    }
};

} // namespace alien

} // namespace seastar 