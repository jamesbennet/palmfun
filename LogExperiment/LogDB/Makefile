APPNAME      := LogDB
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -Os -fno-builtin -Wall -I$(PALM_SDK) -Isrc -m68000 -palmos4
AR           := m68k-palmos-ar
ARFLAGS      := rcs

SRC          := $(wildcard src/*.c)
OBJDIR       := build/obj
LIBDIR       := build
OBJS         := $(patsubst src/%.c,$(OBJDIR)/%.o,$(SRC))
LIB          := $(LIBDIR)/libLogDB.a

all: $(LIB)

# Compile C → object into build/obj
$(OBJDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Archive → static library in build/
$(LIB): $(OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -rf build

.PHONY: all clean
