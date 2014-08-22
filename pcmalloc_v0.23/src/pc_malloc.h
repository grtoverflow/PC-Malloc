#ifndef PC_MALLOC_H_
#define PC_MALLOC_H_

#include <stddef.h>

#include "chunk_monitor.h"

#ifndef PC_MALLOC_TYPE
#define RESTRICT_MAPPING		2
#define OPEN_MAPPING			3
#endif /* PC_MALLOC_TYPE */

#define CALL_STACK_DEPTH	16

#define SMALL_MEMORY_CHUNK	8192

#define MAPPING_FROM_SAMPLING	1
#define MAPPING_FROM_PREDICTION	2

#define POLLUTOR_THRESHOLD		0.9

#define BASE_SAMPLE_INTERVAL	5000000	/* us */

struct page_sample;
struct alloc_context;

struct memory_chunk {
	unsigned long addr;
	size_t size;
	unsigned long idx;

	int under_sampling;
	int state;

	uint32_t llc_victim_ref;
	uint32_t llc_pollutor_ref;
	uint32_t total_ref;
	float mr;

	uint32_t i_victim_ref;
	uint32_t i_pollutor_ref;
	uint32_t i_total_ref;

	int mapping_type;
	int mapping_source;

	int base_sample_cycle;
	int sample_cycle;

	struct timeval sample_interval;

	int nr_sample;
	uint32_t nr_sample_complete;

	struct alloc_context *context;
	struct list_head sample;
	struct list_head p;
	struct list_head sibling;
};

struct alloc_context {
	uint64_t call_stack[CALL_STACK_DEPTH];	
	int stack_depth;
	uint64_t context_key;
	unsigned long idx;

	int predict_type[2];
	float predict_mr[2];
	int sample_skip;
	int skip_interval;

	unsigned long nr_pollutor;
	unsigned long nr_victim;
	unsigned long nr_chunks;
	unsigned long nr_freed;
	size_t last_chunk_sz;

	struct list_head chunk;
	struct list_head sibling;
	struct list_head p;
};


int malloc_init();
void malloc_destroy();
void pc_malloc_enable();
void update_context_mapping_type(struct memory_chunk *chunk);

/* cache size assigned under different memory mappings */
int cache_size_under_restrict_mapping(); 
int cache_size_under_open_mapping(); 
int cache_line_under_restrict_mapping();
int cache_line_under_open_mapping();

/* interfaces for explicit llc control */
extern void* pc_malloc(int type, size_t sz);
extern void* pc_realloc(int type, void *p, size_t newsize);
extern void* pc_calloc(int type, size_t nmemb, size_t sz);
extern void pc_free(void *p);

/* interfaces for automatic llc control */
void* malloc(size_t sz);
void* realloc(void *p, size_t newsize);
void* calloc(size_t nmemb, size_t sz);
void free(void *p);

#endif /* PC_MALLOC_H_ */
