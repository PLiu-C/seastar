#include <seastar/core/memory.hh>
#include <seastar/util/assert.hh>
#include <cstdlib>
#include <sys/mman.h>
#include <new>
#include <unordered_set>
#include <mutex>

namespace seastar {

namespace memory {

namespace {
    std::mutex memory_mutex;
    std::unordered_set<void*> seastar_allocations;
    standard_allocator* current_allocator = nullptr;
    bool initialized = false;
}

void configure_minimal() {
    std::lock_guard<std::mutex> lock(memory_mutex);
    initialized = true;
    // In a full implementation, this would set up hugepage memory pools
    // For minimal version, we just track that memory is configured
}

void* allocate(size_t size) {
    if (current_allocator) {
        auto ptr = current_allocator->allocate(size);
        std::lock_guard<std::mutex> lock(memory_mutex);
        seastar_allocations.insert(ptr);
        return ptr;
    }
    
    // Fallback to standard allocation
    auto ptr = std::malloc(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    std::lock_guard<std::mutex> lock(memory_mutex);
    seastar_allocations.insert(ptr);
    return ptr;
}

void free(void* ptr, size_t size) {
    if (!ptr) return;
    
    {
        std::lock_guard<std::mutex> lock(memory_mutex);
        auto it = seastar_allocations.find(ptr);
        if (it != seastar_allocations.end()) {
            seastar_allocations.erase(it);
        }
    }
    
    if (current_allocator) {
        current_allocator->deallocate(ptr, size);
    } else {
        std::free(ptr);
    }
}

bool is_seastar_memory(void* ptr) {
    std::lock_guard<std::mutex> lock(memory_mutex);
    return seastar_allocations.find(ptr) != seastar_allocations.end();
}

void set_allocator(standard_allocator* alloc) {
    current_allocator = alloc;
}

// Hugepage support functions (simplified)
namespace {

void* allocate_hugepage_memory(size_t size) {
    // Try to allocate using hugepages
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    
    if (ptr == MAP_FAILED) {
        // Fallback to regular pages
        ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }
    }
    
    return ptr;
}

void free_hugepage_memory(void* ptr, size_t size) {
    if (ptr && size > 0) {
        munmap(ptr, size);
    }
}

} // anonymous namespace

// Memory management for per-core allocation
class cpu_memory_pool {
    void* _memory_base = nullptr;
    size_t _memory_size = 0;
    size_t _allocated = 0;
    unsigned _cpu_id;
    
public:
    explicit cpu_memory_pool(unsigned cpu_id) : _cpu_id(cpu_id) {}
    
    ~cpu_memory_pool() {
        if (_memory_base) {
            free_hugepage_memory(_memory_base, _memory_size);
        }
    }
    
    void initialize(size_t size) {
        SEASTAR_ASSERT(!_memory_base);
        _memory_size = size;
        _memory_base = allocate_hugepage_memory(size);
    }
    
    void* allocate(size_t size, size_t alignment = sizeof(void*)) {
        // Simple bump allocator
        size_t aligned_offset = (_allocated + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + size > _memory_size) {
            return nullptr; // Out of memory
        }
        
        void* ptr = static_cast<char*>(_memory_base) + aligned_offset;
        _allocated = aligned_offset + size;
        return ptr;
    }
    
    bool owns(void* ptr) const {
        return ptr >= _memory_base && 
               ptr < static_cast<char*>(_memory_base) + _memory_size;
    }
};

namespace {
    thread_local std::unique_ptr<cpu_memory_pool> cpu_pool;
}

void initialize_cpu_memory(unsigned cpu_id, size_t size) {
    cpu_pool = std::make_unique<cpu_memory_pool>(cpu_id);
    cpu_pool->initialize(size);
}

void* allocate_from_cpu_pool(size_t size, size_t alignment) {
    if (cpu_pool) {
        return cpu_pool->allocate(size, alignment);
    }
    return nullptr;
}

} // namespace memory

} // namespace seastar 