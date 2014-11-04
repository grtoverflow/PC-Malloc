#ifndef UTL_WRAPPER_H_
#define UTL_WRAPPER_H_

#include <stdlib.h>
#include <string.h>


#define wrap_calloc(n, size) calloc(n, size)
#define wrap_malloc(size) malloc(size)
#define wrap_realloc(ptr, size) realloc(ptr, size)
#define wrap_free(ptr) free(ptr)

#define wrap_memset(ptr, c, n) memset(ptr, c, n)



#endif /* UTL_WRAPPER_H_ */
