ALGOS = gups-hotset-move
BINS = $(addprefix gups/,$(ALGOS))

.PHONY: module
module: $(BINS) usr.manifest


.PHONY: build-all
build-all:
	cd gups; make -j

.PHONY: $(BINS)
$(BINS): build-all

.PHONY: clean
clean:
	cd gups; make clean


usr.manifest: $(BINS) Makefile
	@rm -f usr.manifest
	@for algo in $(ALGOS) ; do \
        echo "/$$algo: \$${MODULE_DIR}/gups/$$algo" >> usr.manifest; \
    done
