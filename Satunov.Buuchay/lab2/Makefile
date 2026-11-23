.PHONY: all test clean

all:
	./build.sh

test:
	cmake -S . -B build
	cmake --build build --parallel
	ctest --test-dir build --output-on-failure

clean:
	rm -rf build