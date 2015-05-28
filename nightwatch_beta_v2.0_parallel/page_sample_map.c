#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "utl_event_queue.h"
#include "utl_wrapper.h"
#include "nightwatch.h"
#include "chunk_monitor.h"
#include "page_sample_map.h"


struct page_block {
	unsigned long addr;
	struct page_sample *page_sample[PAGE_BLOCK_SIZE];
	struct list_head p;
};

struct page_map {
	struct list_head page_block_list;
	struct hash_map *page_block_map;
} page_map;

static __thread struct page_block *hot_page_block
	ATTR_INITIAL_EXEC = NULL;

static __thread struct list_head free_page_sample
	ATTR_INITIAL_EXEC
	= {NULL, NULL};
static utl_spinlock_t lock;

static inline struct page_block *
get_page_block(unsigned long addr_blk_align)
{
	struct page_block *block;

	if (hot_page_block != NULL && 
			hot_page_block->addr == addr_blk_align) {
		block = hot_page_block;
		goto done;
	}

	block = (struct page_block *)
		hash_map_find_member(page_map.page_block_map, addr_blk_align);

	hot_page_block = block;

done:
	return block;
}

static inline struct page_block *
init_page_block(unsigned long addr)
{
	unsigned long addr_blk_align;
	struct page_block *block;
		
	addr_blk_align = addr & PAGE_BLOCK_MASK;

	block = (struct page_block *)
		internal_malloc(RESTRICT_MAPPING, sizeof(struct page_block));
	memset(block, 0, sizeof(struct page_block));
	block->addr = addr_blk_align;

	hash_map_add_member(page_map.page_block_map, addr_blk_align, block);
	utl_spin_lock(&lock);	
	list_add(&block->p, &page_map.page_block_list);
	utl_spin_unlock(&lock);	
	hot_page_block = block;

	return block;
}

static inline void
page_sample_free(struct page_sample *page_sample)
{
	list_add(&page_sample->p, &free_page_sample);
}

static inline struct page_sample *
page_sample_alloc(unsigned long addr)
{
	struct page_sample *page_sample;

	if (unlikely(list_need_init(&free_page_sample))) {
		list_init(&free_page_sample); 
	}
	if (unlikely(list_empty(&free_page_sample))) {
		page_sample = (struct page_sample*)
			internal_malloc(OPEN_MAPPING, sizeof(struct page_sample));
	} else {
		page_sample = next_entry(&free_page_sample, struct page_sample, p);	
		list_del(&page_sample->p);
	}

	return page_sample;
}

struct page_sample *
attach_page_sample(unsigned long addr)
{
	unsigned long addr_blk_align;
	struct page_block *block;
	unsigned long sample_idx;
	struct page_sample *page_sample;

	addr_blk_align = addr & PAGE_BLOCK_MASK;

	block = get_page_block(addr_blk_align);
	if (block == NULL) {
		block = init_page_block(addr);
	}

	sample_idx = (addr & ~PAGE_BLOCK_MASK) >> PAGE_SHIFT;

	page_sample = block->page_sample[sample_idx];
	if (page_sample != NULL) {
		page_sample_free(page_sample);
	}

	page_sample = page_sample_alloc(addr);
	block->page_sample[sample_idx] = page_sample;

	return page_sample;
}

void
detach_page_sample(struct page_sample *page_sample)
{
	unsigned long addr;
	unsigned long addr_blk_align;
	struct page_block *block;
	unsigned long sample_idx;

	addr = page_sample->addr;
	addr_blk_align = addr & PAGE_BLOCK_MASK;

	block = get_page_block(addr_blk_align);
	if (block == NULL)
		return;

	sample_idx = (addr & ~PAGE_BLOCK_MASK) >> PAGE_SHIFT;

	page_sample = block->page_sample[sample_idx];
	if (page_sample != NULL) {
		page_sample_free(page_sample);
	}

	block->page_sample[sample_idx] = NULL;
}

struct page_sample *
get_page_sample(unsigned long addr)
{
	unsigned long addr_blk_align;
	struct page_block *block;
	unsigned long sample_idx;

	addr_blk_align = addr & PAGE_BLOCK_MASK;
	sample_idx = (addr & ~PAGE_BLOCK_MASK) >> PAGE_SHIFT;

	block = get_page_block(addr_blk_align);
	if (block == NULL)
		return NULL;

	return block->page_sample[sample_idx];
}

int 
page_sample_map_init()
{
	list_init(&free_page_sample);

	list_init(&page_map.page_block_list);
	page_map.page_block_map = new_hash_map();
	utl_spinlock_init(&lock, UTL_PROCESS_SHARED);	

	return 0;
}

void
page_sample_map_destroy()
{
	struct page_block *block;
	struct page_sample *page_sample;

	delete_hash_map(page_map.page_block_map);

	while (!list_empty(&page_map.page_block_list)) {
		block = next_entry(&page_map.page_block_list, struct page_block, p);
		list_del(&block->p);
		internal_free(block);
	}

	while (!list_empty(&free_page_sample)) {
		page_sample = next_entry(&free_page_sample, struct page_sample, p);
		list_del(&page_sample->p);
		internal_free(page_sample);
	}
	utl_spinlock_destroy(&lock);	
}



