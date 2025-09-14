# Top-level Makefile for logging project
# Builds common/, then LogTestApp and LogViewerApp via their own Makefiles.

APPS := common LogTestApp LogViewerApp

.PHONY: all clean $(APPS)

all: $(APPS)

$(APPS):
	$(MAKE) -C $@

clean:
	@for d in $(APPS); do \
		$(MAKE) -C $$d clean; \
	done
