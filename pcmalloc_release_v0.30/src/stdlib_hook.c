#include <unistd.h>
#include <execinfo.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>


#include "config.h"
#include "stdlib_hook.h"
#include "chunk_monitor.h"
#include "pc_malloc.h"


typedef ssize_t (*func_read)(int fd, void *buf, size_t count);
typedef ssize_t (*func_write)(int fd, const void *buf, size_t count);
typedef void* (*func_fopen)(const char *path, const char *mode);
typedef int (*func_fclose)(FILE *fp);
typedef size_t (*func_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*func_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);

static func_read libc_read;
static func_write libc_write;
static func_fopen libc_fopen;
static func_fclose libc_fclose;
static func_fread libc_fread;
static func_fwrite libc_fwrite;


int
install_stdlibapi_hook()
{
	void *ld_handle;

	DEACT_CACHE_CONTROL();
	ld_handle = dlopen("libc.so.6", RTLD_LAZY);
	libc_read = (func_read)dlsym(ld_handle, "read");
	libc_write = (func_write)dlsym(ld_handle, "write");
	libc_fopen = (func_fopen)dlsym(ld_handle, "fopen");
	libc_fclose = (func_fclose)dlsym(ld_handle, "fclose");
	libc_fread = (func_fread)dlsym(ld_handle, "fread");
	libc_fwrite = (func_fwrite)dlsym(ld_handle, "fwrite");
	ACT_CACHE_CONTROL();

	return 0;
}


static inline void
pre_walk(unsigned long start, unsigned long len)
{
	unsigned long end;
	unsigned long p;
	char c;
	
	end = start + len;
	for (p = start; p < end; p += PAGE_SIZE) {
		c = *(char*)p;
	}
	c++;
}

ssize_t
read(int fd, void *buf, size_t count) 
{
	ssize_t ret;

	pre_walk((unsigned long)buf, (unsigned long)count);
	remove_sample_range((unsigned long)buf, (unsigned long)count);

	DEACT_CACHE_CONTROL();
	ret = libc_read(fd, buf, count);
	ACT_CACHE_CONTROL();

	return ret;
}

ssize_t
write(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	pre_walk((unsigned long)buf, (unsigned long)count);
	remove_sample_range((unsigned long)buf, (unsigned long)count);

	DEACT_CACHE_CONTROL();
	ret = libc_write(fd, buf, count);
	ACT_CACHE_CONTROL();

	return ret;
}


FILE *
fopen(const char *path, const char *mode)
{
	FILE *f;

	DEACT_CACHE_CONTROL();
	f = libc_fopen(path, mode);
	ACT_CACHE_CONTROL();

	return f;
}

int
fclose(FILE *fp)
{
	int ret;

	DEACT_CACHE_CONTROL();
	ret = libc_fclose(fp);
	ACT_CACHE_CONTROL();

	return ret;
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret;

	pre_walk((unsigned long)ptr, (unsigned long)(size * nmemb));
	remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);

	DEACT_CACHE_CONTROL();
	ret = libc_fread(ptr, size, nmemb, stream);
	ACT_CACHE_CONTROL();

	return ret;
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret;

	pre_walk((unsigned long)ptr, (unsigned long)(size * nmemb));
	remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);

	DEACT_CACHE_CONTROL();
	ret = libc_fwrite(ptr, size, nmemb, stream);
	ACT_CACHE_CONTROL();

	return ret;
}



