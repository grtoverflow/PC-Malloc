#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "nightwatch.h"
#include "allocator.h"
#include "locality_profile.h"
#include "chunk_monitor.h"



static inline float
get_predict_mr(struct alloc_context *context)
{
	if (context->predict_mr[0] == -1)
		return context->predict_mr[1];

	return context->predict_mr[0] < context->predict_mr[1]
		? context->predict_mr[0] : context->predict_mr[1];
}


size_t
NightWatch_size_demand(size_t size, void *alloc_context)
{
	struct alloc_context *context;

	if (unlikely(!alloc_context))
		return size;

	context = (struct alloc_context*)alloc_context;
	if (likely(context->map_only_context_flag == MAP_ONLY_CONTEXT)) {
		return size;
	}
	/* page alignment for small sampled chunks */
#if 0
	if (unlikely(!(context->sample_skip > 0 
			&& context->last_chunk_sz == size)
			&& size < SMALL_MEMORY_CHUNK)) {
#endif
	if (unlikely(context->sample_skip == 0 
			&& size < SMALL_MEMORY_CHUNK)) {
		return SMALL_MEMORY_CHUNK;
	} else {
		return size;
	}
}

void
NightWatch_sampling(void *p, size_t size, int type, 
                    void *alloc_context)
{
	struct alloc_context *context;
	struct memory_chunk *chunk;

	context = (struct alloc_context*)alloc_context;
	if (likely(context->map_only_context_flag == MAP_ONLY_CONTEXT)) {
		return;
	}
	/* chunk level skip */
	if (likely(context->sample_skip > 0 
			&& ((size < SMALL_MEMORY_CHUNK)
			|| (context->last_chunk_sz == size
			&& size < LARGE_MEMORY_CHUNK)))) {
		context->sample_skip--;
		set_extend_info(p, NULL);
#ifdef PREDICTOR_INFO
		printf("skipping a chunk, size=%d context=%lu\n",
		       (int)size, context->idx);
#endif /* PREDICTOR_INFO */
		return;
	}

//	assert(size >= SMALL_MEMORY_CHUNK);
#ifdef PREDICTOR_INFO
	printf("monitoring a chunk, size=%d context=%lu\n",
	       (int)size, context->idx);
#endif /* PREDICTOR_INFO */

	chunk = attach_chunk_to_context(p, size, context);
	chunk->mapping_type = type;
	chunk->mr = get_predict_mr(context);
	set_extend_info(p, chunk);

	if (context->last_chunk_sz != size) {
		context->skip_interval = 0;
		context->predict_type[0] = UNKNOWN_MAPPING;
		context->predict_mr[0] = -1;
	}

	if (unlikely(context->sample_skip == 0
			&& size < LARGE_MEMORY_CHUNK)) {
		context->sample_skip = context->skip_interval;
	}

	context->last_chunk_sz = size;
	monit_chunk(chunk);
}


void
NightWatch_collect_sample(void *p)
{
	struct memory_chunk *chunk;

	chunk = (struct memory_chunk *)get_extend_info(p);

	if (chunk != NULL) {
		if (chunk_under_monit(chunk)) {
			stop_monit_chunk(chunk);
		}
		detach_chunk_from_context(chunk);
	}
}


int
NightWatch_heap_type_hint(void *alloc_context)
{
	int mapping;
	struct alloc_context *context;

	context = (struct alloc_context*)alloc_context;
	if (likely(context->map_only_context_flag == MAP_ONLY_CONTEXT))
		return context->map_type;

	if (context->predict_type[0] != RESTRICT_MAPPING
			|| context->predict_type[1] != RESTRICT_MAPPING) {
		mapping = OPEN_MAPPING;
	} else{
		mapping = RESTRICT_MAPPING;	
	}

#ifdef PREDICTOR_INFO
	printf("mapping prediction, mapping=%s context=%lu\n",
	       mapping == OPEN_MAPPING ? "OPEN_MAPPING" : "RESTRICT_MAPPING", 
	       context->idx);
#endif /* PREDICTOR_INFO */

	return mapping;
}


int
update_context_mapping_type(struct memory_chunk *chunk)
{
	struct alloc_context *context;
	float mr;
	int mapping;

	context = chunk->context;
	mr = chunk->total_ref == 0 ? 0.0 
		: (float)chunk->llc_pollutor_ref / (float)chunk->total_ref;
	if (mr >= POLLUTOR_THRESHOLD) {
		mapping = RESTRICT_MAPPING;
	} else {
		mapping = OPEN_MAPPING;
	}

	context->predict_type[0] = context->predict_type[1];
	context->predict_type[1] = mapping;
	context->predict_mr[0] = context->predict_mr[1];
	context->predict_mr[1] = mr;

#ifdef PREDICTOR_INFO
	printf("monitoring result, mapping=%s target_mapping=%s, chunk=%lu context=%lu\n",
	       chunk->mapping_type == OPEN_MAPPING ? "OPEN_MAPPING" : "RESTRICT_MAPPING", 
	       mapping == OPEN_MAPPING ? "OPEN_MAPPING" : "RESTRICT_MAPPING", 
	       chunk->idx, context->idx);
#endif /* PREDICTOR_INFO */

	/* update chunk level skip */
	if (chunk->size < LARGE_MEMORY_CHUNK) {
		if (mapping != chunk->mapping_type) {
			context->skip_interval = 0;
		} else {
			context->skip_interval = 
			    context->skip_interval == 0 ? 1
			    : context->skip_interval << 1;
		}

		context->sample_skip = context->skip_interval;
	}

	/* update size2context table */
	if (chunk->size <= S2C_MAP_SIZE) {
		update_s2c_map(chunk->size, mapping);
	}

	return mapping;
}











