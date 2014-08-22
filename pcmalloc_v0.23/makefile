
SRC_DIR = ./src
OBJ_DIR = ./obj

pcmalloc:
	make -C ${SRC_DIR}

clean:
	rm ${OBJ_DIR}/* -rf


install:
	mkdir /usr/local/pcmalloc
	cp obj/libpcmalloc.so /usr/local/pcmalloc/libpcmalloc.so

.PHONY: pcmalloc clean install

