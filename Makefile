# Top-level Makefile for all projects

PROJECTS := HelloPalm logging

.PHONY: all clean $(PROJECTS)

all: $(PROJECTS)

$(PROJECTS):
	$(MAKE) -C $@

clean:
	@for d in $(PROJECTS); do \
		$(MAKE) -C $$d clean; \
	done
