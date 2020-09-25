###
# -funroll-loops : Unroll loops whose number of iterations can be determined at compile time or upon entry to the loop.
# -m32 for 32 bit
# objdump -S --disassemble run_tests > dmp.asm
# -opt-report
###

CC=${CC:-gcc}
#############################
# -O0 for debug
# -Os (or -O3) for build (emcc)
# -g preserves debug information; -g4 generates source maps (emcc)
#############################

## Desktop version
CFLAGS=${CFLAGS:- -std=c99 -Wall -Werror -Wno-unused -g3 -v -O3 -funroll-loops -msse3 -fopenmp }
# CFLAGS=${CFLAGS:- -std=c99 -Wall -Werror -Wno-unused -g3 -v -O3 -fopenmp }

## Webassembly
#CFLAGS=${CFLAGS:- -std=c99 -Wall -Werror -Wno-unused -v -Os -funroll-loops -fopenmp }

OUT=${OUT:-run_tests}
LIBS=${LIBS:- -lm}

TESTS=./tests/*.c

#############################
${CC} ${CFLAGS} tests.c ${TESTS} ${LIBS} -o ${OUT}
#############################

if [ $? -eq 0 ]
then
    ./${OUT}
    # rm ./${OUT}
fi
