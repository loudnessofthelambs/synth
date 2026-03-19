# Compiler
CC = clang

# Compiler flags
CFLAGS = -Wall -Wextra -std=c23 -O3

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = .
TEST_DIR = tests
TEST_OBJ_DIR = $(OBJ_DIR)/tests

# Output executable
TARGET = $(BIN_DIR)/audio
TEST_TARGET = $(BIN_DIR)/audio_tests

# Find all .c files in src directory
SRCS = $(wildcard $(SRC_DIR)/*.c)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)

# Convert .c files to .o files in obj directory
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(LIB_SRCS))
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(TEST_OBJ_DIR)/%.o, $(TEST_SRCS))

# Default target
all: $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Link object files to create the executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(TEST_TARGET): $(LIB_OBJS) $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# Compile .c files to .o files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c | $(TEST_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories if they don't exist
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TEST_OBJ_DIR):
	mkdir -p $(TEST_OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean up build files
clean:
	rm -rf $(OBJ_DIR) $(TEST_TARGET)

# Phony targets
.PHONY: all clean test
