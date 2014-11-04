
SRC_DIR=./src
OBJ_DIR=./obj
LIBPCMALLOC_DIR=/usr/local/pcmalloc/

pcmalloc:
	make -C ${SRC_DIR}

clean:
	rm ${OBJ_DIR}/* -rf


install:
	if [ ! -d ${LIBPCMALLOC_DIR} ]; then mkdir ${LIBPCMALLOC_DIR}; fi;\
	cp obj/libpcmalloc.so ${LIBPCMALLOC_DIR}libpcmalloc.so

.PHONY: pcmalloc clean install

