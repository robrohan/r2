.PHONY: all test clean build

run: test test_clang check

# Should run something like
# `source ~/Projects/spikes/emsdk/emsdk_env.sh` 
# first to setup environment
test_wasm: clean
	mkdir -p bin
	CC=emcc OUT=./bin/run_tests.html \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -v -Os -funroll-loops -fopenmp' \
	./test.sh

test: clean
	mkdir -p bin
	CC=gcc OUT=./bin/run_tests \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -g3 -v -O3 -funroll-loops -msse3 -fopenmp' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm
	./bin/run_tests

test_clang: clean
	mkdir -p bin
	CC=clang OUT=./bin/run_tests \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -g3 -v -O3 -funroll-loops -msse3' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm
	./bin/run_tests

perf:
#####################################
# build as user, but run this sudo
#####################################
# The output also estimates the instructions per processor 
# clock cycle (IPC). The higher IPC the more efficiently the 
# processor is executing instruction on the system.
#
# The ratio of cache-misses to instructions will give an indication 
# how well the cache is working; the lower the ratio the better.
# ratio = (cache-misses / instructions) ---- 0.000247968
	perf stat -e task-clock,cycles,instructions,cache-references,cache-misses ./bin/run_tests
#	perf record -e cache-misses ./bin/run_tests
# perf report
# perf annotate
#	perf stat ./bin/run_tests

check:
	./check.sh

clean:
	rm -rf bin