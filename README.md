# ℝ² C99 Vector and Matrix Library

Single-file public domain (or MIT licensed) libraries for C (C11)

![C/C++ CI](https://github.com/robrohan/r2/workflows/C/C++%20CI/badge.svg?branch=master)

The main goal of these libraries is to be fast, and usable for games programming - with a sprinkle of machine learning and general maths.

This library is in the style of: https://github.com/nothings/stb

Current Libraries:

- Vector, matrix and quaternion: r2_maths.h - alpha
- Minimal unit testing: r2_unit.h - usable

## Using the Vector, Quaternion, and Matrix Functions

1. Clone this library as a git submodule into a target project:

```bash
$ cd my_project
$ mkdir vendor
$ cd vendor
$ git submodule add https://github.com/robrohan/r2.git
```

2. Then the vendor directory to your compiler flags: `-I./vendor`
3. In the _main_ part of your application (only once), import the library with the implementation flag:

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

5. Go for gold. See `tests/r2_maths.c` for some example usages, or look at `r2_maths.h` for reference.

```c
  vec3 v1 = {.x = 3.f, .y = 3.f, .z = 3.f};
  vec3 v2 = {.x = -30.f, .y = 30.f, .z = -30.f};
  vec4 *out = calloc(sizeof(vec4), 1);
  vec3_cross(&v1, &v2, out);
```

## Getting Started / Running

### Testing

```sh
make test
```

_Note_: If you are on windows, currently, you'll have to write something like a `test.bat` yourself (or some magic to import the files into Visual Studio). You can use `test.sh` as a template.

### Testing in Web Assembly

To compile and run the code in web assembly, first make sure you have [emscripten setup](https://emscripten.org/docs/getting_started/downloads.html), working, and the `emcc` environment variables setup:

```sh
source ~/Projects/spikes/emsdk/emsdk_env.sh
```

Then just run `make test_wasm` or if you wish run `test.sh` passing in the `emcc` compiler and output to `html`:

```sh
CC=emcc OUT=run_tests.html ./test.sh
```

You can then use run some sort of http server within the main directory ([busboy](https://github.com/robrohan/busboy) for example), and then load the HTML page in your browser.
