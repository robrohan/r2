.PHONY: all test clean build

run: test check

test_wasm:
	mkdir -p bin
	CC=emcc OUT=./bin/run_tests.html \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -v -Os -funroll-loops -fopenmp' \
	./test.sh

test:
	mkdir -p bin
	CC=gcc OUT=./bin/run_tests ./test.sh
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