#!/bin/bash

if [ $1 -eq 1 ]; then
	export LD_PRELOAD=/usr/local/pcmalloc/libpcmalloc.so
else
	export LD_PRELOAD=
fi
