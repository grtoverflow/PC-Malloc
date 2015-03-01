#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "context_key.h"
#include "allocator.h"
#include "chunk_monitor.h"
#include "locality_profile.h"
#include "pc_malloc.h"



struct size2context_entry {
	int check_interval;
	int to_check;
	int mapping_type;
	int invalid;
	struct list_head context_set;
};


struct locality_profile {
	unsigned long chunk_idx;

	struct hash_map *context_hash_map;
	struct size2context_entry *s2c_map;
	
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
	list_del(&context->chunk);
	list_del(&context->s2c_set);
	list_add(&context->p, &free_context);
}

static inline struct alloc_context *
context_alloc()
{
	struct alloc_context *context;

	if (unlikely(list_empty(&free_context))) {
		context = (struct alloc_context*)
			internal_malloc(OPEN_MAPPING, sizeof(struct alloc_context));
#ifdef USE_ASSERT
		assert(!!context);
#endif /* USE_ASSERT */
	} else {
		context = next_entry(&free_context, struct alloc_context, p);	
		list_del(&context->p);
	}

	list_add(&context->p, &active_context);
	list_init(&context->chunk);
	list_init(&context->sibling);
	list_init(&context->s2c_set);

	return context;
}


static inline void
chunk_free(struct memory_chunk *chunk)
{
	list_del(&chunk->p);
	list_del(&chunk->sibling);
	list_add(&chunk->p, &free_chunk);
}

static inline struct memory_chunk *
chunk_alloc()
{
	struct memory_chunk *chunk;

	if (unlikely(list_empty(&free_chunk))) {
		chunk = (struct memory_chunk*)
			internal_malloc(OPEN_MAPPING, sizeof(struct memory_chunk));
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

	list_add(&chunk->p, &active_chunk);
	list_init(&chunk->sample);
	list_init(&chunk->sibling);

	return chunk;
}


#ifdef AT_HOME
void
print_chunk(struct memory_chunk *chunk)
{
	disable_cache_management();
	printf("\n");
	printf("chunk info");
	printf("idx:   %lu\n", chunk->idx);
	printf("size:  %d\n", (int)chunk->size);
	printf("mapping_type:    %d\n", chunk->mapping_type);
	printf("llc_mr:  %.4f\n", chunk->mr);
	printf("victim_sample:   %u\n", chunk->llc_victim_ref);
	printf("pollutor_sample: %u\n", chunk->llc_pollutor_ref);
	printf("total_sample:    %u\n", chunk->total_ref);
	printf("sample_page:     %d\n", chunk->nr_sample);
	printf("sample_cycle:    %d\n", chunk->sample_cycle);
	printf("\n");
	enable_cache_management();
}
#endif /* AT_HOME */


int 
locality_profile_init()
{
	int i;

	locality_profile.context_hash_map
		=  new_hash_map();
	locality_profile.s2c_map = (struct size2context_entry *)
		internal_calloc(RESTRICT_MAPPING, S2C_MAP_SIZE + 1, 
			sizeof(struct size2context_entry));
	for (i = 0; i < S2C_MAP_SIZE + 1; i++) {
		list_init(&locality_profile.s2c_map[i].context_set);
	}

	list_init(&locality_profile.context);
	locality_profile.context_idx = 0;
	locality_profile.chunk_idx = 0;

	list_init(&free_context);
	list_init(&active_context);

	list_init(&free_chunk);
	list_init(&active_chunk);
	
	return 0;
}


void
locality_profile_destroy()
{
	struct alloc_context *context;
	struct memory_chunk *chunk;
	
	list_for_each_entry (context, &locality_profile.context, sibling) {
		list_for_each_entry (chunk, &context->chunk, sibling) {
			stop_monit_chunk(chunk);
#ifdef AT_HOME
			print_chunk(chunk);
#endif /* AT_HOME */
		}
	}

	delete_hash_map(locality_profile.context_hash_map);
	internal_free(locality_profile.s2c_map);
}


void
invalidate_s2c_entry(int size)
{
	struct size2context_entry *sz2ctx_entry;

	if (unlikely(size > S2C_MAP_SIZE))
		return;

	sz2ctx_entry = &locality_profile.s2c_map[size];
	sz2ctx_entry->invalid = 1;
}


int
get_s2c_mapping_type(int size)
{
	struct size2context_entry *sz2ctx_entry;

	if (unlikely(size > S2C_MAP_SIZE))
		return UNKNOWN_MAPPING;

	sz2ctx_entry = &locality_profile.s2c_map[size];
	return sz2ctx_entry->mapping_type;
}


void
set_s2c_mapping_type(int size, int mapping_type)
{
	struct size2context_entry *sz2ctx_entry;

	if (unlikely(size > S2C_MAP_SIZE))
		return;

	sz2ctx_entry = &locality_profile.s2c_map[size];
	sz2ctx_entry->mapping_type = mapping_type;
}


static inline int
in_context_set(struct alloc_context *context,
               struct size2context_entry *sz2ctx_entry)
{
	struct alloc_context *ctx_iter;

	list_for_each_entry(ctx_iter, &sz2ctx_entry->context_set, s2c_set) {
		if (ctx_iter->context_key == context->context_key)
			return 1;
	}
	
	return 0;
}

static inline void
update_s2c_map(struct alloc_context *context,
               struct size2context_entry *sz2ctx_entry)
{
	if(list_empty(&sz2ctx_entry->context_set)) {
		list_add(&context->s2c_set, &sz2ctx_entry->context_set);
		sz2ctx_entry->check_interval = 1;
		sz2ctx_entry->to_check = 1;
	} else if (sz2ctx_entry->to_check == 0) {
		if (in_context_set(context, sz2ctx_entry)) {
		    if (sz2ctx_entry->check_interval < S2C_MAP_UPDATE_INTERVAL_MAX) {
		        sz2ctx_entry->check_interval 
		            += random() % (sz2ctx_entry->check_interval << 1);
		    } else {
		        sz2ctx_entry->check_interval 
		            = (S2C_MAP_UPDATE_INTERVAL_MAX << 1) 
		            + (random() % S2C_MAP_UPDATE_INTERVAL_MAX);
		    }
		    sz2ctx_entry->to_check
		        = sz2ctx_entry->check_interval;
		} else {
		    /* new context detected */
		    list_add(&context->s2c_set, &sz2ctx_entry->context_set);
		    sz2ctx_entry->check_interval = 1;
		    sz2ctx_entry->to_check = 1;
		}
	}
}


static inline void
init_context(uint64_t key, struct alloc_context *context)
{
	context->context_key = key;
	context->predict_type[0] = context->predict_type[1] = UNKNOWN_MAPPING;
	context->predict_mr[0] = context->predict_mr[1] = -1;
	context->sample_skip = 0;
	context->skip_interval = 0;
	context->idx = locality_profile.context_idx++;
	hash_map_add_member(locality_profile.context_hash_map, key, context);
	list_add(&context->sibling, &locality_profile.context);
}


void * 
NightWatch_get_alloc_context(size_t size) 
{
	struct alloc_context *context;
	uint64_t key;
	struct size2context_entry *sz2ctx_entry;

	/* quick path */
	sz2ctx_entry = NULL;
	if (unlikely(size > S2C_MAP_SIZE)) {
		goto general_path;
	}

	sz2ctx_entry = &locality_profile.s2c_map[size];

	if (unlikely(list_empty(&sz2ctx_entry->context_set) 
			|| sz2ctx_entry->invalid)) {
		goto general_path;
	}

	if (sz2ctx_entry->to_check > 0) {
		context = list_entry(sz2ctx_entry->context_set.next, 
		                     struct alloc_context, s2c_set);
		sz2ctx_entry->to_check--;
		return context;
	}

general_path:
	key = get_context_key();

	context = (struct alloc_context *) 
		hash_map_find_member(locality_profile.context_hash_map, key);
	if (likely(context != NULL))
		goto done;
	
	/* new context detected */
	context = context_alloc();	
	init_context(key, context);

done:
	if (likely(size <= S2C_MAP_SIZE && !sz2ctx_entry->invalid))
		update_s2c_map(context, sz2ctx_entry);

	return context;
}


struct memory_chunk *
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

	return chunk;
}


void
detach_chunk_from_context(struct memory_chunk *chunk)
{
#ifdef AT_HOME
	print_chunk(chunk);
#endif /* AT_HOME */
	chunk->context = NULL;
	chunk_free(chunk);
}


void
process_active_chunk(process_func func)
{
	struct memory_chunk *chunk;

	list_for_each_entry (chunk, &active_chunk, p) {
		func(chunk);
	}
}

