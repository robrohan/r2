# ℝ² Some Simple, C99 Vector, Matrix, UTF8 String, and Terminal Libraries

Single-file public domain headers (or MIT licensed) libraries for C (C99)

![C/C++ CI](https://github.com/robrohan/r2/workflows/C/C++%20CI/badge.svg)

The main goal of these libraries is to be fast, and usable for games 
programming - with a sprinkle of machine learning and general maths.

This library is in the style of: https://github.com/nothings/stb

Current Libraries:

- Vector, matrix and quaternion: [r2_maths.h](./r2_maths.h)
- UTF-8 String library: [r2_strings.h](./r2_strings.h)
- Terminal escape codes: [r2_termui.h](./r2_termui.h)
- Terminal UI (cells, diff renderer, keyboard input; no malloc, never blocks): [r2_tui.h](./r2_tui.h)
  with a POSIX terminal adapter [r2_tui_posix.h](./r2_tui_posix.h) and a text editor widget
  [r2_tui_edit.h](./r2_tui_edit.h). Try `make demo && ./bin/tui_demo` for everything, or
  `make app && ./bin/tui_app` for just the windowing (a starter app to copy: `examples/tui_app.c`).
  It also runs in a browser as wasm (clang only, nothing imported): `make test_wasm`, then
  `python3 -m http.server 8000` and open `http://localhost:8000/examples/wasm/`.
- Minimal unit testing: [r2_unit.h](./r2_unit.h)

## Installation

Copy any of the header files you want to use into your code base. See the 
header files for instructions (mostly you just include them).

Example:

```bash
curl https://raw.githubusercontent.com/robrohan/r2/main/r2_termui.h > ./src/r2_termui.h
```

## Using the Vector, Quaternion, and Matrix Functions

1. Clone this library as a git submodule into a target project:

```bash
$ cd my_project
$ mkdir vendor
$ cd vendor
$ git submodule add https://github.com/robrohan/r2.git
```

2. Then the vendor directory to your compiler flags: `-I./vendor`
3. In the _main_ part of your application (only once), import the library with 
the implementation flag:

```c
...
# include <limits.h>
# define R2_MATHS_IMPLEMENTATION
# include "r2/r2_maths.h"
# include <string.h>
...
```

4. Anywhere else you need the functions, use the include without the flag:

```c
# include <limits.h>
# include "r2/r2_maths.h"
# include <string.h>
```

5. Go for gold. See `tests/r2_maths.c` for some example usages, or look at 
`r2_maths.h` for reference.

```c
  vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
  vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
  vec4 out = {0};
  vec3_cross(&v1, &v2, &out);
```

### Testing

```sh
make test
```

_Note_: If you are on windows, currently, you'll have to write something like 
a `test.bat` yourself (or some magic to import the files into Visual Studio). 
You can use `test.sh` as a template.

