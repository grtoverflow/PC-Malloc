#ifndef LOCALITY_PROFILE_H_
#define LOCALITY_PROFILE_H_

#include <stdint.h>

#include "utl_list.h"
#include "utl_wrapper.h"



struct page_sample;
struct alloc_context;


struct memory_chunk {
	int mapping_type;

	unsigned long addr;
	size_t size;
	unsigned long idx;

	int state;

	/* reference samples in the previous cycles */
	uint32_t llc_victim_ref;
	uint32_t llc_pollutor_ref;
	uint32_t total_ref;
	float mr;

	/* reference samples in the current cycle */
	uint32_t i_victim_ref;
	uint32_t i_pollutor_ref;
	uint32_t i_total_ref;

	/* number of the minimum sampling cycle */
	int base_sample_cycle;
	/* number of the performed sampling cycle */
	int sample_cycle;

	/* each sample round contains multiple sample cycles */
	int sample_state;

	/* number of sampled pages */
	int nr_sample;

	utl_spinlock_t lock;

	struct alloc_context *context;
	struct list_head sample;
	struct list_head p;
	struct list_head sibling;
};


#define MAP_ONLY_CONTEXT 0

struct alloc_context {
	union {
      struct {
		uint64_t context_key;
		unsigned long idx;
      };
	  struct {
	    uint64_t map_only_context_flag;
		unsigned long map_type;
	  };
	  struct {
	    struct alloc_context *ctx; 
	  };
	};

	utl_spinlock_t lock;

	int predict_type[2];
	float predict_mr[2];
	int sample_skip;
	int skip_interval;

	size_t last_chunk_sz;

	struct list_head chunk;
	struct list_head p;
	struct list_head s2t_set;
};


int locality_profile_init();
void locality_profile_destroy();

struct memory_chunk *
alloc_and_init_chunk(void *p, size_t size, int type,
                     struct alloc_context *context);

void detach_chunk_from_context(struct memory_chunk *chunk);


void update_s2t_map(size_t size, int mapping);

typedef int (*process_func)(struct memory_chunk *chunk);

void process_active_chunk(process_func);

void* NightWatch_get_alloc_context(size_t size);



#endif /* LOCALITY_PROFILE_H_ */



