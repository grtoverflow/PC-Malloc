#ifndef ALLOCATOR_H_
#define ALLOCATOR_H_


#include <stdint.h>
#include <string.h>


/* In order to integrate NightWatch, the memory allocator 
 * should support the following interface. */

extern void* tc_malloc_internal(size_t size, int type);
extern void tc_free_internal(void *p);
extern void switch_heap_type(void *p, size_t size, int target_type);
extern void set_extend_info(void *p, void *extend_info);
extern void* get_extend_info(void *p);


static inline
void* internal_malloc(int type, size_t size) {
	return tc_malloc_internal(size, type);
}

static inline
void* internal_calloc(int type, size_t n, size_t elem_size) {
	size_t size = n * elem_size;
	void *p = tc_malloc_internal(size, type);
	memset(p, 0, size);
	return p;
}

static inline
void internal_free(void *p) {
	return tc_free_internal(p);
}

extern void enter_cache_management();
extern void leave_cache_management();
extern int in_cache_management();

#endif /* ALLOCATOR_H_ */



