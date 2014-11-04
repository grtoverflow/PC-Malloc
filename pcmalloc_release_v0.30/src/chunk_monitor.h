#ifndef CHUNK_MONITOR_H_
#define CHUNK_MONITOR_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#include "utl_list.h"
#include "pc_malloc.h"
#include "llc_event_cntr.h"


#define SAMPLE_SHIFT	(PAGE_SHIFT + 0)
#define SAMPLE_RANGE	(1UL << SAMPLE_SHIFT)

#define SAMPLE_TIMES			SAMPLE_ITERATION
#define SAMPLE_FREQUENCY		100000

#define MIN_SAMPLE_CYCLE		2
#define MAX_SAMPLE_CYCLE		8


/* sample state */
#define START_SAMPLE	0
#define FIRST_ROUND 	1
#define SECOND_ROUND 	2
#define LAST_ROUND 	3


struct page_sample {
	unsigned long addr;

	uint64_t llc_event_cntr[NR_LLC_PERFEVENT];

	uint32_t llc_victim_ref;
	uint32_t llc_pollutor_ref;
	uint32_t total_ref;

	int state;
	int wait_timer;
	int wait_page_fault;

	struct memory_chunk *chunk;
	struct list_head sibling;
	struct list_head p;
};

int chunk_monitor_init();
void chunk_monitor_destroy();

int monit_chunk(struct memory_chunk *chunk);
int stop_monit_chunk(struct memory_chunk *chunk);

int tst_monit_complete(struct memory_chunk *chunk);
uint32_t get_victim_samples(struct memory_chunk *chunk);
uint32_t get_pollutor_samples(struct memory_chunk *chunk);
int chunk_under_monit(struct memory_chunk *chunk);

void remove_sample_range(unsigned long start, unsigned long len);


#endif /* CHUNK_MONITOR_H_ */


