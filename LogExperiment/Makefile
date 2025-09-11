SUBDIRS := LogDB LogTestApp LogViewerApp

all:
	@for d in $(SUBDIRS); do \
	  $(MAKE) -C $$d || exit $$?; \
	done

clean:
	@for d in $(SUBDIRS); do \
	  $(MAKE) -C $$d clean || true; \
	done

.PHONY: all clean