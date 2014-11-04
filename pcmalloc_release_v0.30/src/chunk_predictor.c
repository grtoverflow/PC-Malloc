#include <stdio.h>

#include "config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "pc_malloc.h"
#include "allocator.h"
#include "locality_profile.h"
#include "chunk_monitor.h"



static inline void *
small_chunk_alignment(void *p, int action, int type)
{
	switch (action) {
	case ACT_MALLOC:
		pc_free(p);
		p = pc_malloc(type, SMALL_MEMORY_CHUNK);
		break;
	case ACT_CALLOC:
		pc_free(p);
		p = pc_calloc(type, SMALL_MEMORY_CHUNK, 1);
		break;
	case ACT_REALLOC:
		p = pc_realloc(type, p, SMALL_MEMORY_CHUNK);
		break;
	default:
#ifdef USE_ASSERT
		assert(0);
#endif /* USE_ASSERT */
		break;
	}

	return p;
}


static inline float
get_predict_mr(struct alloc_context *context)
{
	if (context->predict_mr[0] == -1)
		return context->predict_mr[1];

	return context->predict_mr[0] < context->predict_mr[1]
		? context->predict_mr[0] : context->predict_mr[1];
}


void *
chunk_level_sampling(void *p, const size_t size, const int type, 
		const int action, struct alloc_context *context)
{
	struct memory_chunk *chunk;

	/* chunk level skip */
	if (likely(context->sample_skip > 0 
			&& context->last_chunk_sz == size
			&& size < LARGE_MEMORY_CHUNK)) {
		context->sample_skip--;
		set_chunk_private(p, NULL);
#ifdef PREDICTOR_INFO
		printf("skipping a chunk, size=%d context=%lu\n",
		       (int)size, context->idx);
#endif /* PREDICTOR_INFO */
		return p;
	}

	/* page alignment for small chunks */
	if (size < SMALL_MEMORY_CHUNK) {
		p = small_chunk_alignment(p, action, type);
	}

#ifdef PREDICTOR_INFO
	printf("monitoring a chunk, size=%d context=%lu\n",
	       (int)size, context->idx);
#endif /* PREDICTOR_INFO */

	chunk = attach_chunk_to_context(p, size, context);
	chunk->mapping_type = type;
	chunk->mr = get_predict_mr(context);
	set_chunk_private(p, chunk);

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

	return p;
}


void
collect_chunk_level_sample(void *p)
{
	struct memory_chunk *chunk;

	chunk = (struct memory_chunk *)get_chunk_private(p);

	if (chunk != NULL) {
		if (chunk_under_monit(chunk)) {
			stop_monit_chunk(chunk);
		}
		detach_chunk_from_context(chunk);
	}
}


int
get_mapping_type(struct alloc_context *context)
{
	int mapping;

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
	int s2c_mapping;

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
		s2c_mapping = get_s2c_mapping_type(chunk->size);
		if (s2c_mapping == UNKNOWN_MAPPING) {
			set_s2c_mapping_type(chunk->size, mapping);
		} else if (s2c_mapping != mapping) {
			invalidate_s2c_entry(chunk->size);
		}
	}

	return mapping;
}











