APPNAME      := LogViewerApp
CREATOR      := LvAp
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -Os -fno-builtin -Wall -I$(PALM_SDK) -Isrc -I../LogDB/src -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

LIBLOGDB     := ../LogDB/build/libLogDB.a

SRC          := $(wildcard src/*.c)
OBJDIR       := build/obj
OBJS         := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))

RCP          := res/LogViewerApp.rcp
RCP_DIR      := $(dir $(RCP))
RSC_DIR      := build/rsrc
BIN          := build/$(APPNAME)
PRC          := build/$(APPNAME).prc

all: $(PRC)

# Compile C → object into build/obj
$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure LogDB lib is built
$(LIBLOGDB):
	$(MAKE) -C ../LogDB

# Link objects + lib → PalmOS executable in build/
$(BIN): $(OBJS) $(LIBLOGDB)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBLOGDB) -lPalmOSGlue

# PilRC → resource files in build/rsrc
$(RSC_DIR): $(RCP)
	@mkdir -p $(RSC_DIR)
	( cd $(RSC_DIR) && $(PILRC) -q -I "$(abspath $(PALM_SDK))" -I "$(abspath $(RCP_DIR))" "$(abspath $(RCP))" )

# Package PRC: code + resources
$(PRC): $(BIN) $(RSC_DIR)
	@mkdir -p $(dir $@)
	$(BUILD_PRC) -o $(PRC) \
	  -n "$(APPNAME)" -c $(CREATOR) -t $(TYPE) -v "$(VERSION)" \
	  $(BIN) $(RSC_DIR)/*

clean:
	rm -rf build

.PHONY: all clean
