# prc-tools Makefile for LogTestApp
# Assumes pilrc and build-prc are in PATH; adjust SDK include path if needed.

APPNAME      := LogTestApp
CREATOR      := LTst
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -O2 -Wall -I$(PALM_SDK) -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

OBJS  := LogTest.o ../common/LogDB.o

all: $(APPNAME).prc

$(APPNAME).prc: $(APPNAME) $(APPNAME).ro
	$(BUILD_PRC) -o $(APPNAME).prc -t $(TYPE) -c $(CREATOR) -n "$(APPNAME)" $(APPNAME) $(APPNAME).ro

$(APPNAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(APPNAME) $(OBJS) $(LDFLAGS)

LogTest.o: LogTest.c LogTest.h ../common/LogDB.h
	$(CC) $(CFLAGS) -c LogTest.c

../common/LogDB.o: ../common/LogDB.c ../common/LogDB.h
	$(CC) $(CFLAGS) -c ../common/LogDB.c -o ../common/LogDB.o

$(APPNAME).ro: LogTest.rcp LogTest.h
	$(PILRC) -ro -name "$(APPNAME)" -type $(TYPE) -creator $(CREATOR) -o $(APPNAME).ro LogTest.rcp

clean:
	rm -f $(APPNAME) *.o ../common/LogDB.o $(APPNAME).ro $(APPNAME).prc

.PHONY: all clean
