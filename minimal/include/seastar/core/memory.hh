#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace seastar {

namespace memory {

static constexpr size_t page_size = 4096;
static constexpr size_t huge_page_size = 2 * 1024 * 1024; // 2MB

/// Configure memory subsystem
/// This is a simplified version that doesn't require complex NUMA configuration
void configure_minimal();

/// Allocate memory
void* allocate(size_t size);

/// Free memory
void free(void* ptr, size_t size);

/// Check if pointer is from seastar memory pool
bool is_seastar_memory(void* ptr);

/// Memory resource configuration
struct memory_range {
    char* start;
    char* end;
};

struct memory_layout {
    std::vector<memory_range> ranges;
};

/// Basic memory allocation functions
class standard_allocator {
public:
    static void* allocate(size_t size) {
        return ::operator new(size);
    }
    
    static void deallocate(void* ptr, size_t) {
        ::operator delete(ptr);
    }
};

/// Set memory allocator (simplified interface)
void set_allocator(standard_allocator* alloc);

} // namespace memory

} // namespace seastar 