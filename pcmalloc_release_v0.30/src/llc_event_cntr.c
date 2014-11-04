#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "utl_builtin.h"
#include "pc_malloc.h"
#include "llc_event_cntr.h"


#ifdef USE_PERF_EVENT

#include "perf_event.h"

#define PERF_LLC_MISS_CNTR_IDX		1
#define PERF_LLC_ACCESS_CNTR_IDX	2

#define LLC_CNTR_BUF_SIZE		(NR_LLC_PERFEVENT + 1)

static const int cntr_buf_size = LLC_CNTR_BUF_SIZE * sizeof(uint64_t);

static int event_fd[NR_LLC_PERFEVENT];
static uint64_t perf_cntr_buf[LLC_CNTR_BUF_SIZE];


static uint64_t last_llc_miss = 0;
static uint64_t last_llc_access = 0;

int 
llc_event_cntr_init()
{
	struct perf_event_attr attr;
	int fd, ret, fd_idx;
	uint64_t flag, read_format;

	memset(event_fd, 0, sizeof(int) * NR_LLC_PERFEVENT);
	fd_idx = 0;

	flag = PERF_ATTR_DISABLED | PERF_ATTR_PINNED | 
		PERF_ATTR_EXCL_KERL | PERF_ATTR_EXCL_HV;	
	read_format = PERF_FORMAT_GROUP;
	perf_event_attr_setup(&attr, PERF_EVENT_L3_TCM, flag, read_format);
	ret = fd = sys_perf_event_open(&attr, getpid(), -1, -1, 0);
	if (ret < 0)
		goto free_fd;
	event_fd[fd_idx++] = fd;

	flag = PERF_ATTR_EXCL_KERL | PERF_ATTR_EXCL_HV;
	read_format = 0;
	perf_event_attr_setup(&attr, PERF_EVENT_L3_TCA, flag, read_format);
	ret = fd = sys_perf_event_open(&attr, getpid(), -1, event_fd[0], 0);
	if (ret < 0)
		goto free_fd;
	event_fd[fd_idx] = fd;

	return 0;

free_fd:
	for (fd_idx = 0; fd_idx < NR_LLC_PERFEVENT; fd_idx++) {
		if (event_fd[fd_idx]) 
			close(event_fd[fd_idx]);
		event_fd[fd_idx] = 0;
	}	
	return ret;
}

int 
llc_event_cntr_destroy()
{
	int fd_idx;

	for (fd_idx = 0; fd_idx < NR_LLC_PERFEVENT; fd_idx++) {
		if (event_fd[fd_idx]) 
			close(event_fd[fd_idx]);
		event_fd[fd_idx] = 0;
	}	

	return 0;
}

int 
llc_event_cntr_read(uint64_t *cntr_buf, int size)
{
	int group_fd;

	group_fd = event_fd[0];
#ifdef USE_ASSERT
	assert(group_fd != 0);
#endif //USE_ASSERT

	read(group_fd, perf_cntr_buf, cntr_buf_size);

#ifdef USE_ASSERT
	assert(perf_cntr_buf[0] == NR_LLC_PERFEVENT);
#endif //USE_ASSERT

	if (unlikely(last_llc_access == 0)) {
		last_llc_miss = perf_cntr_buf[PERF_LLC_MISS_CNTR_IDX];	
		last_llc_access = perf_cntr_buf[PERF_LLC_ACCESS_CNTR_IDX];	
		cntr_buf[LLC_MISS_CNTR_IDX] 
			= perf_cntr_buf[PERF_LLC_MISS_CNTR_IDX]; 
		cntr_buf[LLC_ACCESS_CNTR_IDX] 
			= perf_cntr_buf[PERF_LLC_ACCESS_CNTR_IDX];
	} else {
		cntr_buf[LLC_MISS_CNTR_IDX]
			= (perf_cntr_buf[PERF_LLC_MISS_CNTR_IDX] - last_llc_miss) >> 32 
			? last_llc_miss : perf_cntr_buf[PERF_LLC_MISS_CNTR_IDX];
		cntr_buf[LLC_ACCESS_CNTR_IDX]
			= (perf_cntr_buf[PERF_LLC_ACCESS_CNTR_IDX] - last_llc_access) >> 32
			? last_llc_access : perf_cntr_buf[PERF_LLC_ACCESS_CNTR_IDX];
	}

	return 0;
}

int 
llc_event_cntr_start()
{
	int fd_idx, ret;

	for (fd_idx = 0; fd_idx < NR_LLC_PERFEVENT; fd_idx++) {
		ret = 0;
		if (event_fd[fd_idx])
			ret = ioctl(event_fd[fd_idx], PERF_EVENT_IOC_ENABLE, 0);
		if (ret) return ret;
	}
	return 0;
}

int 
llc_event_cntr_stop()
{
	int fd_idx, ret;

	for (fd_idx = 0; fd_idx < NR_LLC_PERFEVENT; fd_idx++) {
		ret = 0;
		if (event_fd[fd_idx])
			ret = ioctl(event_fd[fd_idx], PERF_EVENT_IOC_DISABLE, 0);
		if (ret) return ret;
	}
	return 0;
}

#endif /* USE_PERF_EVENT */





#ifdef USE_PAPI


#include "papi.h"
#include "papiStdEventDefs.h"

static int event_set = PAPI_NULL;

char *l2_lines_in_name = L2_MISS_EVENT_NAME;
char *llc_miss_event_name = L3_MISS_EVENT_NAME;
char *llc_access_event_name = L3_ACCESS_EVENT_NAME;
#if 0
char *llc_miss_event_name = "L2_LINES_IN";
char *llc_access_event_name = "ix86arch::LLC_REFERENCES";
#endif

#define PAPI_L2_LINES_IN_CNTR_IDX	0	
#define PAPI_LLC_MISS_CNTR_IDX		1
#define PAPI_LLC_ACCESS_CNTR_IDX	2

#define LLC_CNTR_BUF_SIZE		3


static long long papi_cntr_buf[LLC_CNTR_BUF_SIZE];

int 
llc_event_cntr_init()
{
	int ret;
	int event_code;

	ret = PAPI_library_init(PAPI_VER_CURRENT);
	assert(ret == PAPI_VER_CURRENT);

	ret = PAPI_create_eventset(&event_set);
	assert(ret == PAPI_OK);

	ret = PAPI_event_name_to_code(l2_lines_in_name, &event_code);
	assert(ret == PAPI_OK);

	ret = PAPI_add_event(event_set, event_code);
	assert(ret == PAPI_OK);

	ret = PAPI_event_name_to_code(llc_miss_event_name, &event_code);
	assert(ret == PAPI_OK);

	ret = PAPI_add_event(event_set, event_code);
	assert(ret == PAPI_OK);

	ret = PAPI_event_name_to_code(llc_access_event_name, &event_code);
	assert(ret == PAPI_OK);

	ret = PAPI_add_event(event_set, event_code);
	assert(ret == PAPI_OK);

#if 0
	event_code = PAPI_L3_TCM;
	ret = PAPI_add_event(event_set, event_code);
	assert(ret == PAPI_OK);

	event_code = PAPI_L3_TCA;
	ret = PAPI_add_event(event_set, event_code);
	assert(ret == PAPI_OK);
#endif

	return 0;
}


int 
llc_event_cntr_destroy()
{
	int ret;

	ret = PAPI_cleanup_eventset(event_set);
	assert(ret == PAPI_OK);

	ret = PAPI_destroy_eventset(&event_set);
	assert(ret == PAPI_OK);

	return 0;
}


int 
llc_event_cntr_read(uint64_t *cntr_buf, int size)
{
	PAPI_read(event_set, papi_cntr_buf);
	cntr_buf[LLC_MISS_CNTR_IDX] = papi_cntr_buf[PAPI_LLC_MISS_CNTR_IDX];
	cntr_buf[LLC_ACCESS_CNTR_IDX] = papi_cntr_buf[PAPI_LLC_ACCESS_CNTR_IDX];

	if (papi_cntr_buf[PAPI_L2_LINES_IN_CNTR_IDX]
			> papi_cntr_buf[PAPI_LLC_ACCESS_CNTR_IDX]) {
		cntr_buf[LLC_MISS_CNTR_IDX]
			+= papi_cntr_buf[PAPI_L2_LINES_IN_CNTR_IDX]
			- papi_cntr_buf[PAPI_LLC_ACCESS_CNTR_IDX];
	}

	return 0;
}


int 
llc_event_cntr_start()
{
	int ret;

	ret = PAPI_start(event_set);
	assert(ret == PAPI_OK);

	return 0;
}

int 
llc_event_cntr_stop()
{
	int ret;

	ret = PAPI_stop(event_set, NULL);
	assert(ret == PAPI_OK);

	return 0;
}

#endif /* USE_PAPI */

