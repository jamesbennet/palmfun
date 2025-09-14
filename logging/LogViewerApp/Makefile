# prc-tools Makefile for LogViewerApp
# Assumes pilrc and build-prc are in PATH; adjust SDK include path if needed.

APPNAME      := LogViewerApp
CREATOR      := LVwr
TYPE         := appl
VERSION      := 1.0

# Where your Palm OS SDK headers live:
PALM_SDK     ?= /opt/palmdev/sdk-4/include

CC           := m68k-palmos-gcc
CFLAGS       := -O2 -Wall -I$(PALM_SDK) -m68000 -palmos4
LDFLAGS      := -m68000 -palmos4

PILRC        := pilrc
BUILD_PRC    := build-prc

OBJS  := LogViewer.o ../common/LogDB.o

all: $(APPNAME).prc

$(APPNAME).prc: $(APPNAME) $(APPNAME).ro
	$(BUILD_PRC) -o $(APPNAME).prc -t $(TYPE) -c $(CREATOR) -n "$(APPNAME)" $(APPNAME) $(APPNAME).ro

$(APPNAME): $(OBJS)
	$(CC) $(CFLAGS) -o $(APPNAME) $(OBJS) $(LDFLAGS)

LogViewer.o: LogViewer.c LogViewer.h ../common/LogDB.h
	$(CC) $(CFLAGS) -c LogViewer.c

../common/LogDB.o: ../common/LogDB.c ../common/LogDB.h
	$(CC) $(CFLAGS) -c ../common/LogDB.c -o ../common/LogDB.o

$(APPNAME).ro: LogViewer.rcp LogViewer.h
	$(PILRC) -ro -name "$(APPNAME)" -type $(TYPE) -creator $(CREATOR) -o $(APPNAME).ro LogViewer.rcp

clean:
	rm -f $(APPNAME) *.o ../common/LogDB.o $(APPNAME).ro $(APPNAME).prc

.PHONY: all clean
