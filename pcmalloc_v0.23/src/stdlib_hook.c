#include <unistd.h>
#include <execinfo.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>


#include "chunk_monitor.h"
#include "stdlib_hook.h"


typedef ssize_t (*func_read)(int fd, void *buf, size_t count);
typedef ssize_t (*func_write)(int fd, const void *buf, size_t count);
typedef void* (*func_fopen)(const char *path, const char *mode);
typedef int (*func_fclose)(FILE *fp);
typedef size_t (*func_fread)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*func_fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef int (*func_fputs)(const char *s, FILE *stream);
typedef void (*func_setbuf)(FILE *stream, char * buf);

static func_read libc_read;
static func_write libc_write;
static func_fopen libc_fopen;
static func_fclose libc_fclose;
static func_fread libc_fread;
static func_fwrite libc_fwrite;
static func_fputs libc_fputs;
static func_setbuf libc_setbuf;


int
install_stdlibapi_hook()
{
	void *ld_handle;

	ld_handle = dlopen("libc.so.6", RTLD_LAZY);
	libc_read = (func_read)dlsym(ld_handle, "read");
	libc_write = (func_write)dlsym(ld_handle, "write");
	libc_fopen = (func_fopen)dlsym(ld_handle, "fopen");
	libc_fclose = (func_fclose)dlsym(ld_handle, "fclose");
	libc_fread = (func_fread)dlsym(ld_handle, "fread");
	libc_fwrite = (func_fwrite)dlsym(ld_handle, "fwrite");
	libc_fputs = (func_fputs)dlsym(ld_handle, "fputs");
	libc_setbuf = (func_setbuf)dlsym(ld_handle, "setbuf");

	return 0;
}

ssize_t
read(int fd, void *buf, size_t count) {
	remove_sample_range((unsigned long)buf, (unsigned long)count);
	return libc_read(fd, buf, count);
}

ssize_t
write(int fd, const void *buf, size_t count) {
	remove_sample_range((unsigned long)buf, (unsigned long)count);
	return libc_write(fd, buf, count);
}


FILE *
fopen(const char *path, const char *mode)
{
	return libc_fopen(path, mode);
}

int
fclose(FILE *fp)
{
	return libc_fclose(fp);
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);
	return libc_fread(ptr, size, nmemb, stream);
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	remove_sample_range((unsigned long)ptr, (unsigned long)size * nmemb);
	return libc_fwrite(ptr, size, nmemb, stream);
}

int
fputs(const char *s, FILE *stream)
{
	return libc_fputs(s, stream);
}

void
setbuf(FILE *stream, char *buf)
{
	libc_setbuf(stream, buf);
}



