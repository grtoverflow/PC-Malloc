#ifndef CACHEEX_H_
#define CACHEEX_H_


#define USE_NightWatch

#define NR_CACHE_MAPPING 	2
#define RESTRICT_MAPPING	0
#define OPEN_MAPPING		1
#define UNKNOWN_MAPPING		2
#define MMAP_OPEN_MAPPING		2
#define MMAP_RESTRICT_MAPPING	3

#define NR_HEAP_TYPE	NR_CACHE_MAPPING
#define DEFAULT_HEAP_TYPE	OPEN_MAPPING

#define MAP_CACHE_AWARE_STATE           0x40UL
#define REMAP_CACHE_AWARE_STATE         0x80UL

#define heap_type2mmap_type(type) ((type) + 2)

#define CACHE_LINE_SZ 64
#define tiny_chunk(size) ((size) <= CACHE_LINE_SZ)

#define PAGE_SIZE	4096
#define PAGE_SHIFT  12
#define PAGE_MASK   (~((1UL << PAGE_SHIFT) - 1))

#define UPPER_PAGE_ALIGN(addr)\
(((addr) + PAGE_SIZE - 1) & PAGE_MASK)

#define LOWER_PAGE_ALIGN(addr)\
((addr) & PAGE_MASK)



#endif /* CACHEEX_H_ */
