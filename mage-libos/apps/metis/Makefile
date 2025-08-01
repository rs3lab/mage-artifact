BENCH = wc wr wrmem
BINS = $(addprefix metis/obj,$(BENCH))

.PHONY: module
module: $(BINS) usr.manifest


.PHONY: build-all
build-all:
	cd metis; make

.PHONY: $(BINS)
$(BINS): build-all

.PHONY: clean
clean:
	cd metis; make clean


usr.manifest: $(BINS) Makefile
	@rm -f usr.manifest
	@for b in $(BENCH) ; do \
        echo "/$$b: \$${MODULE_DIR}/metis/obj/app/$$b" >> usr.manifest; \
    done
