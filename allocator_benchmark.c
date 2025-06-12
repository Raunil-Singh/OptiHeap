#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <assert.h>
#include <stdint.h>

#ifdef USE_OPTIHEAP
#include "optiheap_allocator.h"
#define MALLOC(size) optiheap_allocate(size)
#define FREE(ptr) optiheap_free(ptr)
#define ALLOCATOR_NAME "OptiHeap"
#define INITIALIZE_ALLOCATOR() optiheap_allocator_init()
#else
#define MALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#define ALLOCATOR_NAME "glibc"
#define INITIALIZE_ALLOCATOR()
#endif

// Memory constraints (in bytes)
#define MAX_MEMORY_USAGE (12ULL * 1024 * 1024 * 1024) // 12GB
#define CHUNK_SIZE_FOR_POPULATION 4096 // 4KB chunks for memory population

// Test configuration
typedef struct {
    const char* name;
    size_t min_size;
    size_t max_size;
    size_t num_allocations;
    double fragmentation_factor; // 0.0 = no fragmentation, 1.0 = max fragmentation
} benchmark_config_t;

// Benchmark result structure
typedef struct {
    const char* test_name;
    const char* allocator_name;
    double total_time_ms;
    size_t total_operations;
    double kops_per_sec;
    size_t peak_memory_usage;
    size_t total_allocated;
    size_t total_freed;
} benchmark_result_t;

// Memory tracking
static size_t current_memory_usage = 0;
static size_t peak_memory_usage = 0;
static size_t total_allocated = 0;
static size_t total_freed = 0;

// Utility functions
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void update_memory_stats(size_t size, int is_allocation) {
    if (is_allocation) {
        current_memory_usage += size;
        total_allocated += size;
        if (current_memory_usage > peak_memory_usage) {
            peak_memory_usage = current_memory_usage;
        }
    } else {
        current_memory_usage -= size;
        total_freed += size;
    }
}

// Populate memory to ensure real page allocation
static void populate_memory(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
    uint8_t* mem = (uint8_t*)ptr;
    size_t chunks = (size + CHUNK_SIZE_FOR_POPULATION - 1) / CHUNK_SIZE_FOR_POPULATION;
    
    for (size_t i = 0; i < chunks; i++) {
        size_t offset = i * CHUNK_SIZE_FOR_POPULATION;
        size_t write_size = (offset + CHUNK_SIZE_FOR_POPULATION > size) ? 
                           (size - offset) : CHUNK_SIZE_FOR_POPULATION;
        
        // Write pattern to ensure page allocation
        memset(mem + offset, (uint8_t)(i & 0xFF), write_size);
        
        // Read back to ensure memory is actually accessed
        volatile uint8_t dummy = mem[offset];
        (void)dummy;
    }
}

// Memory allocation wrapper with tracking
static void* tracked_malloc(size_t size) {
    if (current_memory_usage + size > MAX_MEMORY_USAGE) {
        return NULL; // Would exceed memory limit
    }
    
    void* ptr = MALLOC(size);
    if (ptr) {
        update_memory_stats(size, 1);
        populate_memory(ptr, size);
    }
    return ptr;
}

static void tracked_free(void* ptr, size_t size) {
    if (ptr) {
        FREE(ptr);
        update_memory_stats(size, 0);
    }
}

// Test 1: Sequential allocation and deallocation
static benchmark_result_t test_sequential(const benchmark_config_t* config) {
    printf("Running %s test with %s allocator...\n", config->name, ALLOCATOR_NAME);
    
    void** ptrs = MALLOC(config->num_allocations * sizeof(void*));
    size_t* sizes = MALLOC(config->num_allocations * sizeof(size_t));
    
    if (!ptrs || !sizes) {
        fprintf(stderr, "Failed to allocate test arrays\n");
        exit(1);
    }
    
    // Reset memory tracking
    current_memory_usage = 0;
    peak_memory_usage = 0;
    total_allocated = 0;
    total_freed = 0;
    
    double start_time = get_time_ms();
    size_t successful_ops = 0;
    
    // Allocation phase
    for (size_t i = 0; i < config->num_allocations; i++) {
        sizes[i] = config->min_size + (rand() % (config->max_size - config->min_size + 1));
        ptrs[i] = tracked_malloc(sizes[i]);
        
        if (ptrs[i]) {
            successful_ops++;
        } else {
            ptrs[i] = NULL;
            sizes[i] = 0;
        }
    }
    
    // Deallocation phase
    for (size_t i = 0; i < config->num_allocations; i++) {
        if (ptrs[i]) {
            tracked_free(ptrs[i], sizes[i]);
            successful_ops++;
        }
    }
    
    double end_time = get_time_ms();
    double total_time = end_time - start_time;
    
    benchmark_result_t result = {
        .test_name = config->name,
        .allocator_name = ALLOCATOR_NAME,
        .total_time_ms = total_time,
        .total_operations = successful_ops,
        .kops_per_sec = (successful_ops / (total_time / 1000.0)) / 1000.0,
        .peak_memory_usage = peak_memory_usage,
        .total_allocated = total_allocated,
        .total_freed = total_freed
    };
    
    FREE(ptrs);
    FREE(sizes);
    return result;
}

// Test 2: Random allocation/deallocation pattern
static benchmark_result_t test_random_pattern(const benchmark_config_t* config) {
    printf("Running %s test with %s allocator...\n", config->name, ALLOCATOR_NAME);
    
    const size_t max_live_allocations = config->num_allocations / 4;
    void** live_ptrs = MALLOC(max_live_allocations * sizeof(void*));
    size_t* live_sizes = MALLOC(max_live_allocations * sizeof(size_t));
    
    if (!live_ptrs || !live_sizes) {
        fprintf(stderr, "Failed to allocate test arrays\n");
        exit(1);
    }
    
    for (size_t i = 0; i < max_live_allocations; i++) {
        live_ptrs[i] = NULL;
        live_sizes[i] = 0;
    }
    
    // Reset memory tracking
    current_memory_usage = 0;
    peak_memory_usage = 0;
    total_allocated = 0;
    total_freed = 0;
    
    double start_time = get_time_ms();
    size_t successful_ops = 0;
    size_t live_count = 0;
    
    for (size_t op = 0; op < config->num_allocations; op++) {
        // Randomly decide to allocate or free (bias towards allocation initially)
        int should_allocate = (live_count == 0) || 
                             (live_count < max_live_allocations / 2) ||
                             (rand() % 100 < 60); // 60% allocation probability
        
        if (should_allocate && live_count < max_live_allocations) {
            // Find empty slot
            size_t slot = 0;
            while (slot < max_live_allocations && live_ptrs[slot] != NULL) {
                slot++;
            }
            
            if (slot < max_live_allocations) {
                size_t size = config->min_size + (rand() % (config->max_size - config->min_size + 1));
                void* ptr = tracked_malloc(size);
                
                if (ptr) {
                    live_ptrs[slot] = ptr;
                    live_sizes[slot] = size;
                    live_count++;
                    successful_ops++;
                }
            }
        } else if (live_count > 0) {
            // Free a random allocation
            size_t slot = rand() % max_live_allocations;
            while (live_ptrs[slot] == NULL) {
                slot = (slot + 1) % max_live_allocations;
            }
            
            tracked_free(live_ptrs[slot], live_sizes[slot]);
            live_ptrs[slot] = NULL;
            live_sizes[slot] = 0;
            live_count--;
            successful_ops++;
        }
    }
    
    // Clean up remaining allocations
    for (size_t i = 0; i < max_live_allocations; i++) {
        if (live_ptrs[i]) {
            tracked_free(live_ptrs[i], live_sizes[i]);
            successful_ops++;
        }
    }
    
    double end_time = get_time_ms();
    double total_time = end_time - start_time;
    
    benchmark_result_t result = {
        .test_name = config->name,
        .allocator_name = ALLOCATOR_NAME,
        .total_time_ms = total_time,
        .total_operations = successful_ops,
        .kops_per_sec = (successful_ops / (total_time / 1000.0)) / 1000.0,
        .peak_memory_usage = peak_memory_usage,
        .total_allocated = total_allocated,
        .total_freed = total_freed
    };
    
    FREE(live_ptrs);
    FREE(live_sizes);
    return result;
}

// Test 3: Fragmentation stress test
static benchmark_result_t test_fragmentation(const benchmark_config_t* config) {
    printf("Running %s test with %s allocator...\n", config->name, ALLOCATOR_NAME);
    
    const size_t pattern_size = 1000;
    void** ptrs = MALLOC(pattern_size * sizeof(void*));
    size_t* sizes = MALLOC(pattern_size * sizeof(size_t));
    
    if (!ptrs || !sizes) {
        fprintf(stderr, "Failed to allocate test arrays\n");
        exit(1);
    }
    
    // Reset memory tracking
    current_memory_usage = 0;
    peak_memory_usage = 0;
    total_allocated = 0;
    total_freed = 0;
    
    double start_time = get_time_ms();
    size_t successful_ops = 0;
    size_t total_ops = 0;
    
    while (total_ops < config->num_allocations) {
        // Allocate pattern
        for (size_t i = 0; i < pattern_size && total_ops < config->num_allocations; i++) {
            sizes[i] = config->min_size + (rand() % (config->max_size - config->min_size + 1));
            ptrs[i] = tracked_malloc(sizes[i]);
            
            if (ptrs[i]) {
                successful_ops++;
            } else {
                ptrs[i] = NULL;
                sizes[i] = 0;
            }
            total_ops++;
        }
        
        // Free every other allocation to create fragmentation
        for (size_t i = 0; i < pattern_size; i += 2) {
            if (ptrs[i]) {
                tracked_free(ptrs[i], sizes[i]);
                ptrs[i] = NULL;
                successful_ops++;
            }
        }
        
        // Allocate again in the gaps
        for (size_t i = 0; i < pattern_size && total_ops < config->num_allocations; i += 2) {
            if (!ptrs[i]) {
                size_t new_size = config->min_size + (rand() % (config->max_size - config->min_size + 1));
                ptrs[i] = tracked_malloc(new_size);
                
                if (ptrs[i]) {
                    sizes[i] = new_size;
                    successful_ops++;
                } else {
                    sizes[i] = 0;
                }
                total_ops++;
            }
        }
        
        // Free all remaining allocations
        for (size_t i = 0; i < pattern_size; i++) {
            if (ptrs[i]) {
                tracked_free(ptrs[i], sizes[i]);
                ptrs[i] = NULL;
                successful_ops++;
            }
        }
    }
    
    double end_time = get_time_ms();
    double total_time = end_time - start_time;
    
    benchmark_result_t result = {
        .test_name = config->name,
        .allocator_name = ALLOCATOR_NAME,
        .total_time_ms = total_time,
        .total_operations = successful_ops,
        .kops_per_sec = (successful_ops / (total_time / 1000.0)) / 1000.0,
        .peak_memory_usage = peak_memory_usage,
        .total_allocated = total_allocated,
        .total_freed = total_freed
    };
    
    FREE(ptrs);
    FREE(sizes);
    return result;
}

static void write_csv_header(FILE* fp) {
    fprintf(fp, "Test,Allocator,Time_ms,Total_Operations,KOps_per_sec,Peak_Memory_MB,Total_Allocated_MB,Total_Freed_MB\n");
}

static void write_csv_result(FILE* fp, const benchmark_result_t* result) {
    fprintf(fp, "%s,%s,%.2f,%zu,%.2f,%.2f,%.2f,%.2f\n",
            result->test_name,
            result->allocator_name,
            result->total_time_ms,
            result->total_operations,
            result->kops_per_sec,
            result->peak_memory_usage / (1024.0 * 1024.0),
            result->total_allocated / (1024.0 * 1024.0),
            result->total_freed / (1024.0 * 1024.0));
}

int main(int argc, char* argv[]) {
    // Initialize random seed
    srand((unsigned int)time(NULL));
    
    INITIALIZE_ALLOCATOR();
    
    // Define benchmark configurations
    benchmark_config_t configs[] = {
        {"Small_Sequential", 16, 1024, 1000000, 0.0},
        {"Medium_Sequential", 1024, 64*1024, 100000, 0.0},
        {"Large_Sequential", 64*1024, 1024*1024, 10000, 0.0},
        {"Mixed_Sequential", 16, 1024*1024, 50000, 0.0},
        {"Small_Random", 16, 1024, 500000, 0.3},
        {"Medium_Random", 1024, 64*1024, 50000, 0.3},
        {"Large_Random", 64*1024, 1024*1024, 5000, 0.3},
        {"Mixed_Random", 16, 1024*1024, 25000, 0.3},
        {"Small_Fragmentation", 16, 1024, 200000, 0.8},
        {"Medium_Fragmentation", 1024, 64*1024, 20000, 0.8},
        {"Large_Fragmentation", 64*1024, 512*1024, 2000, 0.8}
    };
    
    size_t num_configs = sizeof(configs) / sizeof(configs[0]);
    
    // Create output filename
    char filename[256];
    snprintf(filename, sizeof(filename), "benchmark_results_%s.csv", ALLOCATOR_NAME);
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to create output file: %s\n", filename);
        return 1;
    }
    
    write_csv_header(fp);
    
    printf("Starting benchmark suite for %s allocator\n", ALLOCATOR_NAME);
    printf("Memory limit: %.2f GB\n", MAX_MEMORY_USAGE / (1024.0 * 1024.0 * 1024.0));
    printf("Output file: %s\n\n", filename);
    
    // Run benchmarks
    for (size_t i = 0; i < num_configs; i++) {
        benchmark_result_t result;
        
        if (strstr(configs[i].name, "Sequential")) {
            result = test_sequential(&configs[i]);
        } else if (strstr(configs[i].name, "Random")) {
            result = test_random_pattern(&configs[i]);
        } else if (strstr(configs[i].name, "Fragmentation")) {
            result = test_fragmentation(&configs[i]);
        } else {
            result = test_sequential(&configs[i]); // Default
        }
        
        write_csv_result(fp, &result);
        
        printf("  %s: %.2f ms, %.2f KOps/sec, Peak: %.1f MB\n",
               result.test_name, result.total_time_ms, result.kops_per_sec,
               result.peak_memory_usage / (1024.0 * 1024.0));
    }
    
    fclose(fp);
    
    printf("\nBenchmark completed. Results written to %s\n", filename);
    return 0;
}