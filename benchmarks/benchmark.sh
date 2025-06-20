#!/bin/bash

# Memory Allocator Benchmark Script
# This script compiles and runs benchmarks for both glibc malloc and OptiHeap allocator

set -e  # Exit on any error

# Configuration
BENCHMARK_SOURCE="allocator_benchmark.c"
GLIBC_EXECUTABLE="benchmark_glibc"
OPTIHEAP_EXECUTABLE="benchmark_optiheap"
COMBINED_OUTPUT="combined_benchmark_results.csv"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}  Memory Allocator Benchmark Suite   ${NC}"
echo -e "${BLUE}======================================${NC}"
echo

# Check if required files exist
echo -e "${YELLOW}Checking required files...${NC}"
if [ ! -f "$BENCHMARK_SOURCE" ]; then
    echo -e "${RED}Error: $BENCHMARK_SOURCE not found!${NC}"
    exit 1
fi

if [ ! -f "../src/optiheap_allocator.c" ]; then
    echo -e "${RED}Error: optiheap_allocator.c not found!${NC}"
    exit 1
fi

if [ ! -f "../src/heap_allocator.c" ]; then
    echo -e "${RED}Error: heap_allocator.c not found!${NC}"
    exit 1
fi

if [ ! -f "../src/mmap_allocator.c" ]; then
    echo -e "${RED}Error: mmap_allocator.c not found!${NC}"
    exit 1
fi

if [ ! -f "../include/optiheap_allocator.h" ]; then
    echo -e "${RED}Error: optiheap_allocator.h not found!${NC}"
    exit 1
fi

echo -e "${GREEN}All required files found.${NC}"
echo

# Clean previous builds
echo -e "${YELLOW}Cleaning previous builds...${NC}"
rm -f "$GLIBC_EXECUTABLE" "$OPTIHEAP_EXECUTABLE"
rm -f benchmark_results_*.csv
echo -e "${GREEN}Cleanup completed.${NC}"
echo

# Compile glibc version
echo -e "${YELLOW}Compiling glibc malloc benchmark...${NC}"
gcc "$BENCHMARK_SOURCE" -o "$GLIBC_EXECUTABLE" -O2 -std=c99 -Wall -Wextra
if [ $? -eq 0 ]; then
    echo -e "${GREEN}glibc benchmark compiled successfully.${NC}"
else
    echo -e "${RED}Failed to compile glibc benchmark!${NC}"
    exit 1
fi
echo

# Compile OptiHeap version
echo -e "${YELLOW}Compiling OptiHeap benchmark...${NC}"
gcc "$BENCHMARK_SOURCE" ../src/optiheap_allocator.c ../src/heap_allocator.c ../src/mmap_allocator.c \
    -o "$OPTIHEAP_EXECUTABLE" -O2 -std=c99 -Wall -Wextra -DUSE_OPTIHEAP
if [ $? -eq 0 ]; then
    echo -e "${GREEN}OptiHeap benchmark compiled successfully.${NC}"
else
    echo -e "${RED}Failed to compile OptiHeap benchmark!${NC}"
    exit 1
fi
echo

# Function to run benchmark with memory monitoring
run_benchmark() {
    local executable=$1
    local allocator_name=$2
    
    echo -e "${YELLOW}Running $allocator_name benchmark...${NC}"
    echo -e "${BLUE}This may take several minutes depending on system performance.${NC}"
    
    # Get initial memory usage
    local initial_memory=$(free -m | awk 'NR==2{printf "%.0f", $3}')
    
    # Run the benchmark
    ./"$executable"
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}$allocator_name benchmark completed successfully.${NC}"
        
        # Get final memory usage
        local final_memory=$(free -m | awk 'NR==2{printf "%.0f", $3}')
        local memory_diff=$((final_memory - initial_memory))
        
        if [ $memory_diff -gt 100 ]; then
            echo -e "${YELLOW}Warning: Memory usage increased by ${memory_diff}MB during benchmark${NC}"
        fi
    else
        echo -e "${RED}$allocator_name benchmark failed!${NC}"
        return 1
    fi
    echo
}

# Check system resources
echo -e "${YELLOW}Checking system resources...${NC}"
total_memory=$(free -m | awk 'NR==2{printf "%.0f", $2}')
available_memory=$(free -m | awk 'NR==2{printf "%.0f", $7}')
disk_space=$(df -m . | awk 'NR==2 {print $4}')

echo "Total Memory: ${total_memory}MB"
echo "Available Memory: ${available_memory}MB"
echo "Available Disk Space: ${disk_space}MB"

if [ $available_memory -lt 14000 ]; then
    echo -e "${RED}Warning: Available memory (${available_memory}MB) may be insufficient for 12GB benchmark!${NC}"
    echo -e "${YELLOW}Continue anyway? (y/N): ${NC}"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        echo "Benchmark cancelled."
        exit 1
    fi
fi

if [ $disk_space -lt 14000 ]; then
    echo -e "${RED}Warning: Available disk space (${disk_space}MB) may be insufficient!${NC}"
    echo -e "${YELLOW}Continue anyway? (y/N): ${NC}"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        echo "Benchmark cancelled."
        exit 1
    fi
fi
echo

# Run benchmarks
echo -e "${BLUE}Starting benchmark execution...${NC}"
echo

# Run glibc benchmark
run_benchmark "$GLIBC_EXECUTABLE" "glibc malloc"

# Run OptiHeap benchmark
run_benchmark "$OPTIHEAP_EXECUTABLE" "OptiHeap"

# Combine results
echo -e "${YELLOW}Combining results...${NC}"
if [ -f "benchmark_results_glibc.csv" ] && [ -f "benchmark_results_OptiHeap.csv" ]; then
    # Create combined CSV with header from first file
    head -n 1 "benchmark_results_glibc.csv" > "$COMBINED_OUTPUT"
    
    # Add data from both files (skip headers)
    tail -n +2 "benchmark_results_glibc.csv" >> "$COMBINED_OUTPUT"
    tail -n +2 "benchmark_results_OptiHeap.csv" >> "$COMBINED_OUTPUT"
    
    echo -e "${GREEN}Combined results saved to $COMBINED_OUTPUT${NC}"
    
    # Display summary statistics
    echo
    echo -e "${BLUE}=== BENCHMARK SUMMARY ===${NC}"
    echo
    
    # Parse CSV and show summary
    python3 -c "
import csv
import sys

try:
    with open('$COMBINED_OUTPUT', 'r') as f:
        reader = csv.DictReader(f)
        glibc_results = []
        optiheap_results = []
        
        for row in reader:
            if row['Allocator'] == 'glibc':
                glibc_results.append(row)
            else:
                optiheap_results.append(row)
    
    print('Performance Summary:')
    print('===================')
    
    if glibc_results and optiheap_results:
        glibc_avg_kops = sum(float(r['KOps_per_sec']) for r in glibc_results) / len(glibc_results)
        opti_avg_kops = sum(float(r['KOps_per_sec']) for r in optiheap_results) / len(optiheap_results)
        
        glibc_avg_memory = sum(float(r['Peak_Memory_MB']) for r in glibc_results) / len(glibc_results)
        opti_avg_memory = sum(float(r['Peak_Memory_MB']) for r in optiheap_results) / len(optiheap_results)
        
        print(f'Average Performance (KOps/sec):')
        print(f'  glibc malloc: {glibc_avg_kops:.2f}')
        print(f'  OptiHeap:     {opti_avg_kops:.2f}')
        
        if opti_avg_kops > glibc_avg_kops:
            improvement = ((opti_avg_kops - glibc_avg_kops) / glibc_avg_kops) * 100
            print(f'  OptiHeap is {improvement:.1f}% faster on average')
        else:
            decline = ((glibc_avg_kops - opti_avg_kops) / glibc_avg_kops) * 100
            print(f'  OptiHeap is {decline:.1f}% slower on average')
        
        print(f'\\nAverage Peak Memory Usage (MB):')
        print(f'  glibc malloc: {glibc_avg_memory:.2f}')
        print(f'  OptiHeap:     {opti_avg_memory:.2f}')
        
        if opti_avg_memory < glibc_avg_memory:
            savings = ((glibc_avg_memory - opti_avg_memory) / glibc_avg_memory) * 100
            print(f'  OptiHeap uses {savings:.1f}% less memory on average')
        else:
            increase = ((opti_avg_memory - glibc_avg_memory) / glibc_avg_memory) * 100
            print(f'  OptiHeap uses {increase:.1f}% more memory on average')
    
    print(f'\\nTotal tests completed: {len(glibc_results) + len(optiheap_results)}')
    print(f'Results saved to: $COMBINED_OUTPUT')
    
except Exception as e:
    print(f'Could not generate summary: {e}')
    print('Raw results are available in the CSV files.')
" 2>/dev/null || echo -e "${YELLOW}Summary generation failed, but raw results are available in CSV files.${NC}"
    
else
    echo -e "${RED}Error: Could not find both result files for combining!${NC}"
    if [ -f "benchmark_results_glibc.csv" ]; then
        echo -e "${YELLOW}glibc results available in benchmark_results_glibc.csv${NC}"
    fi
    if [ -f "benchmark_results_OptiHeap.csv" ]; then
        echo -e "${YELLOW}OptiHeap results available in benchmark_results_OptiHeap.csv${NC}"
    fi
fi

echo
echo -e "${BLUE}=== FILES GENERATED ===${NC}"
ls -lh benchmark_results_*.csv combined_benchmark_results.csv 2>/dev/null || echo -e "${YELLOW}Some result files may not have been generated.${NC}"

echo
echo -e "${GREEN}Benchmark suite completed!${NC}"
echo -e "${BLUE}You can now use the CSV files to generate graphs for your GitHub repo.${NC}"

# Cleanup executables
echo
echo -e "${YELLOW}Cleaning up executables...${NC}"
rm -f "$GLIBC_EXECUTABLE" "$OPTIHEAP_EXECUTABLE"
echo -e "${GREEN}Cleanup completed.${NC}"