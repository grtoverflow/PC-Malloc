#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>

#include "config.h"
#include "build_in.h"
#include "chunk_monitor.h"
#include "pc_malloc.h"
#include "time_event_queue.h"
#include "page_sample_map.h"
#include "llc_event_cntr.h"
#include "spec_conf.h"


/* monitor state */
#define MONITOR_UNINIT		0
#define MONITOR_ACTIVE		1
#define MONITOR_DESTROYED	2

/* chunk state */
#define CHUNK_NO_MONIT		0
#define CHUNK_PENDING		1
#define CHUNK_UNDER_MONIT	2
#define CHUNK_MONIT_FINISH	3

/* sample state */
#define SAMPLE_UNINIT			0
#define WAIT_FIRST_ACCESS		1
#define WAIT_SUCCESSIVE_ACCESS	2
#define BURST_ACCESS_SKIP		3


/* burst settings */
#define DEFAULT_BURST_LEN		500
#define MAX_BURST_LEN			5000
#define BURST_SKIP_UPDATE_INTERVAL	100000


/* buffer for llc events */
static uint64_t llc_event_cntr[NR_LLC_PERFEVENT];
static const int cntr_buf_size = NR_LLC_PERFEVENT * sizeof(uint64_t);
static int llc_sz_in_line;
static int llc_sz;
static int cache_region_sz_in_line;
static int cache_region_sz;

/* chunk monitor */
struct chunk_monitor {
	int monitor_state;

	/* burst_skip */
	uint64_t burst_skip_len;
	uint64_t llc_acc_at_skip_update;
	struct timeval tv_at_skip_update;
	int nr_skip_update_attempt;
} chunk_monitor;


/* burst skip handling*/
static int
init_burst_skip()
{
	gettimeofday(&chunk_monitor.tv_at_skip_update, NULL);

	llc_event_cntr_read(llc_event_cntr, cntr_buf_size);
	chunk_monitor.llc_acc_at_skip_update
		= llc_event_cntr[LLC_ACCESS_CNTR_IDX];	

	chunk_monitor.burst_skip_len = DEFAULT_BURST_LEN;
	chunk_monitor.nr_skip_update_attempt = 0;

	return 0;
}

static void
burst_skip_update(uint64_t *llc_event_cntr)
{
	uint64_t usec_passed;
	uint64_t llc_acc_occur;
	uint64_t llc_acc;
	struct timeval now;
	uint64_t skip_len;

	llc_acc = llc_event_cntr[LLC_ACCESS_CNTR_IDX];
	if (unlikely(llc_acc < chunk_monitor.llc_acc_at_skip_update))
		return;
	llc_acc_occur = llc_acc - chunk_monitor.llc_acc_at_skip_update;
	if (llc_acc_occur < cache_region_sz_in_line)
		return;
	gettimeofday(&now, NULL);
	usec_passed = (now.tv_sec - chunk_monitor.tv_at_skip_update.tv_sec)
			* 1000000 + now.tv_usec 
			- chunk_monitor.tv_at_skip_update.tv_usec;
	if (usec_passed < BURST_SKIP_UPDATE_INTERVAL)
		return;

	skip_len = usec_passed * cache_region_sz_in_line / llc_acc_occur;
#if 0
	skip_len = skip_len < DEFAULT_BURST_LEN
			? DEFAULT_BURST_LEN : skip_len > MAX_BURST_LEN
			? MAX_BURST_LEN : skip_len;
#endif
	skip_len = skip_len < DEFAULT_BURST_LEN
			? DEFAULT_BURST_LEN : skip_len;

	chunk_monitor.burst_skip_len = skip_len;

	chunk_monitor.tv_at_skip_update = now;
	chunk_monitor.llc_acc_at_skip_update = llc_acc;
}


/* page sample operations */
static inline void
init_page_sample(struct page_sample *sample, 
		struct memory_chunk *chunk, unsigned long addr)
{
	int i;

	for (i = 0; i < NR_LLC_PERFEVENT; i++)
		sample->llc_event_cntr[i] = 0;

	sample->llc_victim_ref = 0;
	sample->llc_pollutor_ref = 0;
	sample->total_ref = 0;

	sample->addr = addr & PAGE_MASK;
	list_add(&sample->sibling, &chunk->sample);
	sample->chunk = chunk;
	sample->state = WAIT_FIRST_ACCESS;

	sample->wait_timer = 0;
	sample->wait_page_fault = 0;
}

extern int errno;

static inline int
enable_page_sample(struct page_sample *sample)
{
	return mprotect((void*)sample->addr, PAGE_SIZE, PROT_NONE);
}

static inline int
disable_page_sample(struct page_sample *sample)
{
	return mprotect((void*)sample->addr, PAGE_SIZE, PROT_READ | PROT_WRITE);
}

static inline void
remove_page_sample(struct page_sample *sample)
{
	if (sample->wait_timer) {
		remove_time_event(sample); 
		sample->wait_timer = 0;
	}

	disable_page_sample(sample);
	sample->wait_page_fault = 0;

	list_del(&sample->sibling);
	detach_page_sample(sample);
	sample->chunk = NULL;

	sample->state = SAMPLE_UNINIT;
}

static void
remove_chunk_samples(struct memory_chunk *chunk)
{
	struct page_sample *sample;

	while(!list_empty(&chunk->sample)) {
		sample = next_entry(&chunk->sample, struct page_sample, sibling);
		remove_page_sample(sample);
	}

	chunk->nr_sample = 0;
}

static inline void
copy_llc_event_cntr(uint64_t *cntr_buf0, uint64_t *cntr_buf1)
{
	cntr_buf0[LLC_MISS_CNTR_IDX] = cntr_buf1[LLC_MISS_CNTR_IDX];
	cntr_buf0[LLC_ACCESS_CNTR_IDX] = cntr_buf1[LLC_ACCESS_CNTR_IDX];
}


/* sample collection */
#define is_burst_access_sample(delta_n) \
(delta_n < (cache_region_sz_in_line) ? 1 : 0)

#define is_victim_sample(delta_m) \
(delta_m * PREFETCH_BATCH < llc_sz_in_line ? 1 : 0)

#define SAMPLE_COMPLETE		1
#define WAIT_NEXT_SAMPLE	2

static int 
page_sample_complete(struct page_sample *sample)
{
	struct memory_chunk *chunk;
	float mr0, mr1, eps;
	int ret;

	chunk = sample->chunk;
	if (chunk->state == CHUNK_MONIT_FINISH) {
		return SAMPLE_COMPLETE;
	}
	
	ret = 0;

	#if 0
	printf("chunk %lu cycle %d %d i_total_ref %d nr_sample %d\n",
		chunk->idx, chunk->base_sample_cycle, chunk->sample_cycle,
		chunk->i_total_ref, chunk->nr_sample);
	#endif
	if (chunk->i_total_ref == chunk->nr_sample) {
		if (chunk->sample_cycle < chunk->base_sample_cycle - 1) {
			ret = 0;	
		} else {
		#if 0
		printf("chunk %lu llc_pollutor_ref %d total_ref %d i_pollutor_ref %d i_total_ref %d\n",
				chunk->idx, chunk->llc_pollutor_ref, chunk->total_ref,
				chunk->i_pollutor_ref, chunk->i_total_ref);
		#endif
			mr0 = chunk->total_ref == 0 ? 0.0 
				: (float)chunk->llc_pollutor_ref / (float)chunk->total_ref;
			mr1 = (float)(chunk->llc_pollutor_ref + chunk->i_pollutor_ref)
				/ (float)(chunk->total_ref + chunk->i_total_ref);
			eps = mr0 > mr1 ? mr0 - mr1 : mr1 - mr0;
			if (eps < MONIT_CONV_ERR) {
				ret = WAIT_NEXT_SAMPLE;
				chunk->state = CHUNK_MONIT_FINISH;
			} else {
				ret = 0;
			}
		}
		chunk->llc_victim_ref += chunk->i_victim_ref;	
		chunk->llc_pollutor_ref += chunk->i_pollutor_ref;
		chunk->total_ref += chunk->i_total_ref;
		chunk->i_victim_ref = 0;
		chunk->i_pollutor_ref = 0;
		chunk->i_total_ref = 0;
		chunk->sample_cycle++;
		#if 0
		printf("chunk %lu eps %.4f iter %d\n", 
			chunk->idx, eps, chunk->sample_cycle);
		#endif
	}

	if (chunk->state == CHUNK_MONIT_FINISH) {
		update_context_mapping_type(chunk);
	}

	return ret;
}

static inline int
update_chunk_sample(struct page_sample *sample, uint64_t *cntr)
{
	struct memory_chunk *chunk;
	uint64_t delta_n, delta_m;

	chunk = sample->chunk;
	delta_n = cntr[LLC_ACCESS_CNTR_IDX] 
			- sample->llc_event_cntr[LLC_ACCESS_CNTR_IDX];
	if (is_burst_access_sample(delta_n)) {
	//	printf("is_burst_access_sample delta_n %lu\n", delta_n);
		return 1;
	}

	delta_m = cntr[LLC_MISS_CNTR_IDX] 
			- sample->llc_event_cntr[LLC_MISS_CNTR_IDX];
	if (is_victim_sample(delta_m)) {
	//	printf("is_victim_sample delta_n %lu delta_m %lu\n", delta_n, delta_m);
		sample->llc_victim_ref++;
		chunk->i_victim_ref++;	
	} else {
	//	printf("is_pollutor_sample delta_n %lu delta_m %lu\n", delta_n, delta_m);
		sample->llc_pollutor_ref++;
		chunk->i_pollutor_ref++;	
	}

	sample->total_ref++;
	chunk->i_total_ref++;

	return 0;
}

static void
last_chunk_sample_update(struct memory_chunk *chunk, uint64_t *cntr)
{
	struct page_sample *sample;	
	uint64_t delta_m;

	chunk->llc_victim_ref += chunk->i_victim_ref;
	chunk->llc_pollutor_ref += chunk->i_pollutor_ref;
	chunk->total_ref += chunk->i_total_ref;

	if (chunk->state == CHUNK_MONIT_FINISH)
		return;
	
	list_for_each_entry (sample, &chunk->sample, sibling) {
		if (sample->llc_event_cntr[LLC_MISS_CNTR_IDX] == 0) {
			continue;
		}
		delta_m = cntr[LLC_MISS_CNTR_IDX] 
				- sample->llc_event_cntr[LLC_MISS_CNTR_IDX];
		if (chunk->size >= llc_sz || !is_victim_sample(delta_m)) {
			sample->llc_pollutor_ref++;
			sample->total_ref++;
			chunk->llc_pollutor_ref++;	
			chunk->total_ref++;
		}
	}
}

static void burst_skip_handler();

static void
collect_page_sample(struct page_sample *sample, unsigned long addr)
{
	struct timeval tv;
	struct memory_chunk *chunk;
	int ret;

	chunk = sample->chunk;

	llc_event_cntr_read(llc_event_cntr, cntr_buf_size);
	burst_skip_update(llc_event_cntr);

	switch (sample->state) {
	case WAIT_FIRST_ACCESS:
		usec2tv(&tv, chunk_monitor.burst_skip_len);
		sample->state = BURST_ACCESS_SKIP;
		copy_llc_event_cntr(sample->llc_event_cntr, llc_event_cntr);
		break;

	case WAIT_SUCCESSIVE_ACCESS:
		if (update_chunk_sample(sample, llc_event_cntr) == 0) {
			/* the sample has been taken */
			usec2tv(&tv, SAMPLE_FREQUENCY);
			sample->state = WAIT_FIRST_ACCESS;
		} else {
			usec2tv(&tv, chunk_monitor.burst_skip_len);
			sample->state = BURST_ACCESS_SKIP;
		}
		break;
	}

	ret = page_sample_complete(sample);
	if (ret == SAMPLE_COMPLETE) {
		sample->state = SAMPLE_UNINIT;
	} else if (ret == WAIT_NEXT_SAMPLE){
		sample->state = SAMPLE_UNINIT;
	} else {
		add_time_event(&tv, burst_skip_handler, (void*)sample);
		sample->wait_timer = 1;
	}

#if 0
	if (chunk->nr_sample_complete == chunk->nr_sample * (SAMPLE_TIMES >> 1))
	printf("idx %lu size %u victim %u pollutor %u total %u\n",
			chunk->idx, (unsigned)chunk->size, 
			chunk->llc_victim_ref, chunk->llc_pollutor_ref, chunk->total_ref);
#endif
}


/* signal handling */
static void
burst_skip_handler(void *private, struct timeval *tv)
{
	struct page_sample *sample;

	sample = (struct page_sample*)private;
	sample->wait_timer = 0;

#ifdef USE_ASSERT
	assert(sample->chunk != NULL);
#endif /* USE_ASSERT */

	if (page_sample_complete(sample)
			|| sample->chunk->state == CHUNK_MONIT_FINISH) {
		sample->state = SAMPLE_UNINIT;
		return;
	}

#ifdef USE_ASSERT
	assert(sample->chunk->state == CHUNK_UNDER_MONIT
		&& (sample->state == BURST_ACCESS_SKIP
		|| sample->state == WAIT_FIRST_ACCESS)); 
#endif /* USE_ASSERT */
	
	switch (sample->state) {
	case WAIT_FIRST_ACCESS:
	case WAIT_SUCCESSIVE_ACCESS:
		break;
	case BURST_ACCESS_SKIP:
		sample->state = WAIT_SUCCESSIVE_ACCESS;
		break;
	}

#ifdef USE_ASSERT
	assert(enable_page_sample(sample) == 0);
#else
	enable_page_sample(sample);
#endif /* USE_ASSERT */

	sample->wait_page_fault = 1;

	#if 0
	if (likely(enable_page_sample(sample) == 0)) {
		sample->wait_page_fault = 1;
	} else {
		printf("enable_page_sample failed\n");
		sample->wait_page_fault = 0;
		sample->state = SAMPLE_UNINIT;
		if (!page_sample_complete(sample)) {
			sample->chunk->nr_sample_complete++;
		}
	}
	#endif
}

static void
page_fault_handler(int sig, siginfo_t *si, void *context)
{
	struct page_sample *sample;
	unsigned long addr;

	addr = (unsigned long)si->si_addr;

	sample = get_page_sample(addr); 

	if (sample == NULL || sample->chunk == NULL)
		while(1);
#ifdef USE_ASSERT
	assert(sample != NULL && sample->chunk != NULL);
#endif /* USE_ASSERT */

	sample->wait_page_fault = 0;
	if (page_sample_complete(sample) 
			|| sample->chunk->state == CHUNK_MONIT_FINISH) {
		disable_page_sample(sample);
		sample->state = SAMPLE_UNINIT;
		return;
	}

#ifdef USE_ASSERT
	assert(sample->chunk->state == CHUNK_UNDER_MONIT
			&& (sample->state == WAIT_FIRST_ACCESS 
			|| sample->state == WAIT_SUCCESSIVE_ACCESS));
#endif /* USE_ASSERT */
	
#ifdef USE_ASSERT
	assert(disable_page_sample(sample) == 0);
#else
	disable_page_sample(sample);
#endif /* USE_ASSERT */

	collect_page_sample(sample, addr);
}

static inline int
regist_page_fault_handler()
{
	struct sigaction sa;
	int ret;

	sa.sa_sigaction = page_fault_handler;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGSEGV);
	ret = sigaction(SIGSEGV, &sa, NULL);

	return ret;
}

static inline int
get_nr_sample(unsigned long range)
{
	int n;
	
	n = (int)sqrt(range >> PAGE_SHIFT);
	n = n < (range >> PAGE_SHIFT) ? n + 1 : n;

	return n;
}

static inline unsigned long
get_sample_addr(unsigned long start, unsigned long end,
		unsigned long range, int i, int n)
{
	unsigned long per_sample_range;
	unsigned long offset, sample_addr;
	unsigned long bound;

	per_sample_range = range / n;
	offset = (((rand() * rand()) % per_sample_range) + i * per_sample_range) & PAGE_MASK;
	bound = ((i + 1) * per_sample_range) & PAGE_MASK;
	offset = offset >= bound ? bound - PAGE_SIZE : offset;
	sample_addr = start + offset;
	sample_addr = sample_addr >= end ? end - PAGE_SIZE : sample_addr;

	return sample_addr;
}

static inline int
get_base_sample_cycle(unsigned long range)
{
	int cycle;

	range >>= 14;
	cycle = MIN_SAMPLE_CYCLE;

	while (cycle != MAX_SAMPLE_CYCLE) {
		if (range == 0)	
			break;
		cycle++;
		range >>= 1;
	}

	return cycle;
}

/* interface */
int
monit_chunk(struct memory_chunk *chunk)
{
	unsigned long start, addr;
	size_t size;
	unsigned long start_align, end_align;
	unsigned long total_range; 
	int nr_sample;
	struct page_sample *sample;
	int i;

	if (chunk->state == CHUNK_UNDER_MONIT)
		return 0;

	pend_time_event_queue();

	/* init monit info */
	start = chunk->addr;
	size = chunk->size;
	start_align = UPPER_PAGE_ALIGN(start);
	end_align = LOWER_PAGE_ALIGN(start + size);
	total_range = end_align - start_align;
	nr_sample = get_nr_sample(total_range);
	chunk->nr_sample = nr_sample;
	chunk->base_sample_cycle = get_base_sample_cycle(total_range);
	list_init(&chunk->sample);

	addr = start_align;
	/* configure page samples */
	for (i = 0; i < nr_sample; i++) {
		addr = get_sample_addr(start_align, end_align, total_range, i, nr_sample);	
		sample = attach_page_sample(addr);
		init_page_sample(sample, chunk, addr);
#ifdef USE_ASSERT
		assert(enable_page_sample(sample) == 0);
#else
		enable_page_sample(sample);
#endif /* USE_ASSERT */
		sample->wait_page_fault = 1;
		#if 0
		if (likely(enable_page_sample(sample) == 0)) {
			sample->wait_page_fault = 1;
		} else {
			printf("enable_page_sample failed\n");
			sample->wait_page_fault = 0;
			sample->state = SAMPLE_UNINIT;
			if (!page_sample_complete(sample)) {
				chunk->nr_sample_complete++;
			}
		}
		#endif
		addr += PAGE_SIZE;
	}

	chunk->state = CHUNK_UNDER_MONIT;

	resume_time_event_queue();

	return 0;
}

int
stop_monit_chunk(struct memory_chunk *chunk)
{

	pend_time_event_queue();

	if (chunk->state != CHUNK_MONIT_FINISH) {
		llc_event_cntr_read(llc_event_cntr, cntr_buf_size);
		last_chunk_sample_update(chunk, llc_event_cntr);
		update_context_mapping_type(chunk);
	}

	chunk->state = CHUNK_MONIT_FINISH;

	remove_chunk_samples(chunk);

	resume_time_event_queue();

	return 0;
}

int 
chunk_monitor_init()
{
	int ret;

	ret = time_event_queue_init();
	if (!!ret) 
		goto done;

	ret = regist_page_fault_handler();
	if (!!ret) 
		goto done;

	ret = page_sample_map_init();
	if (!!ret) 
		goto done;

	ret = llc_event_cntr_init();
	if (!!ret) 
		goto done; 
	ret = llc_event_cntr_start();
	if (!!ret) 
		goto done;

	ret = init_burst_skip();
	if (!!ret) 
		goto done;

	cache_region_sz_in_line = cache_line_under_restrict_mapping();
	llc_sz_in_line = cache_line_under_open_mapping();
	cache_region_sz = cache_size_under_restrict_mapping();
	llc_sz = cache_size_under_open_mapping();

	chunk_monitor.monitor_state = MONITOR_ACTIVE;

done:
	return ret;
}

void
chunk_monitor_destroy()
{
	llc_event_cntr_stop();
	llc_event_cntr_destroy();

	page_sample_map_destroy();

	time_event_queue_destroy();

	chunk_monitor.monitor_state = MONITOR_DESTROYED;
}

int
tst_monit_complete(struct memory_chunk *chunk)
{
	return 1;
}

uint32_t
get_victim_samples(struct memory_chunk *chunk)
{
	return chunk->llc_victim_ref;
}

uint32_t
get_pollutor_samples(struct memory_chunk *chunk)
{
	return chunk->llc_pollutor_ref;
}

int
chunk_under_monit(struct memory_chunk *chunk)
{
	return chunk->state != CHUNK_NO_MONIT;
}

void
remove_sample_range(unsigned long start, unsigned long len)
{
	unsigned long addr;
	unsigned long end;
	struct page_sample *sample;

	end = start + len;
	addr = start;

	while (addr <= end) {
		sample = get_page_sample(addr);	
		if (sample != NULL) {
			remove_page_sample(sample);	
		}
		addr += PAGE_SIZE;	
	}
}

