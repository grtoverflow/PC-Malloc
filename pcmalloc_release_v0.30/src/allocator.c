#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>

#include "config.h"
#include "allocator.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_bitmap.h"



#define CHUNK_ALIGN_SZ		8
#define CHUNK_ALIGN_MASK	(~(CHUNK_ALIGN_SZ - 1))
#define chunk_align(size)	\
(((size) + CHUNK_ALIGN_SZ - 1) & CHUNK_ALIGN_MASK)

#define NR_SMALL_BIN	128
#define NR_LARGE_BIN	184
#define NR_FAST_BIN	(NR_SMALL_BIN + NR_LARGE_BIN)
#define EXLARGE_IDX	NR_FAST_BIN
#define UNCUT_IDX	(EXLARGE_IDX + 1)
#define MMAP_IDX	(UNCUT_IDX + 1)
#define NR_BIN		(MMAP_IDX + 1)

#define SMALL_CHUNK_UPPER_BOUND		1024
#define LARGE_CHUNK_UPPER_BOUND		1048576
#define EXLARGE_CHUNK_UPPER_BOUND	16777216

#define EXPAND_BATCH_SZ		8
#define UNCUTTED_CHK_SZ		EXLARGE_CHUNK_UPPER_BOUND

#define QT_ALIGN_SHIFT		17
#define QT_ALIGN		((size_t)1 << QT_ALIGN_SHIFT)
#define QT_MASK			(~(QT_ALIGN - 1))
#define EXLARGE_QT_SZ \
((EXLARGE_CHUNK_UPPER_BOUND - LARGE_CHUNK_UPPER_BOUND) >> QT_ALIGN_SHIFT)
#define	qt_idx(size) ((size) > EXLARGE_CHUNK_UPPER_BOUND ? EXLARGE_QT_SZ - 1 \
: ((size) - LARGE_CHUNK_UPPER_BOUND) >> QT_ALIGN_SHIFT)

#define small_chunk(size) \
((size) > 0 && (size) <= SMALL_CHUNK_UPPER_BOUND)

#define large_chunk(size) \
((size) > SMALL_CHUNK_UPPER_BOUND && (size) <= LARGE_CHUNK_UPPER_BOUND)

#define exlarge_chunk(size) \
((size) > LARGE_CHUNK_UPPER_BOUND && (size) <= EXLARGE_CHUNK_UPPER_BOUND)


#define NR_FAST_BIN_SET	11


/* alignment in bits */
static const int fast_bin_align[NR_FAST_BIN_SET] = {
	3, /* [0, 1KB) */
	6, /* [1KB, 2KB) */
	6, /* [2KB, 4KB) */
	9, /* [4KB, 8KB) */
	9, /* [8KB, 16KB) */
	9, /* [16KB, 32KB) */
	12, /* [32KB, 64KB) */
	12, /* [64KB, 128KB) */
	12, /* [128KB, 256KB) */
	15, /* [256KB, 512KB) */
	15, /* [512KB, 1MB) */
};

/* number of bins */
static const int fast_bin_idx[NR_FAST_BIN_SET] = {
	0, /* [0, 1KB) */
	128, /* [1KB, 2KB) */
	144, /* [2KB, 4KB) */
	176, /* [4KB, 8KB) */
	184, /* [8KB, 16KB) */
	200, /* [16KB, 32KB) */
	232, /* [32KB, 64KB) */
	240, /* [64KB, 128KB) */
	256, /* [128KB, 256KB) */
	288, /* [256KB, 512KB) */
	296, /* [512KB, 1MB) */
};


#define small_bin_idx(idx) \
((idx) >= 0 && (idx) < fast_bin_idx[1])

#define large_bin_idx(idx) \
((idx) >= fast_bin_idx[1] && (idx) < NR_FAST_BIN)

#define exlarge_bin_idx(idx) \
((idx) == EXLARGE_IDX)

#define mmap_bin_idx(idx) \
((idx) == MMAP_IDX)


struct chunk_head {
	size_t size;
	size_t alloc_size;
	void *private;
	struct list_head free_link;	
	/* only used for full chunk headers */
	struct list_head addr_link;	
	void *p[0];
};


#define CHK_HEAD_SZ (sizeof(struct chunk_head))
#define p2head(p) ((struct chunk_head *)((unsigned long)(p) - CHK_HEAD_SZ))
#define head2p(head) ((void *)((head)->p))


#define CHUNK_FLAG_MASK		(~((size_t)0x7))
#define MAPPING_MASK		(~((size_t)0x1))
#define F_OPEN_MAPPING		0x0
#define F_RESTRICT_MAPPING	0x1
#define FREE_MASK		(~((size_t)0x2))
#define F_FREE			0x0
#define F_USED			0x2

#define FREE_CHUNK		0
#define ACTIVE_CHUNK		1

#define set_chunk_type(type, head) \
((head)->alloc_size = ((head)->alloc_size & MAPPING_MASK) \
| ((type) == OPEN_MAPPING ? F_OPEN_MAPPING : F_RESTRICT_MAPPING))

#define chunk_type(head) \
(((head)->alloc_size & ~MAPPING_MASK) == F_RESTRICT_MAPPING \
? RESTRICT_MAPPING : OPEN_MAPPING)

#define set_chunk_state(state, head) \
((head)->alloc_size = ((head)->alloc_size & FREE_MASK) \
| ((state) == FREE_CHUNK ? F_FREE : F_USED))

#define chunk_state(head) \
(((head)->alloc_size & ~FREE_MASK) == F_FREE ? FREE_CHUNK : ACTIVE_CHUNK)


#define set_alloc_size(size, head) \
((head)->alloc_size = ((head)->alloc_size & ~CHUNK_FLAG_MASK) \
| ((size) & CHUNK_FLAG_MASK))

#define alloc_size(head) \
((head)->alloc_size & CHUNK_FLAG_MASK)

#define set_chunk_size(size, head) \
((head)->size = (size))

#define chunk_size(head) ((head)->size)


static struct list_head bins[NR_MAPPING][NR_BIN];
static struct chunk_head *exlarge_chk_qt[NR_MAPPING][EXLARGE_QT_SZ];
static struct bitmap *qt_bitmap[NR_MAPPING];


/* caller guarantees that size > 0 */
static inline int
size2bin(size_t size)
{
	int sf_pos;

	if (likely(small_chunk(size))) {
		return (size - 1) >> fast_bin_align[0];
	}

	if (large_chunk(size)) {
		sf_pos = bsf(size - 1);

		return fast_bin_idx[sf_pos - 9]
		       + (((size - 1) & ~((size_t)1 << sf_pos)) 
		       >> fast_bin_align[sf_pos - 9]);
	}

	if (exlarge_chunk(size)) {
		return EXLARGE_IDX;
	}

	return MMAP_IDX;
}


static inline int
bin2size(int bin_idx)
{
	int size = 0;	
	int i;

	/* small bins */
	if (small_bin_idx(bin_idx)) {
		return (1 << fast_bin_align[0]) * (bin_idx + 1);
	}

	/* bin2size is not defined for EXLARGE_BIN and MMAP_BIN */
	if (exlarge_bin_idx(bin_idx) || mmap_bin_idx(bin_idx))
		return -1;

	/* large bins */
	size = 1 << 10;
	for (i = 2; i < NR_FAST_BIN_SET; i++) {
		if (bin_idx < fast_bin_idx[i])	
			break;
		size <<= 1;
	}
	
	size += (1 << fast_bin_align[i - 1]) 
	     * (bin_idx + 1 - fast_bin_idx[i - 1]);
	
	return size;
}



int
allocator_init()
{
	int i, j;

	for (i = 0; i < NR_MAPPING; i++) {
		for (j = 0; j < NR_BIN; j++) {
			list_init(&bins[i][j]);
		}
	}

	for (i = 0; i < NR_MAPPING; i++) {
		qt_bitmap[i] = new_bitmap(EXLARGE_QT_SZ, 0);
	}

	for (i = 0; i < NR_MAPPING; i++) {
		for (j = 0; j < EXLARGE_QT_SZ; j++) {
			exlarge_chk_qt[i][j] = NULL;
		}
	}

	return 0;
}


void
allocator_destroy()
{
	struct chunk_head *mmap_head;
	struct list_head *bin;
	size_t alloc_size;
	int i;
	
	for (i = 0; i < NR_MAPPING; i++) {
		bin = &bins[i][MMAP_IDX];
		while (!list_empty(bin)) {
			mmap_head = next_entry(bin, struct chunk_head, free_link);
			alloc_size = alloc_size(mmap_head);
			list_del(&mmap_head->free_link);
			munmap(mmap_head, alloc_size);
		}
	}
}


static inline void
free_mmap_chunk(struct chunk_head *chk_head)
{
	size_t alloc_size;

	alloc_size = alloc_size(chk_head);
	list_del(&chk_head->free_link);

	munmap(chk_head, alloc_size);
}

static inline struct chunk_head *
get_mmap_chunk(int type, size_t size)
{
	struct chunk_head *mmap_head;
	struct list_head *bin;
	size_t alloc_size;

	alloc_size = UPPER_PAGE_ALIGN(size + CHK_HEAD_SZ);
	mmap_head = (struct chunk_head *)
	            mmap(NULL, alloc_size, PROT_READ|PROT_WRITE,
	                 MAP_PRIVATE|MAP_ANONYMOUS|MAP_CACHE_AWARE_STATE, 
	                 -1, type);

	set_alloc_size(alloc_size, mmap_head);

	bin = &bins[get_mapping_idx(type)][MMAP_IDX];
	list_add(&mmap_head->free_link, bin);

	return mmap_head;
}


#define fit_in_size(alloc_size, size) \
(((size) + CHK_HEAD_SZ + LARGE_CHUNK_UPPER_BOUND) > alloc_size)

#define best_fit_chunk(chk_iter, size, start) \
do {\
	list_for_each_entry(chk_iter, start->free_link.prev, free_link) { \
		if (unlikely(alloc_size(chk_iter) < size - QT_ALIGN)) { \
			chk_iter = NULL; \
			break; \
		} else if (alloc_size(chk_iter) >= size) { \
			break; \
		} \
	} \
} while(0)


static inline void
delete_exlarge_chunk(struct chunk_head *chk_head)
{
	int qt_map_idx, mapping_idx;
	struct chunk_head *next_chk, **qt;
	struct list_head *bin;
	
	qt_map_idx = qt_idx(alloc_size(chk_head));
	mapping_idx = get_mapping_idx(chunk_type(chk_head));
	bin = &bins[mapping_idx][EXLARGE_IDX];
	qt = exlarge_chk_qt[mapping_idx];
	if (qt[qt_map_idx] == chk_head) {
		next_chk = next_entry(&chk_head->free_link, 
		                      struct chunk_head, free_link);
		if (&next_chk->free_link == bin
				|| qt_idx(alloc_size(next_chk)) != qt_map_idx) {
			qt[qt_map_idx] = NULL;
			clr_bit(qt_bitmap[mapping_idx], qt_map_idx);
		} else {
			qt[qt_map_idx] = next_chk;
		}
	}
	list_del(&chk_head->free_link);
}


static void
free_exlarge_chunk(struct chunk_head *chk_head, int combine)
{
	int qt_map_idx, mapping_idx;
	struct chunk_head *chk_iter, *neighbour, **qt;
	size_t alloc_size;
	struct list_head *bin;
	int type;

	type = chunk_type(chk_head);
	set_chunk_state(FREE_CHUNK, chk_head);
	alloc_size = alloc_size(chk_head);

	/* chunk combination */
	if (combine) {
		neighbour = prev_entry(&chk_head->addr_link,
		                       struct chunk_head, addr_link);
		if (neighbour < chk_head
				&& chunk_state(neighbour) == FREE_CHUNK
				&& chunk_type(neighbour) == type) {
			alloc_size += alloc_size(neighbour);
			set_alloc_size(alloc_size, neighbour);
			delete_exlarge_chunk(neighbour);
			list_del(&neighbour->addr_link);
			chk_head = neighbour;
		}
		neighbour = next_entry(&chk_head->addr_link,
		                       struct chunk_head, addr_link);
		if (neighbour > chk_head
				&& chunk_state(neighbour) == FREE_CHUNK
				&& chunk_type(neighbour) == type) {
			alloc_size += alloc_size(neighbour);
			set_alloc_size(alloc_size, chk_head);
			delete_exlarge_chunk(neighbour);
			list_del(&neighbour->addr_link);
		}
	}
	
	/* free chunk */
	qt_map_idx = qt_idx(alloc_size);
	mapping_idx = get_mapping_idx(type);
	bin = &bins[mapping_idx][EXLARGE_IDX];
	qt = exlarge_chk_qt[mapping_idx];
	if (qt[qt_map_idx] == NULL)	{
		qt_map_idx = upward_bit_test(qt_bitmap[mapping_idx], qt_map_idx);
		if (qt_map_idx == -1) {
			list_add(&chk_head->free_link, bin->prev);	
		} else {
			list_add(&chk_head->free_link, 
			         qt[qt_map_idx]->free_link.prev);	
		}
		qt_map_idx = qt_idx(alloc_size(chk_head));
		qt[qt_map_idx] = chk_head;
		set_bit(qt_bitmap[mapping_idx], qt_map_idx);
	} else {
		best_fit_chunk(chk_iter, alloc_size, qt[qt_map_idx]);
		if (chk_iter == NULL) {
			list_add(&chk_head->free_link, bin->prev);	
		} else {
			list_add(&chk_head->free_link, chk_iter->free_link.prev);	
		}

		if (alloc_size(qt[qt_map_idx]) >= alloc_size) {
			qt[qt_map_idx] = chk_head;
		}
	}

#if 0
int i = 0;
int last_idx = 0;;
list_for_each_entry(chk_iter, bin, free_link) {
printf("%d: qt_idx=%d bit_map_val=%d qt_head=%p chk_head=%p size=%d type=%s state=%s\n",
i, qt_idx(alloc_size(chk_iter)), bit_test(qt_bitmap, qt_idx(alloc_size(chk_iter))), 
exlarge_chk_qt[qt_idx(alloc_size(chk_iter))], chk_iter, alloc_size(chk_iter), 
chunk_type(chk_iter)==OPEN_MAPPING?"OPEN_MAPPING"
:chunk_type(chk_iter)==RESTRICT_MAPPING?"RESTRICT_MAPPING":"UNKNOWN", 
chunk_state(chk_iter)==FREE_CHUNK?"FREE_CHUNK":"ACTIVE_CHUNK");
i++;
if(qt_idx(alloc_size(chk_iter)) < last_idx)
exit(0);
last_idx = qt_idx(alloc_size(chk_iter));
}
#endif

}


static struct chunk_head*
get_exlarge_chunk(int type, size_t size)
{
	struct chunk_head *mmap_head, **qt;
	struct chunk_head *chk_iter, *chk_head, *free_back;
	size_t alloc_size;
	int qt_map_idx, mapping_idx;
	struct list_head *bin;

	size = UPPER_PAGE_ALIGN(size + CHK_HEAD_SZ);
	mapping_idx = get_mapping_idx(type);
	qt_map_idx = qt_idx(size);
	qt_map_idx = upward_bit_test(qt_bitmap[mapping_idx], qt_map_idx);
	bin = &bins[get_mapping_idx(type)][EXLARGE_IDX];
	qt = exlarge_chk_qt[mapping_idx];

	if (qt_map_idx == -1) {
		goto add;
	} else {
		best_fit_chunk(chk_iter, size, qt[qt_map_idx]);

		if (unlikely(chk_iter == NULL)) {
			goto add;
		} else {
			chk_head = chk_iter;
			goto cut;
		}
	}

add:
	mmap_head = get_mmap_chunk(type, EXLARGE_CHUNK_UPPER_BOUND);
	chk_head = (struct chunk_head *)head2p(mmap_head);
	set_alloc_size(alloc_size(mmap_head) - CHK_HEAD_SZ, chk_head);
	set_chunk_type(type, chk_head);
	set_chunk_state(FREE_CHUNK, chk_head);
	list_init(&chk_head->addr_link);

	list_add(&chk_head->free_link, bin->prev);
	if (qt[EXLARGE_QT_SZ - 1] == NULL) {
		qt[EXLARGE_QT_SZ - 1] = chk_head;
		set_bit(qt_bitmap[mapping_idx], EXLARGE_QT_SZ - 1);
	}

cut:
	delete_exlarge_chunk(chk_head);

	alloc_size = alloc_size(chk_head);
	if (fit_in_size(alloc_size, size))
		return chk_head;
	
	free_back = (struct chunk_head *)((unsigned long)chk_head + size);
	set_alloc_size(alloc_size - size, free_back);	
	set_chunk_type(type, free_back);
	set_chunk_state(FREE_CHUNK, free_back);
	set_alloc_size(size, chk_head);
	list_add(&free_back->addr_link, &chk_head->addr_link);

	free_exlarge_chunk(free_back, 0);

	return chk_head;
}


static struct chunk_head *
get_uncutted_chunk(int type, size_t size)
{
	struct list_head *bin;
	struct chunk_head *mmap_head, *chk_head;

	bin = &bins[get_mapping_idx(type)][UNCUT_IDX];
	if (unlikely(list_empty(bin))) {
		goto add;
	} else {
		list_for_each_entry(chk_head, bin, free_link) {
			if(alloc_size(chk_head) > size)
				return chk_head;
		}
	}

add:
	mmap_head = get_mmap_chunk(type, EXLARGE_CHUNK_UPPER_BOUND);
	chk_head = (struct chunk_head *)head2p(mmap_head);
	set_alloc_size(alloc_size(mmap_head) - CHK_HEAD_SZ, chk_head);

	list_add(&chk_head->free_link, bin);

	return chk_head;
}


static inline void*
get_chunk_from_uncutted(int type, size_t size)
{
	struct chunk_head *uncut, *chk_head;
	struct list_head *bin;

	chk_head = get_uncutted_chunk(type, size);
	list_del(&chk_head->free_link);
	uncut = (struct chunk_head *)((unsigned long)chk_head + size);
	set_alloc_size(alloc_size(chk_head) - size, uncut);
	set_alloc_size(size, chk_head);

	bin = &bins[get_mapping_idx(type)][UNCUT_IDX];
	list_add(&uncut->free_link, bin);

	return chk_head;
}


static int
expand_bin(int type, int bin_idx, struct list_head *bin)
{
	size_t size, alloc_size;
	struct chunk_head *chk_head;
	void *p;
	int i;

	/* expand fast bin */
	if (small_bin_idx(bin_idx)
			|| large_bin_idx(bin_idx)) {
		size = bin2size(bin_idx);		
		alloc_size 
		    = chunk_align(size + CHK_HEAD_SZ);
		p = get_chunk_from_uncutted(type, alloc_size * EXPAND_BATCH_SZ);

		for (i = 0; i < EXPAND_BATCH_SZ; i++) {
			chk_head = (struct chunk_head *)
			         ((unsigned long)p + alloc_size * i);
			set_alloc_size(alloc_size, chk_head);
			set_chunk_type(type, chk_head);
			set_chunk_state(FREE_CHUNK, chk_head);
			list_add(&chk_head->free_link, bin);
		}
	}

	return 0;
}


static inline void
free_fast_chunk(struct chunk_head *head)
{
	int bin_idx, mapping_idx;
	struct list_head *bin;
	
	bin_idx = size2bin(chunk_size(head));
	mapping_idx = get_mapping_idx(chunk_type(head));
	bin = &bins[mapping_idx][bin_idx];

	set_chunk_state(FREE_CHUNK, head);
	list_add(&head->free_link, bin);
}


static inline struct chunk_head *
get_fast_chunk(int type, size_t size)
{
	int bin_idx, mapping_idx;
	struct list_head *bin;
	struct chunk_head *head;

	bin_idx = size2bin(size);
	mapping_idx = get_mapping_idx(type);
	bin = &bins[mapping_idx][bin_idx];

	if(unlikely(list_empty(bin))) {
		expand_bin(type, bin_idx, bin);
	}
	head = list_entry(bin->next, 
	                  struct chunk_head, free_link);
	list_del(&head->free_link);

	return head;
}

void
pc_free(void *p)
{
	size_t size;
	struct chunk_head *head;

	if (unlikely(p == NULL))
		return;
	head = p2head(p);
	set_chunk_state(FREE_CHUNK, head);
	size = chunk_size(head);

	if (likely(small_chunk(size) || large_chunk(size))) {
		free_fast_chunk(head);
	} else if (exlarge_chunk(size)) {
		free_exlarge_chunk(head, 1);
	} else {
		free_mmap_chunk(head);
	}
}

void* 
pc_malloc(int type, size_t size)
{
	struct chunk_head *head;

	if (unlikely(size == 0 || !valid_mapping_type(type)))
		return NULL;

	if (small_chunk(size) || large_chunk(size)) {
		head = get_fast_chunk(type, size);
	} else if (exlarge_chunk(size)) {
		head = get_exlarge_chunk(type, size);
	} else {
		head = get_mmap_chunk(type, size);
	}

	set_chunk_state(ACTIVE_CHUNK, head);
	set_chunk_size(size, head);

	return head2p(head);
}

void* 
pc_realloc(int type, void *old, size_t newsize)
{
	void *p;

	if (unlikely(newsize == 0)) {
		pc_free(old);
		return NULL;
	}

	if (old == NULL) {
		return pc_malloc(type, newsize);
	}

	if (newsize <= chunk_size(p2head(old))) {
		return old;
	}

	p = pc_malloc(type, newsize);
	memcpy(p, old, chunk_size(p2head(old)));		
	pc_free(old);
	
	return p;
}

void*
pc_calloc(int type, size_t nmemb, size_t size)
{
	void *p;

	p = pc_malloc(type, nmemb * size);
	memset(p, 0, nmemb * size);

	return p;
}

void
switch_mapping(void *p, int target_mapping)
{
	struct chunk_head *chk_head;
	size_t sz;

	if (unlikely(p == NULL))
		return;

	chk_head = p2head(p);
	if (chunk_size(chk_head) < (PAGE_SIZE * 2)
			|| chunk_state(chk_head) != ACTIVE_CHUNK
			|| chunk_type(chk_head) == target_mapping) {
		return;
	}

	sz = LOWER_PAGE_ALIGN(((unsigned long)p) + chunk_size(chk_head)) 
	     - UPPER_PAGE_ALIGN((unsigned long)p);

	set_chunk_type(target_mapping, chk_head);
	mmap((void *)UPPER_PAGE_ALIGN((unsigned long)p), sz, PROT_READ|PROT_WRITE,
	     MAP_PRIVATE|MAP_ANONYMOUS|REMAP_CACHE_AWARE_STATE,
	     -1, target_mapping);
}

void
set_chunk_private(void *p, void *private)
{
	p2head(p)->private = private;
}

void*
get_chunk_private(void *p)
{
	return p2head(p)->private;
}

size_t
get_chunk_size(void *p)
{
	return chunk_size(p2head(p));
}














