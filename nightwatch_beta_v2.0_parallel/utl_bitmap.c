#include <stdio.h>

#include "utl_builtin.h"
#include "utl_wrapper.h"
#include "utl_bitmap.h"



#define MAP_BLK_SZ	(sizeof(unsigned long) * 8)
#define MAP_BLK_RMASK	(MAP_BLK_SZ - 1)
#define MAP_BLK_MASK	(~(MAP_BLK_SZ - 1))


#define out_of_range(m, n) ((n) < 0 || (n) >= (m)->size)


struct bitmap* 
new_bitmap(int size, int init_val)
{
	struct bitmap *m;
	int nr_bytes, nr_blk;

	if (unlikely(size <= 0))
		return NULL;

	nr_blk = (size + (MAP_BLK_SZ - 1)) / MAP_BLK_SZ;
	nr_bytes = nr_blk << 3;
	m = (struct bitmap *)utl_malloc(sizeof(struct bitmap));
	m->map = (unsigned long *)utl_malloc(nr_bytes);

	if (init_val == 0) {
		utl_memset(m->map, 0, nr_bytes);
	} else {
		utl_memset(m->map, 0xff, nr_bytes);
		m->map[nr_blk - 1] 
			= m->map[nr_blk - 1] >> ((nr_bytes << 3) - size);
	}
	m->size = size;
	m->nr_blk = nr_blk;

	return m;
}


void
delete_bitmap(struct bitmap *m)
{
	utl_free(m->map);
	utl_free(m);
}


int
bit_test(struct bitmap *m, int n)
{
	int blk_idx, offset;

	if (unlikely(out_of_range(m, n)))
		return 0;

	blk_idx = n / MAP_BLK_SZ;
	offset = n - blk_idx * MAP_BLK_SZ;

	return !!(m->map[blk_idx] & (1UL << offset));
}


void
set_bit(struct bitmap *m, int n)
{
	int blk_idx, offset;

	if (unlikely(out_of_range(m, n)))
		return;

	blk_idx = n / MAP_BLK_SZ;
	offset = n - blk_idx * MAP_BLK_SZ;

	m->map[blk_idx] |= (1UL << offset);
}


void
clr_bit(struct bitmap *m, int n)
{
	int blk_idx, offset;

	if (unlikely(out_of_range(m, n)))
		return;

	blk_idx = n / MAP_BLK_SZ;
	offset = n - blk_idx * MAP_BLK_SZ;

	m->map[blk_idx] &= ~(1UL << offset);
}


/* Start from index n, find the index of the
 * LEAST significant 1-bit of x.
 * If no 1-bit is found, return -1 */
int
upward_bit_test(struct bitmap *m, int n)
{
	int p;
	int blk_idx, offset;
	unsigned long blk;

	if (unlikely(out_of_range(m, n)))
		return -1;

	blk_idx = n / MAP_BLK_SZ;
	offset = n - blk_idx * MAP_BLK_SZ;
	p = n;

	do {
		blk = m->map[blk_idx++] >> offset;
		if (blk == 0) {
			p += MAP_BLK_SZ - offset;
			offset = 0;
		} else {
			p += ffsl(blk) - 1;
			break;
		}
	} while (p < m->size);

	return out_of_range(m, p) ? -1 : p;
}


/* Start from index n, find the index of the
 * MOST significant 1-bit of x.
 * If no 1-bit is found, return -1 */
int
downward_bit_test(struct bitmap *m, int n)
{
	int p;
	int blk_idx, offset;
	unsigned long blk;

	if (unlikely(out_of_range(m, n)))
		return -1;

	blk_idx = n / MAP_BLK_SZ;
	offset = n - blk_idx * MAP_BLK_SZ;
	p = ((n + MAP_BLK_SZ) & MAP_BLK_MASK) -1;

	do {
		blk = offset == MAP_BLK_SZ - 1 ? m->map[blk_idx]
		      : m->map[blk_idx] & ((1UL << (offset + 1)) - 1);
		blk_idx--;
		if (blk == 0) {
			p -= MAP_BLK_SZ;
			offset = MAP_BLK_SZ - 1;
		} else {
			p -= MAP_BLK_SZ - 1 - bsfl(blk);
			break;
		}
	} while (p >= 0);

	return out_of_range(m, p) ? -1 : p;
}









