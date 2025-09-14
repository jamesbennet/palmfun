# prc-tools Makefile for HelloPalm
# Assumes pilrc and build-prc are in PATH; adjust SDK include path if needed.

APPNAME      := HelloPalm
CREATOR      := HlWd
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -Os -fno-builtin -Wall -I$(PALM_SDK) -I../logging/common/src -Ires -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

BUILD_DIR    := build
# Auto-pick all .c files under src/
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS)) ../logging/common/$(BUILD_DIR)/LogDB.o
TARGET := $(BUILD_DIR)/$(APPNAME)

RCP          := res/HelloPalm.rcp
RCP_DIR      := $(dir $(RCP))
RSC_DIR      := $(BUILD_DIR)/rsrc
PRC          := $(TARGET).prc

all: $(PRC)

# Ensure build dir exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generic compile rule for any source in src/ -> object in build/
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# Link objects into PalmOS executable
$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ -lPalmOSGlue

# Build shared object via common/Makefile
../logging/common/$(BUILD_DIR)/LogDB.o:
	$(MAKE) -C ../logging/common

# Compile resources via PilRC → .bin files in RSC_DIR
$(RSC_DIR): $(RCP)
	@mkdir -p $(RSC_DIR)
	( cd $(RSC_DIR) && $(PILRC) -q -I "$(abspath $(PALM_SDK))" -I "$(abspath $(RCP_DIR))" "$(abspath $(RCP))" )

# Package PRC: code + resources
$(PRC): $(TARGET) $(RSC_DIR)
	@mkdir -p $(@D)
	$(BUILD_PRC) -o $(PRC) \
	  -n "$(APPNAME)" -c $(CREATOR) -t $(TYPE) -v "$(VERSION)" \
	  $(TARGET) $(RSC_DIR)/*

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C ../logging/common clean

.PHONY: all clean
