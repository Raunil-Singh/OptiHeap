# OptiHeap — A Hybrid Memory Allocator in C

> A modular, high-performance memory allocator that combines heap-based and mmap-based allocation with optional reference counting, thread safety, and debugging support.

---

## Features

OptiHeap brings together modern memory management strategies into a modular, high-performance allocator suitable for both low-latency and high-throughput environments.

### 🔀 Hybrid Allocation Strategy
- Uses **heap allocation** (via `sbrk`) for small and medium-sized blocks for faster performance.
- Falls back to **mmap-based allocation** for large blocks to avoid heap fragmentation and support memory locality for big data structures.
- Dynamically selects the optimal strategy based on a tunable threshold (`MAX_HEAP_ALLOC_SIZE`).

### 🧠 Smart Pointer–like Reference Counting (Optional)
- Implements a **retain/release model** with atomic reference counters and custom destructors.
- Prevents accidental memory leaks by ensuring blocks are freed when no longer referenced.
- `optiheap_reference_count`, `optiheap_set_destructor` APIs allow deep control over object lifecycle.

### 🧵 Thread Safety (Optional)
- Fully thread-safe when compiled with `-DOPTIHEAP_THREAD_SAFE`.
- Internally guarded by `pthread_mutex` around critical regions in heap and mmap operations.
- No additional locking overhead when thread-safety is disabled.

### 🧩 Modular Architecture
- Clean separation of heap, mmap, reference counting, and orchestration logic.
- Easy to extend for:
  - Custom page allocators
  - Garbage collection schemes
  - Arena-based designs

### 🛠️ Debugging and Safety
- Compile with `-DOPTIHEAP_DEBUGGER` to enable extensive runtime checks.
- Includes:
  - Magic bytes for corruption detection
  - Leak detection with reference counter audits
  - Debug logs with allocation state, memory boundaries, and block info
- Helpful during development and unit testing to catch hard-to-find bugs.

### 🔍 Visual Inspection Tools
- Built-in `debug_print_heap()` and `debug_print_mmap()` allow developers to inspect internal memory state on demand.
- Prints block states, addresses, sizes, and allocation metadata.

### 🧪 Benchmarking Infrastructure
- Compare performance with `glibc malloc` using bundled benchmark suite.
- Tracks:
  - Throughput (allocs/sec)
  - Peak memory
  - Fragmentation
- CSV export and plots for easy visualization and trend tracking.

### 🔄 Safe Deallocation and Coalescing
- Heap allocator aggressively coalesces adjacent free blocks to minimize fragmentation.
- Safely rejects invalid or corrupted pointers with verbose error output.

### ⚙️ Compile-Time Feature Flags
- Fully customizable builds using Makefile flags:
  - `OPTIHEAP_REFERENCE_COUNTING`
  - `OPTIHEAP_THREAD_SAFE`
  - `OPTIHEAP_DEBUGGER`
- Compile lean-and-fast builds for production, or safe-and-verbose builds for dev/test.


---

## Architecture Overview

| Module | Responsibility |
|--------|----------------|
| `optiheap_allocator.h` | Lists all the APIs available |
| `optiheap_allocator.c` | Routes requests to heap or mmap allocator |
| `heap_allocator.c`     | Manages small blocks via segregated free lists |
| `mmap_allocator.c`     | Handles large allocations with page-aligned `mmap()` |
| `reference_counting.c` | Smart-pointer-like layer (optional) |
| `memory_structs.h`     | Common memory header with metadata, magic bytes, and links |

---

## Installation

### Compile with GCC

Set the required optiheap flags in the makefile based on requirements
```
make libraries
```
The default no-flag configuration creates a lean and minimal memory allocation library that largely focusses of performance.

To remove the existing library build use
```
make clean
```

### Library usage example

include the header file `./include/optiheap_allocator.h` in your source code

Using the static library
```
gcc Your_source_code_that_uses_the_library.c ./lib/liboptiheap.a -o Executable_name
```

---

## Compile-Time Flags

These are the flags that must be used in the makefile before building the library.

| Flag                            | Description                                       |
| ------------------------------- | ------------------------------------------------- |
| `-DOPTIHEAP_DEBUGGER`           | Enables verbose memory state printing             |
| `-DOPTIHEAP_REFERENCE_COUNTING` | Enables smart-pointer support                     |
| `-DOPTIHEAP_THREAD_SAFE`        | Adds `pthread_mutex` locking to critical sections |

**Note***: These flags can largely help the enduser but they drain the allocator's performance to do what they do, especially the debug flag. Enabling of the debug flag increases the amounts of safety checks in the source code that can be used to ensure that the code written by the programmer using the library is safe and for this reason it is `highly recommended to use the debug flag in production` but, always construct the `final deployment build without the debug flag` to ensure performance.

---

## Benchmark Results

Benchmarked using a custom suite comparing against glibc malloc.

![image](benchmarks/Benchmarking_results_version4/graph_time_taken.png)
![image](benchmarks/Benchmarking_results_version4/graph_peak_memory.png)

[Elaborate Benchmarking results of the latest version](benchmarks/Benchmarking_results_version4/combined_benchmark_results.csv)

The library has seen a series of improvements to achieve the desired performance

| Version Number | Throughput against glibc | Modifications Made |
| -------------- | ------------------------- | ------------------ |
| 1 | 59.1 % slower | Base Design where the allocator had a lot of check, used a single freelist with best fit |
| 2 | 22.3 % slower | Replaced single freelist + bestfit with segregated freelists with 11 size classes + firstfit |
| 3 | 12.9 % slower | Made debugging and thread-safety an optional feature to reduce the number of unproductive operations |
| 4 | 1.5 % slower | Optimized the block coalescing that happens in heap allocator whenever a block is freed |

The repository also contains some profiling data done with `perf`.

[You can checkout the profiling data here](perf_data/perf.data)
