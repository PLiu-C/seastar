# Minimal Seastar Framework

A simplified, poll-mode only implementation of the Seastar framework focused on core functionality. This minimal version provides the essential building blocks for high-performance asynchronous applications while removing complex features and dependencies.

## Features

### ✅ Included Features
- **Poll-mode only reactor** - No sleeping/blocking, continuous polling
- **Epoll backend only** - Linux epoll for I/O event polling  
- **Core affinity support** - SMP multi-core functionality
- **Hugepage memory management** - Efficient memory allocation
- **Futures and promises** - Asynchronous programming primitives
- **Simple task scheduler** - Cooperative task execution
- **Timer system** - Basic timing functionality
- **Semaphores** - Synchronization primitives
- **Basic networking** - Socket addresses, inet_address, IPv4/IPv6 support
- **Utility functions** - Assert, modules, yield, basic async utilities

### ❌ Removed Features
- **Stall detection** - No CPU stall monitoring
- **Logger support** - No built-in logging infrastructure  
- **File I/O** - No seastar::File support
- **DPDK network stack** - Only POSIX networking supported
- **Complex scheduling groups** - Simplified scheduler
- **Multiple reactor backends** - Only epoll
- **Sleep modes** - No blocking/sleeping support

## Requirements

- **C++20** compiler (GCC 10+ or Clang 11+)
- **Linux** operating system
- **CMake 3.16+**
- **POSIX threads** support
- **Hugepage** support (optional but recommended)

## Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the library and test
make -j$(nproc)

# Run the test
./minimal_test
```

## Usage Example

```cpp
#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>
#include <seastar/core/memory.hh>

using namespace seastar;

future<> my_application() {
    std::cout << "Hello from minimal Seastar!" << std::endl;
    
    // Your application logic here
    
    // Stop the reactor when done
    engine().stop();
    return make_ready_future<>();
}

int main() {
    // Configure memory
    memory::configure_minimal();
    
    // Configure SMP
    smp::configure(1);
    
    // Create reactor configuration for poll mode
    reactor_config cfg;
    cfg.force_poll = true;
    
    // Create and run reactor
    auto smp_instance = std::make_shared<smp>();
    alien::instance alien;
    reactor r(smp_instance, alien, 0, cfg);
    
    // Schedule application
    r.run_in_background(my_application());
    
    // Run reactor (poll mode)
    return r.do_run();
}
```

## Architecture

### Core Components

1. **Reactor** (`reactor.hh`, `reactor.cc`)
   - Main event loop using epoll
   - Poll-mode only (no blocking)
   - Task scheduling and execution

2. **Memory Management** (`memory.hh`, `memory.cc`)
   - Hugepage allocation support
   - Per-core memory pools
   - Simple bump allocator

3. **Futures** (`future.hh`, `future.cc`)
   - Promise/future pattern
   - Simplified continuation support
   - Exception handling

4. **SMP** (`smp.hh`, `smp.cc`)
   - Multi-core support
   - Core affinity
   - Simplified inter-shard communication

5. **Scheduling** (`scheduling.hh`, `scheduling.cc`)
   - Basic scheduling groups
   - Task queues
   - Cooperative multitasking

6. **Networking** (`socket_defs.hh`, `inet_address.hh`)
   - Socket address types (IPv4/IPv6)
   - Internet address handling
   - Basic network type definitions

7. **Utilities** (`assert.hh`, `modules.hh`, `later.hh`)
   - Assertion macros for debugging
   - Module export definitions
   - Cooperative yield functionality

### Limitations

- **Basic networking only**: Limited to socket addresses and IP handling, no full protocol stack
- **Simplified futures**: Limited continuation chaining compared to full Seastar
- **Minimal alien threads**: Cross-thread communication is basic
- **Poll mode only**: No support for blocking operations
- **Single backend**: Only epoll support (no io_uring, linux-aio)
- **No advanced features**: No coroutines, advanced scheduling, or complex I/O patterns

## Performance Notes

- Optimized for **poll mode** - continuously polling for events
- Uses **hugepages** when available for better memory performance
- **CPU affinity** support for multi-core scaling
- **Zero-copy** where possible in data structures

## Configuration

The framework can be configured using:
- **Memory size per core** (default: 256MB)
- **Task quota** (default: 500μs)
- **Hugepage usage** (automatic fallback to regular pages)

## Contributing

This is a minimal implementation for specific use cases. For full Seastar features, use the main Seastar repository.

## License

This minimal framework follows the same Apache 2.0 license as the main Seastar project. 