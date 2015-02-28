#ifndef NIGHTWATCH_H_
#define NIGHTWATCH_H_

#include <stddef.h>

#include "config.h"
#include "allocator.h"



/* cache size assigned under different memory mappings */
int cache_size_under_restrict_mapping(); 
int cache_size_under_open_mapping(); 
int cache_line_under_restrict_mapping();
int cache_line_under_open_mapping();


extern int    NightWatch_init();
extern void   NightWatch_destroy();
extern void*  NightWatch_get_alloc_context(size_t size);
extern int    NightWatch_heap_type_hint(void *alloc_context);
extern size_t NightWatch_size_demand(size_t size, void *context);
extern void   NightWatch_sampling(void *p, size_t size, 
                           int type, void *context);
int NightWatch_active();



#endif /* NIGHTWATCH_H_ */
