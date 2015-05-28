#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "utl_config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "utl_wrapper.h"



#define HT_INIT_SIZE	1024
#define N_LOCK			64
#define HT_INIT_KEY_LEN	10



struct map_member {
	uint64_t key;
	void *val;
	struct list_head crash;
	struct list_head p;
};

struct hmap_entry {
	int n;
	struct list_head entry;
};

struct hash_map {
	struct hmap_entry *map;
	utl_spinlock_t lock[N_LOCK];
	int size;
	int fill;
	int key_len;
	unsigned long key_mask;
	struct list_head members;
};

static __thread struct list_head free_member
	ATTR_INITIAL_EXEC
	= {NULL, NULL};

#define get_lock_idx(key) ((key) % N_LOCK)


#define get_key_mask(key_len) \
(~((((unsigned long)1 << key_len) - 1) << ((64 - key_len) >> 1)));

static inline unsigned long
get_map_slot(unsigned long key, unsigned long mask, int key_len)
{
	unsigned long h, slot;

	h = key + (key >> key_len) + (key >> (key_len << 1))
			+ (key >> (key_len << 2));
	h *= h;
	slot = (h & ~mask) >> ((64 - key_len) >> 1);

	return slot;
}

static inline struct map_member*
map_member_alloc()
{
	struct map_member *member;

	if (list_need_init(&free_member))
		list_init(&free_member);

	if (unlikely(list_empty(&free_member))) {
		member = (struct map_member*)
			utl_malloc(sizeof(struct map_member));	
#ifdef UTL_USE_ASSERT
		assert(member);
#endif /* UTL_USE_ASSERT */
	} else {
		member = next_entry(&free_member, struct map_member, p);	
		list_del(&member->p);
	}

	return member;
}

static inline void
map_member_free(struct map_member *member)
{
	if (list_need_init(&free_member))
		list_init(&free_member);
	list_del(&member->p);
	list_add(&member->p, &free_member);
}

struct hash_map*
new_hash_map()
{
	struct hash_map *hmap;
	int i, size;

	hmap = (struct hash_map*)utl_malloc(sizeof(struct hash_map));	
#ifdef UTL_USE_ASSERT
	assert(!!hmap);
#endif /* UTL_USE_ASSERT */
	
	size = HT_INIT_SIZE * sizeof(struct hmap_entry);
	hmap->map = (struct hmap_entry*)utl_malloc(size);
#ifdef UTL_USE_ASSERT
	assert(!!hmap->map);
#endif /* UTL_USE_ASSERT */
	memset(hmap->map, 0, size);
	
	hmap->size = HT_INIT_SIZE;
	hmap->fill = 0;
	hmap->key_len = HT_INIT_KEY_LEN;
	hmap->key_mask = get_key_mask(HT_INIT_KEY_LEN);
	list_init(&hmap->members);

	for (i = 0; i < HT_INIT_SIZE; i++) {
		list_init(&hmap->map[i].entry);	
	}
	for (i = 0; i < N_LOCK; i++) {
		utl_spinlock_init(&hmap->lock[i], UTL_PROCESS_SHARED);	
	}

	return hmap;
}

void
delete_hash_map(struct hash_map* hmap)
{
	int i;
	struct map_member *member;

	while (!list_empty(&hmap->members)) {
		member = next_entry(&hmap->members, struct map_member, p);
		map_member_free(member);
	}
	for (i = 0; i < N_LOCK; i++) {
		utl_spinlock_destroy(&hmap->lock[i]);	
	}

	utl_free(hmap->map);
	utl_free(hmap);
}

static inline struct hmap_entry*
get_map_entry(struct hash_map *hmap, unsigned long slot)
{
	return &hmap->map[slot];
}

static inline int
map_member_exist(struct hmap_entry *map_entry, uint64_t key)
{
	struct list_head *entry;
	struct map_member *member_iter;

	if (map_entry->n == 0)
		return 0;
	entry = &map_entry->entry;
	list_for_each_entry(member_iter, entry, crash) {
		if (member_iter->key == key)	
			return 1;
	}
	return 0;
}

static inline struct map_member*
get_map_member(struct hmap_entry *map_entry, uint64_t key)
{
	struct list_head *entry;
	struct map_member *member_iter;

	if (map_entry->n == 0)
		return NULL;
	entry = &map_entry->entry;
	list_for_each_entry(member_iter, entry, crash) {
		if (member_iter->key == key)	
			break;
	}
	if (&member_iter->crash == entry && member_iter->key != key)
		return NULL;
	return member_iter;
}

static inline int
hash_map_full(struct hash_map *hmap)
{
	return (hmap->size >> 1) < hmap->fill;
}

static int 
hash_map_rebuild(struct hash_map *hmap)
{
	struct hmap_entry *map, *map_entry;
	struct list_head *members;
	struct map_member *member_iter;
	int i, size;
	unsigned long slot;

	utl_spin_lock(&hmap->lock[0]);
	if (!hash_map_full(hmap)) {
		/* The hash map has been rebuilt by other thread. */
		utl_spin_unlock(&hmap->lock[0]);
		return 0;	
	}

	for (i = 1; i < N_LOCK; i++) {
		utl_spin_lock(&hmap->lock[i]);
	}
	
	members = &hmap->members;
	if (unlikely(list_empty(members)))
		goto done;

	map = hmap->map;
	hmap->size <<= 1;
	size = hmap->size * sizeof(struct hmap_entry);
	hmap->map = (struct hmap_entry*)utl_malloc(size);
	assert(!!hmap->map);
	memset(hmap->map, 0, size);

	hmap->key_len++;
	hmap->key_mask = get_key_mask(hmap->key_len);

	size = hmap->size;
	for (i = 0; i < size; i++) {
		list_init(&hmap->map[i].entry);	
	}

	list_for_each_entry(member_iter, members, p) {
		slot = get_map_slot(member_iter->key, hmap->key_mask, hmap->key_len);
		map_entry = get_map_entry(hmap, slot);	
		map_entry->n++;
		list_del(&member_iter->crash);
		list_add(&member_iter->crash, &map_entry->entry);
	}

	utl_free(map);

done:
	for (i = 0; i < N_LOCK; i++) {
		utl_spin_unlock(&hmap->lock[i]);
	}

	return 0;
}

int 
hash_map_add_member(struct hash_map *hmap, uint64_t key, void *val)
{
	struct hmap_entry *map_entry;
	struct map_member *member;
	int ret, lock_idx;
	int key_len;
	unsigned long slot;

	ret = 0;

	if (unlikely(hash_map_full(hmap))) {
		ret = hash_map_rebuild(hmap);
#ifdef UTL_USE_ASSERT
		assert(ret == 0);
#endif /* UTL_USE_ASSERT */
	}

	while(1) {
		key_len = hmap->key_len;
		slot = get_map_slot(key, hmap->key_mask, key_len);
		lock_idx = get_lock_idx(slot);
		utl_spin_lock(&hmap->lock[lock_idx]);
		if (key_len == hmap->key_len)
			break;
		utl_spin_unlock(&hmap->lock[lock_idx]);
	}

	map_entry = get_map_entry(hmap, slot);
	
#ifdef UTL_USE_ASSERT
	if ((map_member_exist(map_entry, key)))
		printf("key=0x%lx exist\n", key);
	assert(!(map_member_exist(map_entry, key)));
#endif /* UTL_USE_ASSERT */

	member = map_member_alloc();

	member->key = key;
	member->val = val;
	list_add(&member->crash, &map_entry->entry);
	list_add(&member->p, &hmap->members);
	map_entry->n++;
	fetch_and_add(&hmap->fill, 1);

	utl_spin_unlock(&hmap->lock[lock_idx]);

	return ret;
}

void
hash_map_delete_member(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;
	struct map_member *member;
	int lock_idx;
	int key_len;
	unsigned long slot;

	while(1) {
		key_len = hmap->key_len;
		slot = get_map_slot(key, hmap->key_mask, key_len);
		lock_idx = get_lock_idx(slot);
		utl_spin_lock(&hmap->lock[lock_idx]);
		if (key_len == hmap->key_len)
			break;
		utl_spin_unlock(&hmap->lock[lock_idx]);
	}

	map_entry = get_map_entry(hmap, slot);
	member = get_map_member(map_entry, key);
#ifdef UTL_USE_ASSERT
	assert(member != NULL);
#endif /* UTL_USE_ASSERT */

	list_del(&member->crash);
	map_member_free(member);
	map_entry->n--;
	fetch_and_sub(&hmap->fill, 1);

	utl_spin_unlock(&hmap->lock[lock_idx]);
}

void*
hash_map_find_member(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;
	struct map_member *member;
	int lock_idx;
	int key_len;
	unsigned long slot;

	while(1) {
		key_len = hmap->key_len;
		slot = get_map_slot(key, hmap->key_mask, key_len);
		lock_idx = get_lock_idx(slot);
		utl_spin_lock(&hmap->lock[lock_idx]);
		if (key_len == hmap->key_len)
			break;
		utl_spin_unlock(&hmap->lock[lock_idx]);
	}

	map_entry = get_map_entry(hmap, slot);
	member = get_map_member(map_entry, key);
	if (member == NULL) {
		utl_spin_unlock(&hmap->lock[lock_idx]);
		return NULL;
	}
	utl_spin_unlock(&hmap->lock[lock_idx]);
	return member->val;
}

int 
key_crash_in_hash_map(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;
	int ret, lock_idx;
	int key_len;
	unsigned long slot;

	while(1) {
		key_len = hmap->key_len;
		slot = get_map_slot(key, hmap->key_mask, key_len);
		lock_idx = get_lock_idx(slot);
		utl_spin_lock(&hmap->lock[lock_idx]);
		if (key_len == hmap->key_len)
			break;
		utl_spin_unlock(&hmap->lock[lock_idx]);
	}

	map_entry = get_map_entry(hmap, slot);
	ret = map_entry->n <= 1 ? 0 : 1;
	utl_spin_unlock(&hmap->lock[lock_idx]);

	return ret;
}

int 
hash_map_init()
{
	return 0;
}

void
hash_map_destroy()
{
	struct map_member *member;

	if (list_need_init(&free_member))
		list_init(&free_member);

	while (!list_empty(&free_member)) {
		member = next_entry(&free_member, struct map_member, p);
		list_del(&member->p);
		utl_free(member);
	}
}


