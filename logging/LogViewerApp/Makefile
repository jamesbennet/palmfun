# prc-tools Makefile for LogViewerApp
# Assumes pilrc and build-prc are in PATH; adjust SDK include path if needed.

APPNAME      := LogViewerApp
CREATOR      := LVwr
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -Os -fno-builtin -Wall -I$(PALM_SDK) -I../common/src -Ires -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

BUILD_DIR    := build
# Auto-pick all .c files under src/
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS)) ../common/$(BUILD_DIR)/LogDB.o
TARGET := $(BUILD_DIR)/$(APPNAME)

RCP          := res/LogViewer.rcp
RCP_DIR := $(dir $(RCP))
PRC          := $(TARGET).prc

all: $(PRC)

# Ensure build dir exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Package PRC: code + resources
$(BUILD_DIR)/$(APPNAME).prc: $(TARGET) $(BUILD_DIR)/$(APPNAME).ro | $(BUILD_DIR)
	$(BUILD_PRC) -o $@ -t $(TYPE) -c $(CREATOR) -n "$(APPNAME)" -v "$(VERSION)" $(TARGET) $(BUILD_DIR)/$(APPNAME).ro

# Link objects into PalmOS executable
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) -lPalmOSGlue

# Generic compile rule for any source in src/
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build shared object via common/Makefile
../common/$(BUILD_DIR)/LogDB.o:
	$(MAKE) -C ../common

# Compile resources via PilRC
$(BUILD_DIR)/$(APPNAME).ro: $(RCP) src/LogViewer.h | $(BUILD_DIR)
	$(PILRC) -ro -I "$(abspath $(RCP_DIR))" -I "$(abspath src)" -name "$(APPNAME)" -type $(TYPE) -creator $(CREATOR) -o $@ $(RCP)

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C ../common clean

.PHONY: all clean
