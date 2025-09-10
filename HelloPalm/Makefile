APPNAME      := HelloPalm
CREATOR      := HlWd
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -Os -fno-builtin -Wall -I$(PALM_SDK) -Ires -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

SRC          := src/HelloPalm.c
OBJ          := $(SRC:.c=.o)

RCP          := res/HelloPalm.rcp
RCP_DIR      := $(dir $(RCP))
RSC_DIR      := build/rsrc
BIN          := build/$(APPNAME)
PRC          := build/$(APPNAME).prc

all: $(PRC)

# Compile C → object
src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Link objects into PalmOS executable
$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ -lPalmOSGlue

# Compile resources via PilRC → .bin files in RSC_DIR
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
	rm -rf build src/*.o

.PHONY: all clean