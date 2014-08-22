#include <unistd.h>
#include <execinfo.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>

#include "config.h"
#include "list.h"
#include "sys_descript.h"
#include "build_in.h"
#include "hash_map_64.h"
#include "allocator.h"
#include "pc_malloc.h"
#include "chunk_monitor.h"
#include "spec_conf.h"
#include "stdlib_hook.h"


#define CACHE_REGION_SZ_IN_LINE		(L3_SZ_IN_LINE >> 3)
#define SHARED_CACHE_SZ_IN_LINE		L3_SZ_IN_LINE

#define CACHE_REGION_SZ		(CACHE_REGION_SZ_IN_LINE << CACHE_LINE_SHIFT)
#define SHARED_CACHE_SZ		(SHARED_CACHE_SZ_IN_LINE << CACHE_LINE_SHIFT)

#define INNER_CALL_STACK_DEPTH	2
#define TOTAL_CALL_STACK_DEPTH	(CALL_STACK_DEPTH + INNER_CALL_STACK_DEPTH)

#define PC_MALLOC_UNINIT		0
#define PC_MALLOC_ACTIVE		1
#define CACHE_CONTROL_ENABLED	2

#define CHUNK_UNDER_SAMPLE		1
#define STANDALONE_CHUNK_SAMPLE	2


static int pc_malloc_state = PC_MALLOC_UNINIT;
static int malloc_depth = 0;

#define inc_malloc_depth() (malloc_depth++)
#define dec_malloc_depth() (malloc_depth--)
#define inner_malloc() (malloc_depth > 0)



#ifdef PCMALLOC_DEBUG
static inline void
print_chunk(struct memory_chunk *chunk)
{
	FILE *f;

	f = fopen(get_out_path(), "a+");
	fprintf(f, "\n");
	fprintf(f, "chunk : idx size mapping_source mapping_type llc_mr victim_sample pollutor_sample total_sample sample_page sample_cycle\n");
	fprintf(f, "%lu\n", chunk->idx);
	fprintf(f, "%d\n", (int)chunk->size);
	fprintf(f, "%d\n", chunk->mapping_source);
	fprintf(f, "%d\n", chunk->mapping_type);
	fprintf(f, "%.4f\n", chunk->mr);
	fprintf(f, "%u\n", chunk->llc_victim_ref);
	fprintf(f, "%u\n", chunk->llc_pollutor_ref);
	fprintf(f, "%u\n", chunk->total_ref);
	fprintf(f, "%d\n", chunk->nr_sample);
	fprintf(f, "%d\n", chunk->sample_cycle);
	fprintf(f, "\n");
	fclose(f);
}
#endif /* PCMALLOC_DEBUG */



static inline void
set_pc_malloc_active()
{
	pc_malloc_state |= PC_MALLOC_ACTIVE;
}

static inline int
pc_malloc_active()
{
	return pc_malloc_state & PC_MALLOC_ACTIVE;
}

static inline void
enable_cache_control()
{
	pc_malloc_state |= CACHE_CONTROL_ENABLED;
}

static inline int
cache_control_enabled()
{
	return pc_malloc_state & CACHE_CONTROL_ENABLED;
}


struct locality_profile {
	unsigned long chunk_idx;

	struct hash_map_64 *context_hash_map;
	struct list_head context;
	unsigned long context_idx;
} locality_profile;

static struct list_head free_context;
static struct list_head active_context;

static struct list_head free_chunk;
static struct list_head active_chunk;


static inline void
context_free(struct alloc_context *context)
{
	list_del(&context->p);
	list_del(&context->sibling);
	list_add(&context->p, &free_context);
}

static inline struct alloc_context *
context_alloc()
{
	struct alloc_context *context;

	if (unlikely(list_empty(&free_context))) {
		context = (struct alloc_context*)
			pc_malloc(OPEN_MAPPING, sizeof(struct alloc_context));
#ifdef USE_ASSERT
		assert(!!context);
#endif /* USE_ASSERT */
	} else {
		context = next_entry(&free_context, struct alloc_context, p);	
		list_del(&context->p);
	}

	context->nr_chunks = 0;
	context->nr_freed = 0;
	context->last_chunk_sz = 0;
	list_add(&context->p, &active_context);
	list_init(&context->chunk);
	list_init(&context->sibling);

	return context;
}

static inline void
chunk_free(struct memory_chunk *chunk)
{
	list_del(&chunk->p);
	list_add(&chunk->p, &free_chunk);
}

static inline struct memory_chunk *
chunk_alloc()
{
	struct memory_chunk *chunk;

	if (unlikely(list_empty(&free_chunk))) {
		chunk = (struct memory_chunk*)
			pc_malloc(OPEN_MAPPING, sizeof(struct memory_chunk));
#ifdef USE_ASSERT
		assert(!!chunk);
#endif /* USE_ASSERT */
	} else {
		chunk = next_entry(&free_chunk, struct memory_chunk, p);	
		list_del(&chunk->p);
	}

	chunk->llc_victim_ref = 0;
	chunk->llc_pollutor_ref = 0;
	chunk->total_ref = 0;

	chunk->i_victim_ref = 0;
	chunk->i_pollutor_ref = 0;
	chunk->i_total_ref = 0;

	chunk->sample_cycle = 0;

	chunk->nr_sample = 0;
	chunk->nr_sample_complete = 0;

	list_add(&chunk->p, &active_chunk);
	list_init(&chunk->sample);

	return chunk;
}


static inline uint64_t
get_context_key(unsigned long *call_stack, int depth) {
	uint64_t key;
	int i, offset;

	key = 0;
	offset = 0;
	i = 0;
	while (call_stack[i] != 0 && i < depth) {
		key ^= (call_stack[i] << offset) | (call_stack[i] >> (64 - offset));
		offset = (offset + 16) % 64;
		i++;
	}
	return key;
}

static inline int
call_stack_match(unsigned long *call_stack0, unsigned long *call_stack1,
		int depth0, int depth1)
{
	int i;

	if (depth0 != depth1)
		return 0;

	for (i = 0; i < depth0; i++) {
		if (call_stack0[i] != call_stack1[i])	
			return 0;
	}
	return 1;
}

static struct alloc_context* 
get_alloc_context()
{
	struct alloc_context *context, *citer, *entry;
	uint64_t key;
	int i, depth;
	static uint64_t total_call_stack[TOTAL_CALL_STACK_DEPTH];
	uint64_t *call_stack;

	depth = backtrace((void**)total_call_stack, TOTAL_CALL_STACK_DEPTH);
	call_stack = total_call_stack + INNER_CALL_STACK_DEPTH;
	depth -= INNER_CALL_STACK_DEPTH;

	key = get_context_key(call_stack, depth);

	entry = (struct alloc_context *) 
			hash_map_64_find_member(locality_profile.context_hash_map, key);
	if (unlikely(entry == NULL)) {
		goto add;
	} else {
		if (call_stack_match(call_stack, entry->call_stack, depth, entry->stack_depth)) {
			context = entry;
			goto done;
		}
	}
	
	if (unlikely(!list_empty(&entry->sibling))) {
		list_for_each_entry(citer, &entry->sibling, sibling) {
			if (call_stack_match(call_stack, citer->call_stack, depth, entry->stack_depth)) {
				context = citer;
				goto done;
			}
		}	
		goto add;
	}

add:
	context = context_alloc();	
	context->context_key = key;
	for (i = 0; i < depth; i++) {
		context->call_stack[i] = call_stack[i];	
	}
	context->stack_depth = depth;
	context->predict_type[0] = context->predict_type[1]	= UNKNOWN_MAPPING;
	context->predict_mr[0] = context->predict_mr[1] = -1;
	context->sample_skip = 0;
	context->skip_interval = 0;
	context->idx = locality_profile.context_idx++;
	context->nr_pollutor = 0;
	context->nr_victim = 0;

	if (likely(entry == NULL)) {
		hash_map_64_add_member(locality_profile.context_hash_map, key, context);
	} else {
		list_add(&context->sibling, &entry->sibling);
	}

done:
	return context;
}

static inline int
get_mapping_type(struct alloc_context *context)
{
	if (context->predict_type[0] == UNKNOWN_MAPPING
			&& context->predict_type[1]	== UNKNOWN_MAPPING) {
		return UNKNOWN_MAPPING;			
	} else if (context->predict_type[0] == OPEN_MAPPING
			|| context->predict_type[1] == OPEN_MAPPING) {
		return OPEN_MAPPING;			
	} else {
		return RESTRICT_MAPPING;	
	}
}

static inline float
get_predict_mr(struct alloc_context *context)
{
	if (context->predict_mr[0] == -1)
		return context->predict_mr[1];

	return context->predict_mr[0] < context->predict_mr[1]
		? context->predict_mr[0] : context->predict_mr[1];
}

static inline void
attach_chunk_to_context(void *p, size_t size, 
		struct alloc_context *context) 
{
	struct memory_chunk *chunk;

	chunk = chunk_alloc();

	chunk->addr = (unsigned long)p;
	chunk->size = size;
	chunk->idx = locality_profile.chunk_idx++;
	chunk->context = context;
	list_add(&chunk->sibling, &context->chunk);

	context->nr_chunks++;

	set_chunk_private(p, chunk);

	if (likely(cache_control_enabled())) {
		chunk->mapping_type = get_mapping_type(context);
		chunk->mr = get_predict_mr(context);

		if (chunk->mapping_type == UNKNOWN_MAPPING) {
			chunk->mapping_source = MAPPING_FROM_SAMPLING;
		} else {
			chunk->mapping_source = MAPPING_FROM_PREDICTION;
		}

#if 0
		printf("attach_chunk_to_context\n");
		printf("context %lu chunk %lu size %d source %d type %d mr %.4f\n", 
			context->idx, chunk->idx, chunk->size,
			chunk->mapping_source, chunk->mapping_type,
			chunk->mr);

		printf("sample_skip %d\n", context->sample_skip);
#endif
		if (context->sample_skip == 0
				|| context->last_chunk_sz != size) {
			monit_chunk(chunk);
			context->sample_skip = context->skip_interval;
			chunk->under_sampling |= CHUNK_UNDER_SAMPLE;
			if (context->last_chunk_sz != size) {
				context->skip_interval = 0;
				context->predict_type[0] = UNKNOWN_MAPPING;
				context->predict_mr[0] = -1;
				chunk->under_sampling |= STANDALONE_CHUNK_SAMPLE;
			}
		} else {
			context->sample_skip--;
			chunk->under_sampling = 0;
		}
	}

	context->last_chunk_sz = size;
}

static unsigned long n_detach = 0;

static inline void
detach_chunk_from_context(void *p)
{
	struct memory_chunk *chunk;

	n_detach++;

	chunk = (struct memory_chunk *)get_chunk_private(p);

	if (chunk != NULL) {
		if (chunk_under_monit(chunk)) {
			stop_monit_chunk(chunk);
		}
#ifdef PCMALLOC_DEBUG
		print_chunk(chunk);
#endif /* PCMALLOC_DEBUG */
		chunk->context->nr_freed++;
		chunk_free(chunk);
		list_del(&chunk->sibling);
	}
}

void
update_context_mapping_type(struct memory_chunk *chunk)
{
	struct alloc_context *context;
	float mr;
	int mapping;

	if (likely(!chunk->under_sampling))
		return;

	context = chunk->context;
	mr = chunk->total_ref == 0 ? 0.0 
		: (float)chunk->llc_pollutor_ref / (float)chunk->total_ref;
	if (mr >= POLLUTOR_THRESHOLD) {
		mapping = RESTRICT_MAPPING;
		context->nr_pollutor++;
	} else {
		mapping = OPEN_MAPPING;
		context->nr_victim++;
	}
	/*
	chunk->mapping_type = mapping;
	chunk->mr = mr;
	*/

	context->predict_type[0] = context->predict_type[1];
	context->predict_type[1] = mapping;
	context->predict_mr[0] = context->predict_mr[1];
	context->predict_mr[1] = mr;

	if (context->predict_type[0] != context->predict_type[1]
			&& context->predict_type[0] != UNKNOWN_MAPPING)
		context->skip_interval = 0;
	else
		context->skip_interval++;

#if 0
	printf("update_context_mapping_type %d\n", context->skip_interval);
	printf("context %lu chunk %lu type %d %d mr %.4f %.4f\n", 
		context->idx, chunk->idx, 
		context->predict_type[0], context->predict_type[1],
		context->predict_mr[0], context->predict_mr[1]);
#endif
}

void
locality_profile_destroy()
{
	struct alloc_context *context;
	struct memory_chunk *chunk;
	
	list_for_each_entry (context, &locality_profile.context, sibling) {
		list_for_each_entry (chunk, &context->chunk, sibling) {
			n_detach++;
			stop_monit_chunk(chunk);
#ifdef PCMALLOC_DEBUG
			print_chunk(chunk);
#endif /* PCMALLOC_DEBUG */
		}
	}
}

void malloc_destroy();

struct timeval start, end;

int
malloc_init()
{
	int ret;

	inc_malloc_depth();

	ret = 0;
	if (pc_malloc_active())
		goto out;
	set_pc_malloc_active();

	ret = pc_malloc_init();
	if (!!ret) goto out;

	ret = hash_map_64_init();
	if (!!ret) goto out;

	ret = install_stdlibapi_hook();
	if (!!ret) goto out;

	ret = chunk_monitor_init();
	if (!!ret) goto out;

	locality_profile.context_hash_map
			=  new_hash_map_64();
	list_init(&locality_profile.context);
	locality_profile.context_idx = 0;
	locality_profile.chunk_idx = 0;

	list_init(&free_context);
	list_init(&active_context);

	list_init(&free_chunk);
	list_init(&active_chunk);

	gettimeofday(&start, NULL);

out:
	dec_malloc_depth();
	return ret;
}

void
malloc_destroy()
{
#ifdef PCMALLOC_DEBUG
	FILE *f;
	unsigned long usec;
#endif /* PCMALLOC_DEBUG */

	if (pc_malloc_state == PC_MALLOC_UNINIT)
		return;

	inc_malloc_depth();

	gettimeofday(&end, NULL);

#ifdef PCMALLOC_DEBUG
#if 0
	usec = (end.tv_sec - start.tv_sec) * 1000000
		+ end.tv_usec - start.tv_usec;
	f = fopen(get_out_path(), "a+");
	fprintf(f, "\n\n");
	fprintf(f, "execution time:\n");
	fprintf(f, "%lu.%lu\n", usec / 1000000, usec % 1000000);
	fclose(f);
#endif
#endif /* PCMALLOC_DEBUG */

	hash_map_64_destroy();

	locality_profile_destroy();

	chunk_monitor_destroy();
//	pc_malloc_destroy();

	printf("n_detach %lu\n", n_detach);
	pc_malloc_state = PC_MALLOC_UNINIT;

	dec_malloc_depth();
}

/* If pc_malloc_enable() is not called,
 * then pc_malloc is a traditional allocator,
 * and no cache control will be performed.*/ 
void
pc_malloc_enable()
{
	struct memory_chunk *chunk;

	inc_malloc_depth();

	list_for_each_entry (chunk, &active_chunk, p) {
		monit_chunk(chunk);
	}

	enable_cache_control();

	atexit(malloc_destroy);

	dec_malloc_depth();
}

void *
malloc(size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	if (likely(size < SMALL_MEMORY_CHUNK
			|| inner_malloc())) {
		p = pc_malloc(OPEN_MAPPING, size);	
		goto done;
	}

	inc_malloc_depth();

	context = get_alloc_context();
	type = get_mapping_type(context);

	p = pc_malloc(type, size);	

	attach_chunk_to_context(p, size, context);

	dec_malloc_depth();

done:
	return p;
}

void *
calloc(size_t nmemb, size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	if (likely(nmemb * size < SMALL_MEMORY_CHUNK
			|| inner_malloc())) {
		p = pc_calloc(OPEN_MAPPING, nmemb, size);	
		goto done;
	}
	
	inc_malloc_depth();

	context = get_alloc_context();
	type = get_mapping_type(context);

	p = pc_calloc(type, nmemb, size);	

	attach_chunk_to_context(p, nmemb * size, context);

	dec_malloc_depth();

done:
	return p;
}

void
free(void *p)
{
	if (unlikely(p == NULL))
		return;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	detach_chunk_from_context(p);

	pc_free(p);
}

void *
realloc(void *old, size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();
	
	if (unlikely(size == 0)) {
		if (old != NULL)
			free(old);
		p = NULL;
		goto done;
	}

#if 0
	if (unlikely(old != NULL && size <= get_chunk_size(old))) {
		p = old;
		goto done;
	}
#endif

	if (likely(size < SMALL_MEMORY_CHUNK 
			|| inner_malloc())) {
		if (old == NULL)
			p = pc_malloc(OPEN_MAPPING, size);	
		else
			p = pc_realloc(OPEN_MAPPING, old, size);	
		goto done;
	}

	inc_malloc_depth();

	context = get_alloc_context();
	type = get_mapping_type(context);

	if (old == NULL) {
		p = pc_malloc(type, size);	
	} else {
		detach_chunk_from_context(old);
		p = pc_realloc(type, old, size);	
	}

	attach_chunk_to_context(p, size, context);

	dec_malloc_depth();

done:
	return p;
}


int
cache_size_under_restrict_mapping() 
{
	return CACHE_REGION_SZ;
}

int
cache_size_under_open_mapping() 
{
	return SHARED_CACHE_SZ;
}

int
cache_line_under_restrict_mapping()
{
	return CACHE_REGION_SZ_IN_LINE;	
}

int
cache_line_under_open_mapping()
{
	return SHARED_CACHE_SZ_IN_LINE;	
}



