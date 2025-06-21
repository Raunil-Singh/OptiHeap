# Makefile for OptiHeap

CC = gcc
CFLAGS = -O2 -std=c99 -Wall -Wextra -fPIC

# OPTIHEAP_FLAGS determine the features to enable in OptiHeap
# Possible values:
# - OPTIHEAP_DEBUGGER: Enable debugging features
# - OPTIHEAP_THREAD_SAFE: Enable thread safety
# - OPTIHEAP_REFERENCE_COUNTING: Enable reference counting
OPTIHEAP_FLAGS = 

INCLUDES = -I./src -I./include

SRC_DIR = ./src
OBJ_DIR = ./obj
LIB_DIR = ./lib

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

STATIC_LIB = $(LIB_DIR)/liboptiheap.a
SHARED_LIB = $(LIB_DIR)/liboptiheap.so

.PHONY: all clean dirs

libraries: dirs $(STATIC_LIB) $(SHARED_LIB)

dirs:
	@mkdir -p $(OBJ_DIR) $(LIB_DIR)

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(CFLAGS) $(OPTIHEAP_FLAGS) $(INCLUDES) -c $< -o $@

# Build static library
$(STATIC_LIB): $(OBJS)
	@ar rcs $@ $^
	@echo "Static library created: $@"

# Build shared library
$(SHARED_LIB): $(OBJS)
	@$(CC) -shared -o $@ $^
	@echo "Shared library created: $@"

clean:
	@rm -rf $(OBJ_DIR) $(LIB_DIR)
	@echo "Cleaned up object and library directories."