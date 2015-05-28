#ifndef HASH_MAP_64_H_
#define HASH_MAP_64_H_

#include <stdint.h>



struct hash_map;

int hash_map_init();
void hash_map_destroy();

struct hash_map* new_hash_map();
void delete_hash_map(struct hash_map* hmap);

int hash_map_add_member(struct hash_map *hmap, uint64_t key, void *val);
void hash_map_delete_member(struct hash_map *hmap, uint64_t key);
void* hash_map_find_member(struct hash_map *hmap, uint64_t key);
int key_crash_in_hash_map(struct hash_map *hmap, uint64_t key);

#endif /* HASH_MAP_64_H_ */

