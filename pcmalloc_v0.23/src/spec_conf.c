
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "config.h"
#include "build_in.h"
#include "cpuaffi.h"
#include "allocator.h"
#include "pc_malloc.h"


static char *bin_name;
static char *default_bin_name = "unknown";
static char *ctl = "/home/grt/dbg/spec.ctl";
static char *outdir = "/home/grt/dbg/";
static char *outposfix = "";
static char *out;
static int bin_subidx;

static void
parse_bin_name(char *s)
{
	int i, len, bin_len;
	char *p, *start, *end;

	len = strlen(s);
	p = start = s;
	while (len--){
		if (*p == '/')
			start = p;
		p++;	
	}
	
	end = start;
	while (end != s + len && *end != '.')
		end++;

	if (end == s + len) {
		bin_name = default_bin_name;
		return;
	}

	bin_len = (int)(end - start);
	bin_name = (char*)pc_malloc(OPEN_MAPPING, bin_len);
	start++;

	for (i = 0; i < bin_len; i++) {
		bin_name[i] = *(start + i);	
	}
	bin_name[bin_len - 1] = '\0';
	printf("bin name : %s\n", bin_name);
}

static void
cpu_config()
{
	FILE *f;
	int buf_sz = 256;
	char *buf, *subname;
	char *p;
	int cpuid;
	int len;

	f = fopen(ctl, "r+");
	if (f == NULL)
		return;
	buf = (char*)pc_malloc(OPEN_MAPPING, buf_sz);	
	subname = (char*)pc_malloc(OPEN_MAPPING, buf_sz);

	strcpy(subname, bin_name);
	p = subname + strlen(subname);
	sprintf(p, "%d", bin_subidx);
	*(p+1) = '\0';
	p = NULL;
	while (!feof(f)) {
		fgets(buf, buf_sz, f);
		p = strstr(buf, subname);
		if (p != NULL)
			break;
	}
	if (p != NULL) {
		len = strlen(subname);	
		p += len + 1;
	}
	cpuid = atoi(p);
	set_cpu_affinity(cpuid);
	printf("cpu : %d\n", cpuid);

	fclose(f);
}

static void
out_path_config()
{
	int len;
	char *p;
	FILE *f;
	int i;
	char idx[8];

	len = strlen(outdir);
	len += strlen(bin_name);
	len += strlen(outposfix);
	len += 64;

	out = (char*)pc_malloc(OPEN_MAPPING, len);
	p = out;
	
	strcpy(p, outdir);
	p += strlen(outdir);
	strcpy(p, bin_name);
	p += strlen(bin_name);
	strcpy(p, outposfix);
	p += strlen(outposfix);
	i = 0;	
	while (1) {
		sprintf(idx, "%d", i);	
		strcpy(idx + strlen(idx), "_");
		strcpy(p, idx);
		bin_subidx = i;
		f = fopen(out, "r");
		if (f == NULL)
			break;
		fclose(f);
		i++;
	}

	printf("profile message path : %s\n", out);
	f = fopen(out, "a+");
	fprintf(f, "\n");
	fclose(f);
}

static inline void
spec_config(char *path)
{
	parse_bin_name(path);
	out_path_config();
	cpu_config();
}

char *
get_out_path()
{
	return out;
}

static inline char *
get_bin_name()
{
	return bin_name;
}

static void clsa_begin(char *bin_path)
{
	malloc_init();

	spec_config(bin_path);
}

#define extend_function_enable(path) \
clsa_begin(path)

void normal_enable_(char *bin_path)
{
	extend_function_enable(bin_path);
}

void bwaves_enable_()
{
	extend_function_enable("/bwaves_base.amd64-m64-gcc43-nn");
}

void gamess_enable_()
{
	extend_function_enable("/gamess_base.amd64-m64-gcc43-nn");
}

void leslie3d_enable_()
{
	extend_function_enable("/leslie3d_base.amd64-m64-gcc43-nn");
}

void gemsfdtd_enable_()
{
	extend_function_enable("/GemsFDTD_base.amd64-m64-gcc43-nn");
}

void wrf_enable_()
{
	extend_function_enable("/wrf_base.amd64-m64-gcc43-nn");
}

void tonto_enable_()
{
	extend_function_enable("/tonto_base.amd64-m64-gcc43-nn");
}

void
clsa_monit_start_()
{
	pc_malloc_enable();
}

void
clsa_monit_enable_()
{
	malloc_init();
}

void
clsa_monit_end_()
{
	malloc_destroy();
}














