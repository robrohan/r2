# r2

Single-file public domain (or MIT licensed) libraries for C (C99)

![C/C++ CI](https://github.com/robrohan/r2/workflows/C/C++%20CI/badge.svg?branch=master)

The goal of these is to be usable for games programming, and I am trying to build these in the style of: https://github.com/nothings/stb

- Minimal unit testing: r2_unit.h - usable
- Vector, matrix and quaternion: r2_maths.h - alpha

## Testing

Just run test.sh:

```sh
./test.sh
```

N.B. If you are on windows, currently, you'll have to write test.bat yourself or import the files into Visual Studio.

## Testing in Web Assembly

To compile and run the code in web assembly, first make sure you have `emcc` environment variables setup:

```sh
source ~/Projects/spikes/emsdk/emsdk_env.sh
```

Then just run `test.sh` passing in the `emcc` compiler and output to `html`:

```sh
CC=emcc OUT=run_tests.html ./test.sh
```

You can then use run some sort of http server within the main directory ([busboy](https://github.com/robrohan/busboy) for example), and then load the HTML page in your browser.
