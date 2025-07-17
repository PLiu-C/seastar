#pragma once

#include <cassert>

#ifdef NDEBUG
#define SEASTAR_ASSERT(expr) do {} while (0)
#else
#define SEASTAR_ASSERT(expr) assert(expr)
#endif

namespace seastar {

[[noreturn]] void abort_on_assertion_failure() noexcept;

} // namespace seastar 