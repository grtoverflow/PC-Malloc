#ifndef PC_MALLOC_H_
#define PC_MALLOC_H_

#include <stddef.h>

#include "config.h"




#define ACT_MALLOC	1
#define ACT_CALLOC	2
#define ACT_REALLOC	3
#define ACT_FREE	4




int malloc_init();
void malloc_destroy();
void pc_malloc_enable();

/* cache size assigned under different memory mappings */
int cache_size_under_restrict_mapping(); 
int cache_size_under_open_mapping(); 
int cache_line_under_restrict_mapping();
int cache_line_under_open_mapping();

/* interfaces for explicit llc control */
extern void* pc_malloc(int type, size_t sz);
extern void* pc_realloc(int type, void *p, size_t newsize);
extern void* pc_calloc(int type, size_t nmemb, size_t sz);
extern void pc_free(void *p);

/* interfaces for automatic llc control */
void* malloc(size_t sz);
void* realloc(void *p, size_t newsize);
void* calloc(size_t nmemb, size_t sz);
void free(void *p);



/* used by pc_malloc components*/
void force_inc_malloc_depth();
void force_dec_malloc_depth();

#define DEACT_CACHE_CONTROL() force_inc_malloc_depth()
#define ACT_CACHE_CONTROL() force_dec_malloc_depth()



#endif /* PC_MALLOC_H_ */
