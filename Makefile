.PHONY: all test clean build

run: test check

test_wasm:
	mkdir -p bin
	CC=emcc OUT=./bin/run_tests.html \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -v -Os -funroll-loops -fopenmp' \
	./test.sh 

test:
	mkdir -p bin
	OUT=./bin/run_tests ./test.sh
	./bin/run_tests

check:
	./check.sh

clean:
	rm -rf bin