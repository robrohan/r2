# The real build is GNUmakefile (GNU make syntax). GNU make reads that file
# first and never sees this one; BSD make (FreeBSD's make) reads this.
# demo and app build here with plain POSIX make; everything else is handed
# to gmake (pkg install gmake).
GMAKE = gmake

# no built-in suffix rules: "make test" must not turn test.sh into ./test
.SUFFIXES:
CC = cc

C_ERRS = -Wall -Wextra -Wpedantic \
	-Wformat=2 -Wno-unused-parameter -Wshadow \
	-Wwrite-strings -Wstrict-prototypes -Wold-style-definition \
	-Wredundant-decls -Wnested-externs -Wmissing-include-dirs \
	-Wno-unused

help:
	@echo "BSD make builds these directly:"
	@echo "  demo  - build the terminal UI demo (bin/tui_demo)"
	@echo "  app   - build the plain windowing app, no editor (bin/tui_app)"
	@echo "  clean - remove build artifacts"
	@echo "Any other target (test, test_clang, ...) needs gmake: pkg install gmake"

demo:
	mkdir -p bin
	$(CC) -std=c11 $(C_ERRS) -O2 -I. tui/examples/tui_demo.c -o bin/tui_demo

app:
	mkdir -p bin
	$(CC) -std=c11 $(C_ERRS) -O2 -I. tui/examples/tui_app.c -o bin/tui_app

clean:
	rm -rf bin

.DEFAULT:
	@$(GMAKE) CC='$(CC)' $@
