#ifndef CHUNK_PREDICTOR_H_
#define CHUNK_PREDICTOR_H_


#include "locality_profile.h"



int    NightWatch_heap_type_hint(void *alloc_context);
size_t NightWatch_size_demand(size_t size, void *context);
void   NightWatch_sampling(void *p, size_t size, 
                           int type, void *context);

void collect_chunk_level_sample(void *p);
int update_context_mapping_type(struct memory_chunk *chunk);
	


#endif /* CHUNK_PREDICTOR_H_ */


