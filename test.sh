###
# -funroll-loops : Unroll loops whose number of iterations can be determined at compile time or upon entry to the loop.
# -m32 for 32 bit
###

CC=${CC:-gcc}
# -O0 for debug
# -Os (or -O3) for build (emcc)
# -g preserves debug information; -g4 generates source maps (emcc)
CFLAGS=${CFLAGS:- -std=c99 -v -Wall -Werror -Wno-unused -g3 -v -O3 -funroll-loops }
# CFLAGS=${CFLAGS:- -std=c99 -Wall -Werror -Wno-unused -v -Os -funroll-loops }
OUT=${OUT:-run_tests}
LIBS=${LIBS:- -lm}

TESTS=./tests/*.c

${CC} ${CFLAGS} tests.c ${TESTS} ${LIBS} -o ${OUT}

if [ $? -eq 0 ]
then
    ./${OUT}
    # rm ./${OUT}
fi
