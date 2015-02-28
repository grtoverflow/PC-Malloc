#include <unistd.h>
#include <execinfo.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>


#include "config.h"
#include "allocator.h"
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

static int hooked = 0;

#define stdlib_hooked() (hooked == 1)
#define set_stdlib_hook_state() (hooked = 1)

int
install_stdlibapi_hook()
{
	void *ld_handle;

	if (stdlib_hooked())
		return 0;
	disable_cache_management();
	ld_handle = dlopen("libc.so.6", RTLD_LAZY);
	libc_read = (func_read)dlsym(ld_handle, "read");
	libc_write = (func_write)dlsym(ld_handle, "write");
	libc_fopen = (func_fopen)dlsym(ld_handle, "fopen");
	libc_fclose = (func_fclose)dlsym(ld_handle, "fclose");
	libc_fread = (func_fread)dlsym(ld_handle, "fread");
	libc_fwrite = (func_fwrite)dlsym(ld_handle, "fwrite");
	enable_cache_management();
	set_stdlib_hook_state();

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

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	if (NightWatch_active()) {
		pre_walk((unsigned long)buf, (unsigned long)count);
		remove_sample_range((unsigned long)buf, (unsigned long)count);
	}

	disable_cache_management();
	ret = libc_read(fd, buf, count);
	enable_cache_management();

	return ret;
}

ssize_t
write(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	if (NightWatch_active()) {
		pre_walk((unsigned long)buf, (unsigned long)count);
		remove_sample_range((unsigned long)buf, (unsigned long)count);
	}

	disable_cache_management();
	ret = libc_write(fd, buf, count);
	enable_cache_management();

	return ret;
}


FILE *
fopen(const char *path, const char *mode)
{
	FILE *f;

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	disable_cache_management();
	f = libc_fopen(path, mode);
	enable_cache_management();

	return f;
}

int
fclose(FILE *fp)
{
	int ret;

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	disable_cache_management();
	ret = libc_fclose(fp);
	enable_cache_management();

	return ret;
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret;

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	if (NightWatch_active()) {
		pre_walk((unsigned long)ptr, (unsigned long)(size * nmemb));
		remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);
	}

	disable_cache_management();
	ret = libc_fread(ptr, size, nmemb, stream);
	enable_cache_management();

	return ret;
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret;

	if (!stdlib_hooked())
		install_stdlibapi_hook();

	if (NightWatch_active()) {
		pre_walk((unsigned long)ptr, (unsigned long)(size * nmemb));
		remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);
	}

	disable_cache_management();
	ret = libc_fwrite(ptr, size, nmemb, stream);
	enable_cache_management();

	return ret;
}



