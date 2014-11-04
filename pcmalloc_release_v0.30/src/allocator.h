#ifndef ALLOCATOR_H_
#define ALLOCATOR_H_


#include <stdint.h>



int allocator_init();
void allocator_destroy();

void* pc_malloc(int type, size_t sz);
void* pc_realloc(int type, void *p, size_t newsize);
void* pc_calloc(int type, size_t nmemb, size_t sz);
void pc_free(void *p);
void switch_mapping(void *p, int target_mapping);

void set_chunk_private(void *p, void *private);
void* get_chunk_private(void *p);
size_t get_chunk_size(void *p);



#endif /* ALLOCATOR_H_ */



