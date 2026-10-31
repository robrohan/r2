.PHONY: all test test_strings test_maths test_quant test_tui test_clang test_wasm demo clean build help

help:
	@echo "Available targets:"
	@echo "  run          - build with gcc and clang, then lint (default)"
	@echo "  test         - build and run all tests with gcc"
	@echo "  test_strings - build and run only the strings test suite with gcc"
	@echo "  test_maths   - build and run only the maths test suite with gcc"
	@echo "  test_quant   - build and run only the quantized-math test suite with gcc"
	@echo "  test_tui     - build and run only the terminal UI test suite with gcc"
	@echo "  test_clang   - build and run all tests with clang"
	@echo "  test_wasm    - build and run all tests with emcc (needs emsdk env)"
	@echo "  demo         - build the terminal UI demo (bin/tui_demo)"
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
	SIMD_FLAGS  := -msse4.1
	OMP_FLAGS   := -fopenmp
	OMP_LIBS    :=
	# SIMD_AVX2=1 opts into the AVX2 quantized-math kernels (r2_quant.h);
	# not the default since not every x86_64 host is guaranteed to have it.
	ifdef SIMD_AVX2
		SIMD_FLAGS += -mavx2 -mfma
	endif
endif

# On macOS ARM64 (Apple Silicon), skip SSE3 and OpenMP. NEON is mandatory
# on aarch64 so r2_quant.h's NEON kernels are picked up automatically here
# with no extra flag needed.
ifeq ($(OS)_$(ARCH),Darwin_arm64)
	SIMD_FLAGS  :=
	OMP_FLAGS   :=
	OMP_LIBS    :=
endif

# SIMD_FORCE_SCALAR=1 forces r2_quant.h's portable-C reference kernels even
# on hosts that would otherwise auto-select a real SIMD backend - used in CI
# so the scalar fallback actually runs, not just compiles. Arch-independent.
ifdef SIMD_FORCE_SCALAR
	SIMD_FLAGS += -DR2_SIMD_FORCE_SCALAR
endif

ifeq ($(OS), Darwin)
#	the CI/CD doesn't seem to have BLAS
	ifndef NO_BLAS
		BLAS_CFLAGS  = -DHAVE_BLAS -DACCELERATE_NEW_LAPACK -framework Accelerate
		BLAS_LDFLAGS = -framework Accelerate
	endif
else
	BLAS_LIBS := $(shell pkg-config --libs openblas 2>/dev/null)
	ifneq ($(BLAS_LIBS),)
		BLAS_CFLAGS  = -DHAVE_BLAS $(shell pkg-config --cflags openblas)
		BLAS_LDFLAGS = $(shell pkg-config --libs openblas)
	endif
endif


run: test test_clang check

build_tests: clean
	mkdir -p bin
	CC=gcc OUT=./bin/run_tests \
	CFLAGS='-std=c11 $(C_ERRS) -g3 -v -O3 -funroll-loops $(SIMD_FLAGS) $(OMP_FLAGS) $(BLAS_CFLAGS)' \
	LDFLAGS='$(BLAS_LDFLAGS)' \
	./test.sh
#	objdump -S --disassemble ./bin/run_tests > ./bin/run_tests.asm

test: build_tests
	./bin/run_tests

test_strings: build_tests
	./bin/run_tests strings

test_maths: build_tests
	./bin/run_tests maths

test_quant: build_tests
	./bin/run_tests quant

test_term: build_tests
	./bin/run_tests termui

test_tui: build_tests
	./bin/run_tests tui

# Nothing here uses OpenMP pragmas, so clang builds without OMP_FLAGS
# (clang's -fopenmp needs libomp, which isn't always installed).
test_clang: clean
	mkdir -p bin
	CC=clang OUT=./bin/run_tests_clang \
	CFLAGS='-std=c11 $(C_ERRS) -g3 -O3 $(SIMD_FLAGS) $(BLAS_CFLAGS)' \
	LDFLAGS='$(BLAS_LDFLAGS)' \
	./test.sh
	./bin/run_tests_clang

# Needs an activated emsdk (emcc) and node.
test_wasm: clean
	mkdir -p bin
	CC=emcc OUT=./bin/run_tests.js \
	CFLAGS='-std=c11 $(C_ERRS) -O2 -DR2_SIMD_FORCE_SCALAR' \
	LDFLAGS='-sALLOW_MEMORY_GROWTH=1' \
	./test.sh
	node ./bin/run_tests.js

demo:
	mkdir -p bin
	$(CC) -std=c11 $(C_ERRS) -O2 examples/tui_demo.c -o bin/tui_demo

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
