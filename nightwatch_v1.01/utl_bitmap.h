#ifndef UTL_BITMAP_H_
#define UTL_BITMAP_H_



struct bitmap {
	unsigned long *map;
	int size;
	int nr_blk;
};


struct bitmap* new_bitmap(int size, int init_val);
void delete_bitmap(struct bitmap *m);

int bit_test(struct bitmap *m, int n);
void set_bit(struct bitmap *m, int n);
void clr_bit(struct bitmap *m, int n);

int upward_bit_test(struct bitmap *m, int n);
int downward_bit_test(struct bitmap *m, int n);


#endif /* UTL_BITMAP_H_ */

