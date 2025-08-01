.PHONY: module

include ../build_type.mk

module:
	@mkdir -p snappy/build
	@cd snappy/build; cmake -DCMAKE_BUILD_TYPE=Release ..; make; cd ../..
	@mkdir -p build
	@cd build; cmake -G Ninja -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} ..; ninja

clean:
	rm -rf build
	rm -r  snappy/build
