# Makefile for common code (LogDB)

# Where your Palm OS SDK headers live:
PALM_SDK ?= /opt/palmdev/sdk-4/include

CC     := m68k-palmos-gcc
CFLAGS := -Os -fno-builtin -Wall -I$(PALM_SDK) -m68000 -palmos4

# Sources in src/, objects output to build/ directory
BUILD_DIR := build
# Auto-pick all .c files under src/
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(OBJS)

# Ensure build dir exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile src/foo.c -> build/foo.o
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ -lPalmOSGlue

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
