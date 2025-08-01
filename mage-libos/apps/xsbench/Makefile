ALGOS = XSBench
BINS = $(addprefix gapbs/,$(ALGOS))
genlib = ldd XSBench/openmp-threading/XSBench | grep "lib$1.so" | sed 's/ *[^ ] *\(.*\) => \(.*\) .*/\/\1: \2/'

.PHONY: module
module: $(BINS) usr.manifest


.PHONY: build-all
build-all:
	cd XSBench/openmp-threading; make -j

.PHONY: $(BINS)
$(BINS): build-all

.PHONY: clean
clean:
	cd XSBench/openmp-threading; make clean


usr.manifest: $(BINS) Makefile
	@rm -f usr.manifest
	@for algo in $(ALGOS) ; do \
        echo "/$$algo: \$${MODULE_DIR}/XSBench/openmp-threading/$$algo" >> usr.manifest; \
    done
#	omp is used for clang compilers
#	$(call genlib,omp.*) >> usr.manifest
#	gomp is used for gcc compilers
	$(call genlib,gomp.*) >> usr.manifest
