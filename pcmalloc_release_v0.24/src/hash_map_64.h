#ifndef HASH_MAP_64_H_
#define HASH_MAP_64_H_

#include <stdint.h>

#include "list.h"


struct hash_map_64;

int hash_map_64_init();
void hash_map_64_destroy();

struct hash_map_64* new_hash_map_64();
void hash_map_64_delete(struct hash_map_64* hmap);

int hash_map_64_add_member(struct hash_map_64 *hmap, uint64_t key, void *val);
void hash_map_64_delete_member(struct hash_map_64 *hmap, uint64_t key);
void* hash_map_64_find_member(struct hash_map_64 *hmap, uint64_t key);
int key_crash_in_hash_map(struct hash_map_64 *hmap, uint64_t key);

#endif /* HASH_MAP_64_H_ */

