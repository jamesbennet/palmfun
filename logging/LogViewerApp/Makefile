# prc-tools Makefile for LogViewerApp

APP   := LogViewerApp
CREA  := LVwr
TYPE  := appl

CC    := m68k-palmos-gcc
CFLAGS:= -O2 -Wall
LDFLAGS:=

OBJS  := LogViewer.o ../common/LogDB.o

all: $(APP).prc

$(APP).prc: $(APP) $(APP).ro
	build-prc -o $(APP).prc -t $(TYPE) -c $(CREA) $(APP) $(APP).ro

$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $(APP) $(OBJS) $(LDFLAGS)

LogViewer.o: LogViewer.c LogViewer.h ../common/LogDB.h
	$(CC) $(CFLAGS) -c LogViewer.c

../common/LogDB.o: ../common/LogDB.c ../common/LogDB.h
	$(CC) $(CFLAGS) -c ../common/LogDB.c -o ../common/LogDB.o

$(APP).ro: LogViewer.rcp LogViewer.h
	pilrc -ro -name "$(APP)" -type $(TYPE) -creator $(CREA) -o $(APP).ro LogViewer.rcp

clean:
	rm -f $(APP) *.o ../common/LogDB.o $(APP).ro $(APP).prc
