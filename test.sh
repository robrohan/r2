CC=${CC:-gcc}
CFLAGS=${CFLAGS:- -std=c99 -Wall -Werror -Wno-unused \
  -g3 -v -O0 }
OUT=${OUT:-run_tests}

TESTS=./tests/*.c

${CC} ${CFLAGS} tests.c ${TESTS} -o ${OUT}

if [ $? -eq 0 ]
then
    ./${OUT}
    rm ./${OUT}
fi
