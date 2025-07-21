## Changes Summary

### 1. **Enhanced SMP Options Configuration** (`include/seastar/core/smp_options.hh`)
- Added `per_core_memory` field to allow specifying memory for individual cores
- Format: `"cpu0:size0,cpu1:size1,..."` (e.g., `"0:2G,1:1G,2:4G"`)

### 2. **Extended Resource Configuration** (`include/seastar/core/resource.hh`)
- Added `std::unordered_map<unsigned, size_t> per_core_memory` to the configuration structure
- Maps CPU ID to memory size in bytes

### 3. **Created Memory Parsing Logic** (`src/core/reactor.cc`)
- Added `parse_per_core_memory()` function to parse the configuration string
- Includes validation for duplicate CPU specifications and proper error handling
- Supports memory size suffixes (G, M, K) via existing `parse_memory_size()` function

### 4. **Updated Resource Allocation Logic** (`src/core/resource.cc`)

**For non-HWLOC systems:**
- Modified the allocation function to check for per-core memory configuration
- If specified, uses individual core memory sizes
- Cores not specified get equal share of remaining memory

**For HWLOC systems:**
- Enhanced the allocation logic to support per-core memory
- Maintains NUMA-aware allocation while respecting per-core memory limits
- Proper alignment and memory limit enforcement

### 5. **Added Command Line Option** (`src/core/reactor.cc`)
- Registered `--per-core-memory` command line option
- Integrated parsing into the configuration pipeline

### 6. **Enhanced Documentation** (`include/seastar/core/smp.hh`)
- Added comprehensive documentation explaining both global and per-core memory options
- Described behavior when per-core memory is specified

## Key Features

✅ **Backward Compatibility**: Existing equal memory distribution works unchanged  
✅ **Flexible Configuration**: Mix of specified and unspecified cores supported  
✅ **Validation**: Proper error handling for invalid configurations  
✅ **Memory Alignment**: Respects existing memory alignment requirements  
✅ **NUMA Awareness**: Works with both HWLOC and non-HWLOC configurations  
✅ **Memory Limits**: Enforces 36-bit memory address limits  

## Usage Example

```bash
# Equal distribution (existing behavior)
./app --smp 4 --memory 8G

# Per-core specification
./app --smp 4 --per-core-memory "0:2G,1:1G,2:4G,3:1G"

# Mixed: specify some cores, others get equal share of remaining
./app --smp 4 --memory 8G --per-core-memory "0:4G,1:2G"
# Cores 2,3 would get 1G each (8G total - 6G specified = 2G / 2 cores)
```

The implementation maintains Seastar's existing memory management principles while providing fine-grained control over per-core memory allocation, which can be useful for workloads with asymmetric memory requirements across cores.