#ifndef UTL_WRAPPER_H_
#define UTL_WRAPPER_H_

#include <stdlib.h>
#include <string.h>

#include "allocator.h"
#include "config.h"

#define wrap_calloc(n, size) internal_calloc(OPEN_MAPPING, n, size)
#define wrap_malloc(size) internal_malloc(OPEN_MAPPING, size)
#define wrap_free(ptr) internal_free(ptr)

#define wrap_memset(ptr, c, n) memset(ptr, c, n)



#endif /* UTL_WRAPPER_H_ */
