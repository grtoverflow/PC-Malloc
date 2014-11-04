#ifndef CHUNK_PREDICTOR_H_
#define CHUNK_PREDICTOR_H_


#include "locality_profile.h"



void* chunk_level_sampling(void *p, const size_t size, const int type, 
		const int action, struct alloc_context *context);

void collect_chunk_level_sample(void *p);

int get_mapping_type(struct alloc_context *context);
int update_context_mapping_type(struct memory_chunk *chunk);
	


#endif /* CHUNK_PREDICTOR_H_ */


