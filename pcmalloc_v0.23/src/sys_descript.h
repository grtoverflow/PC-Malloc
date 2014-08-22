#ifndef SYS_DESCRIPT_H_
#define SYS_DESCRIPT_H_


/* arch description */
#define CACHE_LINE_SZ		64
#define CACHE_LINE_SHIFT	6
#define CACHE_LINE_MASK		(~((1UL << CACHE_LINE_SHIFT) - 1))

#define NR_CACHE_LEVEL		3
#define L1D_SZ_IN_LINE		512
#define L2_SZ_IN_LINE		4096
#define L3_SZ_IN_LINE		163840

#define PREFETCH_BATCH		1


/* os description */
#define PAGE_SIZE	4096
#define PAGE_SHIFT	12
#define PAGE_MASK	(~((1UL << PAGE_SHIFT) - 1))

#define UPPER_PAGE_ALIGN(addr)\
	((addr) & ~PAGE_MASK ? ((addr) & PAGE_MASK) + PAGE_SIZE : (addr))

#define LOWER_PAGE_ALIGN(addr)\
	((addr) & PAGE_MASK)

#endif /* SYS_DESCRIPT_H_ */
