# prc-tools Makefile for LogTestApp
# Assumes pilrc and build-prc are in PATH; adjust SDK include path if needed.

APP   := LogTestApp
CREA  := LTst
TYPE  := appl

CC    := m68k-palmos-gcc
CFLAGS:= -O2 -Wall
LDFLAGS:=

OBJS  := LogTest.o ../common/LogDB.o

all: $(APP).prc

$(APP).prc: $(APP) $(APP).ro
	build-prc -o $(APP).prc -t $(TYPE) -c $(CREA) -n "$(APP)" $(APP) $(APP).ro

$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $(APP) $(OBJS) $(LDFLAGS)

LogTest.o: LogTest.c LogTest.h ../common/LogDB.h
	$(CC) $(CFLAGS) -c LogTest.c

../common/LogDB.o: ../common/LogDB.c ../common/LogDB.h
	$(CC) $(CFLAGS) -c ../common/LogDB.c -o ../common/LogDB.o

$(APP).ro: LogTest.rcp LogTest.h
	pilrc -ro -name "$(APP)" -type $(TYPE) -creator $(CREA) -o $(APP).ro LogTest.rcp

clean:
	rm -f $(APP) *.o ../common/LogDB.o $(APP).ro $(APP).prc
