OBJ_DIR = ./obj
LIBCACHEEX_DIR=/usr/local/lib
CACHEEX_OBJ=${patsubst %.c,${OBJ_DIR}/%.o,${wildcard *.c}}
CACHEEX_CFLAGS=-Wall -lm -g -fPIC -Wformat -O2
CACHEEX_LD=-lpapi
PAPI_PATH=/usr/local/papi/include
CACHEEX_LIB=${OBJ_DIR}/libcacheex.so
CC=gcc

all: ${CACHEEX_LIB}

${CACHEEX_LIB} : ${CACHEEX_OBJ}
	${CC} -fPIC -shared -Wall ${CACHEEX_CFLAGS} -o $@ $^ -ldl -I${PAPI_PATH}

${OBJ_DIR}/%.o : %.c
	${CC} -c ${CACHEEX_CFLAGS} $< -o $@ -I${PAPI_PATH}


clean:
	rm ${OBJ_DIR}/*

install:
	cp ${CACHEEX_LIB} ${LIBCACHEEX_DIR}

.PHONY: all clean install

