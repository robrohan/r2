.PHONY: all test test_strings test_clang clean build help

help:
	@echo "Available targets:"
	@echo "  run          - build with gcc and clang, then lint (default)"
	@echo "  test         - build and run all tests with gcc"
	@echo "  test_strings - build and run only the strings test suite with gcc"
	@echo "  test_clang   - build and run all tests with clang"
	@echo "  test_wasm    - build and run all tests with emcc (needs emsdk env)"
	@echo "  check        - run static analysis / lint (check.sh)"
	@echo "  perf         - run perf stat on the test binary (Linux only, run as sudo)"
	@echo "  clean        - remove build artifacts"

ARCH    := $(shell uname -m)
OS      := $(shell uname -s)

C_ERRS += -Wall -Wextra -Wpedantic \
		-Wformat=2 -Wno-unused-parameter -Wshadow \
		-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
		-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
		-Wno-unused

# x86-only flags
ifeq ($(ARCH),x86_64)
	SIMD_FLAGS  := -msse3
	OMP_FLAGS   := -fopenmp
	OMP_LIBS    :=
endif

# On macOS ARM64 (Apple Silicon), skip SSE3 and OpenMP
ifeq ($(OS)_$(ARCH),Darwin_arm64)
	SIMD_FLAGS  :=
	OMP_FLAGS   :=
	OMP_LIBS    :=
endif

ifeq ($(OS), Darwin)
	BLAS_CFLAGS  = -DHAVE_BLAS -DACCELERATE_NEW_LAPACK -framework Accelerate
	BLAS_LDFLAGS = -framework Accelerate
else
	BLAS_LIBS := $(shell pkg-config --libs openblas 2>/dev/null)
	ifneq ($(BLAS_LIBS),)
		BLAS_CFLAGS  = -DHAVE_BLAS $(shell pkg-config --cflags openblas)
		BLAS_LDFLAGS = $(shell pkg-config --libs openblas)
	endif
endif


run: test test_clang check

test: clean
	mkdir -p bin
	CC=gcc OUT=./bin/run_tests \
	CFLAGS='-std=c11 $(C_ERRS) -g3 -v -O3 -funroll-loops $(SIMD_FLAGS) $(OMP_FLAGS) $(BLAS_CFLAGS)' \
	LDFLAGS='$(BLAS_LDFLAGS)' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm
	./bin/run_tests


test_strings: clean
	mkdir -p bin
	CC=gcc OUT=./bin/run_tests \
	CFLAGS='-std=c11 $(C_ERRS) -g3 -v -O3 -funroll-loops $(SIMD_FLAGS) $(OMP_FLAGS) $(BLAS_CFLAGS)' \
	LDFLAGS='$(BLAS_LDFLAGS)' \
	./test.sh
	./bin/run_tests strings

test_clang: clean
#	sudo apt-get install libomp-dev
	mkdir -p bin
	CC=clang OUT=./bin/run_tests \
	CFLAGS='-std=c11 $(C_ERRS) -g3 -v -O3 -funroll-loops $(SIMD_FLAGS) $(OMP_FLAGS) $(BLAS_CFLAGS)' \
	LDFLAGS='$(BLAS_LDFLAGS)' \
	LIBS='-lm $(OMP_LIBS)' \
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
