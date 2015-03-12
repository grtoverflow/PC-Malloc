#ifndef CONFIG_H_
#define CONFIG_H_



/* fuctionality configuration */
//#define AT_HOME
#ifdef AT_HOME
  #define MONITOR_INFO
  #define PREDICTOR_INFO
#endif /* AT_HOME */

//#define USE_ASSERT

//#define USE_PERF_EVENT
#define USE_PAPI

#define USE_GLIBC_BACKTRACE
//#define USE_QUICK_BACKTRACE



/* architecture */
#define X86_64



/* processor */
#define CACHE_LINE_SZ		64
#define CACHE_LINE_SHIFT	6
#define CACHE_LINE_MASK		(~((1UL << CACHE_LINE_SHIFT) - 1))

#define NR_CACHE_LEVEL		3
#define L1D_SZ_IN_LINE		512
#define L2_SZ_IN_LINE		4096
#define L3_SZ_IN_LINE		163840

#define CACHE_REGION_SZ_IN_LINE		(L3_SZ_IN_LINE >> 3)
#define SHARED_CACHE_SZ_IN_LINE		L3_SZ_IN_LINE

#define CACHE_REGION_SZ		(CACHE_REGION_SZ_IN_LINE << CACHE_LINE_SHIFT)
#define SHARED_CACHE_SZ		(SHARED_CACHE_SZ_IN_LINE << CACHE_LINE_SHIFT)

#define PREFETCH_BATCH		1



/* operating system */
#define PAGE_SIZE	4096
#define PAGE_SHIFT	12
#define PAGE_MASK	(~((1UL << PAGE_SHIFT) - 1))

#define UPPER_PAGE_ALIGN(addr)\
	(((addr) + PAGE_SIZE - 1) & PAGE_MASK)

#define LOWER_PAGE_ALIGN(addr)\
	((addr) & PAGE_MASK)

#ifndef PC_MALLOC_TYPE
#define RESTRICT_MAPPING_IDX		0
#define OPEN_MAPPING_IDX		1
#define NR_MAPPING			2
#endif /* PC_MALLOC_TYPE */

#if 0
#define get_mapping_idx(type) ((type) - 2)
#define valid_mapping_type(type) \
((type) == RESTRICT_MAPPING || (type) == OPEN_MAPPING)
#endif


/* call stack */
#define CALL_STACK_DEPTH	8
#define INNER_CALL_STACK_DEPTH	2
#define TOTAL_CALL_STACK_DEPTH	(CALL_STACK_DEPTH + INNER_CALL_STACK_DEPTH)

#define MAP_CACHE_AWARE_STATE           0x40UL
#define REMAP_CACHE_AWARE_STATE         0x80UL



/* papi */
#ifdef USE_PAPI
#define L2_MISS_EVENT_NAME "L2_LINES_IN"
#define L3_MISS_EVENT_NAME "ix86arch::LLC_MISSES"
#define L3_ACCESS_EVENT_NAME "ix86arch::LLC_REFERENCES"
#endif /* USE_PAPI */



/* pc_malloc locality profile */
#define S2C_MAP_SIZE		65536
#define S2C_MAP_UPDATE_INTERVAL_MAX	1024
#define SMALL_MEMORY_CHUNK	8192
#define LARGE_MEMORY_CHUNK	1048576

/* pc_malloc monitor */
#define POLLUTOR_THRESHOLD	0.9
#define SAMPLE_INTERVAL	5000000	/* us */
#define MONIT_CONV_ERR			0.05
#define SAMPLE_DENSITY			0.65


#define NR_CACHE_MAPPING 	2
#define RESTRICT_MAPPING	0
#define OPEN_MAPPING		1
#define UNKNOWN_MAPPING		2
#define MMAP_OPEN_MAPPING		2
#define MMAP_RESTRICT_MAPPING	3



#endif /* CONFIG_H_ */




