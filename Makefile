.PHONY: all test clean build

CC := gcc
C_ERRS += -Wall -Wextra -Wpedantic \
		-Wformat=2 -Wno-unused-parameter -Wshadow \
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		-Wno-unused -fms-extensions

DEBUG := -ggdb
CFLAGS := -std=c99 -v 

run: test test_clang check

mk_bin:
	mkdir -p bin

# Should run something like
# `source ~/Projects/spikes/emsdk/emsdk_env.sh` 
# first to setup environment
test_wasm: clean mk_bin
	CC=emcc OUT=./bin/run_tests.html \
	CFLAGS='-std=c99 -Wall -Werror -Wno-unused -v -Os' \
	./test.sh

test: clean mk_bin
	CC=gcc OUT=./bin/run_tests \
	CFLAGS='-std=c99 $(C_ERRS) -g3 -v -O3 -funroll-loops -msse3 -fopenmp' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm
	./bin/run_tests

test_clang: clean mk_bin
#	sudo apt-get install libomp-dev
	CC=clang OUT=./bin/run_tests \
	CFLAGS='-std=c99 $(C_ERRS) -g3 -v -O3 -funroll-loops -msse3 -fopenmp' \
	LIBS='-lm -lomp' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm
	./bin/run_tests

#####################################
# UI Tests
build_x11: clean mk_bin
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200112L $(C_ERRS) $(DEBUG) \
		tests_ui.c \
		-o ./bin/run_window_test \
		-lXrandr \
		-lX11 \
		-lm \
		-ldl \
		-lpthread \
		-mshstk

build_mac:
	$(CC) $(CFLAGS) $(C_ERRS) $(DEBUG) \
		tests_ui.c \
		-o ./bin/run_window_test \
		-lm \
		-framework Foundation \
		-framework AppKit \
		-framework CoreVideo \
		-mshstk \

build_window:
	$(CC) $(CFLAGS) $(C_ERRS) $(DEBUG) \
		tests_ui.c \
		-o ./bin/run_window_test\
		-lshell32 \
		-lwinmm \
		-lgdi32 \
		-lopengl32 \
		-mwindows \
		-mshstk \

run_test_windows:
	./bin/run_window_test
#####################################

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