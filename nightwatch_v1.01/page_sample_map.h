#ifndef PAGE_SAMPLE_MAP_H_
#define PAGE_SAMPLE_MAP_H_


#include "config.h"


#define BLOCK_SEG_LEN		12
/* # of pages per page block */
#define PAGE_BLOCK_SIZE		(1 << BLOCK_SEG_LEN)
#define PAGE_BLOCK_SHIFT	(BLOCK_SEG_LEN + PAGE_SHIFT)
#define PAGE_BLOCK_MASK		(~((1UL << PAGE_BLOCK_SHIFT) - 1))


int page_sample_map_init();
void page_sample_map_destroy();
void pc_malloc_enable();
	
struct page_sample* attach_page_sample(unsigned long addr);
void detach_page_sample(struct page_sample *page_sample);
struct page_sample* get_page_sample(unsigned long addr);

unsigned long get_active_page_number();

#endif /* PAGE_SAMPLE_MAP_H_ */

