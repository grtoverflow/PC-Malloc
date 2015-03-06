#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "utl_wrapper.h"


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
	int size;
	int fill;
	int key_len;
	unsigned long key_mask;
	struct list_head members;
};

struct list_head free_member;

#define HT_INIT_SIZE	1024
#define HT_INIT_KEY_LEN	10

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

	if (unlikely(list_empty(&free_member))) {
		member = (struct map_member*)
			wrap_malloc(sizeof(struct map_member));	
#ifdef USE_ASSERT
		assert(member);
#endif /* USE_ASSERT */
	} else {
		member = next_entry(&free_member, struct map_member, p);	
		list_del(&member->p);
	}

	return member;
}

static inline void
map_member_free(struct map_member *member)
{
	list_del(&member->p);
	list_add(&member->p, &free_member);
}

struct hash_map*
new_hash_map()
{
	struct hash_map *hmap;
	int i, size;

	hmap = (struct hash_map*)wrap_malloc(sizeof(struct hash_map));	
#ifdef USE_ASSERT
	assert(!!hmap);
#endif /* USE_ASSERT */
	
	size = HT_INIT_SIZE * sizeof(struct hmap_entry);
	hmap->map = (struct hmap_entry*)wrap_malloc(size);
#ifdef USE_ASSERT
	assert(!!hmap->map);
#endif /* USE_ASSERT */
	memset(hmap->map, 0, size);
	
	hmap->size = HT_INIT_SIZE;
	hmap->fill = 0;
	hmap->key_len = HT_INIT_KEY_LEN;
	hmap->key_mask = get_key_mask(HT_INIT_KEY_LEN);
	list_init(&hmap->members);

	for (i = 0; i < HT_INIT_SIZE; i++) {
		list_init(&hmap->map[i].entry);	
	}

	return hmap;
}

void
delete_hash_map(struct hash_map* hmap)
{
	struct map_member *member;

	while (!list_empty(&hmap->members)) {
		member = next_entry(&hmap->members, struct map_member, p);
		map_member_free(member);
	}

	wrap_free(hmap->map);
	wrap_free(hmap);
}

static inline struct hmap_entry*
get_map_entry(struct hash_map *hmap, uint64_t key)
{
	unsigned long slot;

	slot = get_map_slot(key, hmap->key_mask, hmap->key_len);

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
	
	members = &hmap->members;
	if (unlikely(list_empty(members)))
		return 0;
	map = hmap->map;
	
	hmap->size <<= 1;
	size = hmap->size * sizeof(struct hmap_entry);
	hmap->map = (struct hmap_entry*)wrap_malloc(size);
	assert(!!hmap->map);
	memset(hmap->map, 0, size);

	hmap->key_len++;
	hmap->key_mask = get_key_mask(hmap->key_len);

	size = hmap->size;
	for (i = 0; i < size; i++) {
		list_init(&hmap->map[i].entry);	
	}

	list_for_each_entry(member_iter, members, p) {
		map_entry = get_map_entry(hmap, member_iter->key);	
		map_entry->n++;
		list_del(&member_iter->crash);
		list_add(&member_iter->crash, &map_entry->entry);
	}

	wrap_free(map);

	return 0;
}

int 
hash_map_add_member(struct hash_map *hmap, uint64_t key, void *val)
{
	struct hmap_entry *map_entry;
	struct map_member *member;
	int ret;

	ret = 0;

	if (unlikely(hash_map_full(hmap))) {
		ret = hash_map_rebuild(hmap);
#ifdef USE_ASSERT
		assert(ret == 0);
#endif /* USE_ASSERT */
	}

	map_entry = get_map_entry(hmap, key);
#ifdef USE_ASSERT
	if ((map_member_exist(map_entry, key)))
		printf("key=0x%lx exist\n", key);
	assert(!(map_member_exist(map_entry, key)));
#endif /* USE_ASSERT */

	member = map_member_alloc();

	member->key = key;
	member->val = val;
	list_add(&member->crash, &map_entry->entry);
	list_add(&member->p, &hmap->members);
	map_entry->n++;
	hmap->fill++;

	return ret;
}

void
hash_map_delete_member(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;
	struct map_member *member;

	map_entry = get_map_entry(hmap, key);
	member = get_map_member(map_entry, key);
	if (member == NULL) {
		printf("hash_map_delete_member abnormal\n");
		return;
	}
#if 0
#ifdef USE_ASSERT
	assert(!!member);
#endif /* USE_ASSERT */
#endif

	list_del(&member->crash);
	map_member_free(member);
	
	map_entry->n--;
	hmap->fill--;
}

void*
hash_map_find_member(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;
	struct map_member *member;

	map_entry = get_map_entry(hmap, key);
	member = get_map_member(map_entry, key);
	if (member == NULL) {
		return NULL;
	}
	return member->val;
}

int 
key_crash_in_hash_map(struct hash_map *hmap, uint64_t key)
{
	struct hmap_entry *map_entry;

	map_entry = get_map_entry(hmap, key);
	return map_entry->n <= 1 ? 0 : 1;
}

int 
hash_map_init()
{
	list_init(&free_member);

	return 0;
}

void
hash_map_destroy()
{
	struct map_member *member;

	while (!list_empty(&free_member)) {
		member = next_entry(&free_member, struct map_member, p);
		list_del(&member->p);
		wrap_free(member);
	}
}


