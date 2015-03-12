#ifndef UTL_WRAPPER_H_
#define UTL_WRAPPER_H_

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "allocator.h"
#include "config.h"

#define utl_calloc(n, size) internal_calloc(OPEN_MAPPING, n, size)
#define utl_malloc(size) internal_malloc(OPEN_MAPPING, size)
#define utl_free(ptr) internal_free(ptr)

#define utl_memset(ptr, c, n) memset(ptr, c, n)


#define UTL_PROCESS_SHARED PTHREAD_PROCESS_SHARED 
#define UTL_PROCESS_PRIVATE PTHREAD_PROCESS_PRIVATE 

typedef pthread_spinlock_t utl_spinlock_t;
#define utl_spinlock_init(lock, flag) \
pthread_spin_init(lock, flag)
#define utl_spinlock_destroy(lock) \
pthread_spin_destroy(lock)
#define utl_spin_lock(lock) \
pthread_spin_lock(lock)
#define utl_spin_trylock(lock) \
pthread_spin_trylock(lock)
#define utl_spin_unlock(lock) \
pthread_spin_unlock(lock)

#endif /* UTL_WRAPPER_H_ */
