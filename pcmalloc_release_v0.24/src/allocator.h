#ifndef ALLOCATOR_H_
#define ALLOCATOR_H_

#include <stdlib.h>

#include "config.h"




int pc_malloc_init();

void* pc_malloc(int type, size_t sz);
void* pc_realloc(int type, void *p, size_t newsize);
void* pc_calloc(int type, size_t nmemb, size_t sz);
void pc_free(void *p);
void switch_mapping(void *p, int target_mapping);

void set_chunk_private(void *p, void *private);
void* get_chunk_private(void *p);
size_t get_chunk_size(void *p);


#define boot_alloc_mark		0xaaaaaaaaU

#define tst_boot_alloc_chunk(p)		\
(((unsigned int*)p)[-2] == boot_alloc_mark)



#endif /* ALLOCTOR_H_ */


