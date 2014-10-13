#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "build_in.h"
#include "allocator.h"


#define EXPANDSIZE		16777216
#define POOLNUM 		1026
#define SIZESTATE 		12
#define FILLNUM 		8

#define ALIGNSIZE(sz) ((unsigned long)(((sz) + 7) & ~7))
#define size2index(sz) ((ALIGNSIZE(sz)>>3) + 1)

#define MEMSIZE			112
#define alignsz 		64
#define head_reserve	48
//#define CHUNK_FLAG		0xfedcba98U

#define ADDRALIGN(s, alignptr)		\
do {								\
	void *tempptr = (void *)((char *)(s) + head_reserve);					\
	unsigned int offset = alignsz - ((unsigned long)tempptr%(alignsz));		\
	alignptr = (void *)((char *)tempptr + offset);							\
	((unsigned int *)alignptr)[-1] = offset;								\
	((unsigned int *)alignptr)[-2] = boot_alloc_mark;								\
} while(0)


#define fit_size(p, sz)	\
	(((p)->size + MEMSIZE >= (sz)) && ((p)->size + MEMSIZE <= (sz) + (PAGE_SIZE)))

typedef struct memchunk {
	struct memchunk *next;
	struct memchunk *pre;
	unsigned int size;
	unsigned int precise_size;
	unsigned int used;
	unsigned int type;
	void *private;
} memchunk, *pmemchunk, poolhead, *ppoolhead;

static poolhead pools_1[POOLNUM];
static poolhead pools_2[POOLNUM];
static poolhead pools_3[POOLNUM];
static memchunk *memstate_1[SIZESTATE]={0};
static memchunk *memstate_2[SIZESTATE]={0};
static memchunk *memstate_3[SIZESTATE]={0};

static inline pmemchunk
expand_pool(unsigned int size, int type)
{
	memchunk *p;
	unsigned int sz;
	unsigned int alisz;

	sz = (size > EXPANDSIZE ? size : (EXPANDSIZE + MEMSIZE));
	//sz = ALIGNSIZE(sz + MEMSIZE);
	alisz = ALIGNSIZE(sz);

	void *expand_chunk =
			mmap(NULL, alisz, PROT_READ|PROT_WRITE, 
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_CACHE_AWARE_STATE, 
				-1, type);

	if (__builtin_expect((expand_chunk == (void *)-1), 0))
		return NULL;
	p = (memchunk *)expand_chunk;
	p->next = p->pre = NULL;
	p->size = alisz - MEMSIZE;
	p->private = NULL;

	return p;
}


int
pc_malloc_init()
{
	int i;
	int j;

	(pools_1)->next = expand_pool(EXPANDSIZE, 1);
	(pools_1->next)->next = pools_1;
	(pools_1->next)->pre = pools_1;
	(pools_1->pre) = pools_1->next;
	pools_1->next->type = 1;
	//(pools_1 + 1)->next = pools_1 + 1;

	(pools_2)->next = expand_pool(EXPANDSIZE, 2);
	(pools_2->next)->next = pools_2;
	(pools_2->next)->pre = pools_2;
	(pools_2->pre) = pools_2->next;
	pools_2->next->type = 2;
	//(pools_2 + 1)->next = pools_2 + 1;

	(pools_3)->next = expand_pool(EXPANDSIZE, 3);
	(pools_3->next)->next = pools_3;
	(pools_3->next)->pre = pools_3;
	(pools_3->pre) = pools_3->next;
	pools_3->next->type = 3;
	//(pools_3 + 1)->next = pools_1 + 3;

	memstate_1[SIZESTATE - 1] = pools_1->next;
	memstate_2[SIZESTATE - 1] = pools_2->next;
	memstate_3[SIZESTATE - 1] = pools_3->next;

	for(i = 2; i < POOLNUM; i++) {
		j = 8*(i-1);
		(pools_1 + i)->next = (pools_1 + i);
		(pools_1 + i)->size = j; //8 * (i-1);
		(pools_1 + i)->used = 0;
		(pools_1 + i)->type = 1;

		(pools_2 + i)->next = (pools_2 + i);
		(pools_2 + i)->size = j;
		(pools_2 + i)->used = 0;
		(pools_2 + i)->type = 2;

		(pools_3 + i)->next = (pools_3 + i);
		(pools_3 + i)->size = j;
		(pools_3 + i)->used = 0;
		(pools_3 + i)->type = 3;
	}

	return 0;
}


static inline void
fill_chunk(poolhead *poolq, int index)
{
	unsigned int	sz;
	memchunk		**pstate;
	memchunk		*temp;
	memchunk		*p;
	int				type;

	type	= poolq->type;
	sz		= (poolq+index)->size + MEMSIZE;
	pstate	= (type == 1 ?
				memstate_1 : (type == 2 ? memstate_2 : memstate_3));
	if (pstate[11] == 0) {
		temp = expand_pool(EXPANDSIZE, type);
		if (__builtin_expect(temp == NULL, 0))
			return;
		pstate[11] = temp;
		temp->type = type;
		//temp->type = poolq->type;
		temp->next = poolq;
		temp->pre = poolq->pre;
		poolq->pre->next = temp;
		poolq->pre = temp;
	}
	p = pstate[11];

	int		i;
	char	*tail;

	for (i = 0; i != FILLNUM; i++) {
		tail = (char *)p + MEMSIZE + p->size;
		temp = (memchunk *)(tail - sz);
		temp->size = (poolq + index)->size;
		temp->used = 0;
		temp->type = type;
		//temp->type = poolq->type;

		p->size		= p->size - sz;
		temp->next	= (poolq + index)->next;
		temp->pre	= (poolq + index);
		(poolq + index)->next->pre	= temp;
		(poolq + index)->next		= temp;
		(poolq + index)->used++;
	}

	// index
	int idx = 0;
	sz		= p->size;
	if (sz>>23) {
		idx = 11;
		return;
	}
	else {
	// in fill_chunk(), current_index-1
		for (sz>>=13; sz > 0; sz>>=1)
			++idx;
		if (p->next == poolq) {
			pstate[11] = 0;
		}
		else {
			pstate[11] = p->next;
		}
	}
}


void
pc_free(void *p)
{

	if (unlikely(p == NULL)) {
		return;
	}

	memchunk *s;
	poolhead *q;
	memchunk **pstate;
	unsigned int sz;
	
	if (((unsigned *)p)[-2] != boot_alloc_mark) {
		free(p);
		return;
	}

/*
	char offset = ((char *)p)[-1];
	p = (char *)p - offset;
*/
	unsigned int offset = ((unsigned *)p)[-1];
	p = (char *)p - offset;
	s = (memchunk *)((char *)p - head_reserve);

	if ((s->used) != 1) {
		//printf("not allocated memory!\n");
		return;
	}

	q	= (s->type == 1 ? pools_1 : (s->type == 2 ? pools_2 : pools_3));
	sz	= s->size;

	int index;
	int i;

	if (sz <= 8192) {
		index = size2index(s->size);

		s->next	= (q + index)->next;
		s->pre	= (q + index);
		(q + index)->next->pre	= s;
		(q + index)->next		= s;
		(q + index)->used++;
	}
	else {
		pstate = (s->type == 1 ? memstate_1 : (s->type == 2 ? memstate_2 : memstate_3));
		if (sz>>23) {
			index = 11;
			if (pstate[11] == 0) {
				pstate[11] = s;
			}
			s->next	= q;
			s->pre	= q->pre;
			q->pre->next = s;
			q->pre	= s;
		}
		else {
			index = 0;
			for (sz>>=13; sz > 0; sz>>=1) {
				++index;
			}
			if (pstate[index] != 0) {
				s->next	= pstate[index]->next;
				s->pre	= pstate[index];
				pstate[index]->next->pre	= s;
				pstate[index]->next			= s;
			}
			else {
				pstate[index] = s;
				for (i = index+1; i < SIZESTATE; i++) {
					if (pstate[i])
						break;
				}
				if (i == 12) {
					s->next	= q;
					s->pre	= q->pre;
					q->pre->next = s;
					q->pre	= s;
				}
				else {
					s->next	= pstate[i];
					s->pre	= pstate[i]->pre;
					pstate[i]->pre->next	= s;
					pstate[i]->pre			= s;
				}
			}
		}
	}

	s->private		= NULL;
	s->precise_size = 0;
	s->used			= 0;
}

void*
pc_malloc(int type, size_t bytes)
{
	poolhead *q;
	memchunk *s;
	memchunk **pstate;
	memchunk *temp;
	memchunk *p;
	unsigned int sz;
	unsigned int sszz;
	int		index;
	int		idx;
	int		tmpidx;
	char	*tail;
	void	*alignptr;

	q = (type == 1 ? pools_1 : (type == 2 ? pools_2 : pools_3));
	q->type = type;

	if (bytes == 0)
		bytes = 1;
	if (bytes <= 8192) {
		index = size2index(bytes);
		if ((q + index)->used == 0) {
			fill_chunk(q, index);
			if (__builtin_expect((q + index)->used == 0, 0)) {
				return NULL;
			}
		}

		(q + index)->used--;
		s = (q + index)->next;
		s->next->pre		= q + index;
		(q + index)->next	= s->next;
		s->used = 1;
		s->private		= NULL;
		s->precise_size	= bytes;

		ADDRALIGN(s, alignptr);
		return alignptr;
	}
	else {
		pstate	= (type == 1 ? memstate_1 : (type == 2 ? memstate_2 : memstate_3));
		sszz	= sz = bytes + MEMSIZE;

		index	= 0;
		if (sszz>>23) {
			index = 11;
		}
		else {
			for (sszz>>=13; sszz > 0; sszz>>=1) {
				++index;
			}
		}
		if (pstate[index] == 0) {
			while ((index < SIZESTATE) && (pstate[index] == 0)) {
				index++;
			}
			if (index == 12) {
				index--;
				temp = expand_pool(sz, type);
				if (__builtin_expect(temp == NULL, 0)) {
					return NULL;
				}
				pstate[index]	= temp;
				temp->type		= type;
				temp->next		= q;
				temp->pre		= q->pre;
				q->pre->next	= temp;
				q->pre			= temp;
			}
		}

		p = pstate[index];
		//q = (type == 1 ? pools_1 : (type == 2 ? pools_2 : pools_3));
		unsigned int tmpsz	= 0;
		unsigned int fucksz	= 0;
		int tmpindex	= 0;
		int fuckindex	= 0;

		for ( ; p!=q; ) {
		//printf("in for,out fit_size\n");
			if (fit_size(p, sz)) {
			//printf("in fit_size\n");
				p->next->pre = p->pre;
				p->pre->next = p->next;
				s = p;
				s->used			= 1;
				s->type			= type;
				s->precise_size	= bytes;
				s->private		= NULL;
    
				// index
				tmpsz = p->size;
				if (tmpsz>>23) {
					tmpindex = 11;
				}
				else {
					tmpindex = 0;
					for (tmpsz >>= 13; tmpsz > 0; tmpsz >>= 1) {
						tmpindex++;
					}
				}
    
				if (tmpindex == index) {
					if (pstate[index] == p) {
						if (p->next == q) {
							pstate[index] = 0;
							ADDRALIGN(s, alignptr);
							return alignptr;
						}
						else {
							fucksz = p->next->size;
							if (fucksz>>23) {
								fuckindex = 11;
							}
							else {
								fuckindex = 0;
								for (fucksz >>= 13; fucksz; fucksz >>= 1) {
									fuckindex++;
								}
							}
    
							if (tmpindex == fuckindex) {
								pstate[index] = p->next;
								ADDRALIGN(s, alignptr);
								return alignptr;
							}
							else {
								pstate[index] = 0;
								ADDRALIGN(s, alignptr);
								return alignptr;
							}
						}
					}
					else {
						ADDRALIGN(s, alignptr);
						return alignptr;
					}
				}
				else {
					if (p->next == q) {
						pstate[tmpindex] = 0;
						ADDRALIGN(s, alignptr);
						return alignptr;
					}
					fucksz = p->next->size;
					if (fucksz>>23) {
						fuckindex = 11;
					}
					else {
						fuckindex = 0;
						for (fucksz >>= 13; fucksz > 0; fucksz >>= 1) {
							fuckindex++;
						}
					}
    
					if (tmpindex == fuckindex) {
						pstate[tmpindex] = p->next;
						ADDRALIGN(s, alignptr);
						return alignptr;
					}
					else {
						pstate[tmpindex] = 0;
						ADDRALIGN(s, alignptr);
						return alignptr;
					}
				}
			}
			else if (p->size < sz) {
				if (p->next == q) {
					temp = expand_pool(sz, type);
					if (__builtin_expect(temp == NULL, 0)) {
						return NULL;
					}

					if (pstate[11] == 0) {
						pstate[11] = temp;
					}

					temp->type	= type;
					temp->next	= p->next;
					temp->pre	= p;
					p->next->pre = temp;
					p->next		= temp;
				}
				p = p->next;
				continue;
			}
			else {
			
				sszz	= p->size;
				index	= 0;
				if (sszz>>23) {
					index = 11;
				}
				else {
					for (sszz>>=13; sszz > 0; sszz>>=1)
						++index;
				}

				tail = (char *)p + MEMSIZE + p->size;
				s = (memchunk *)(tail - sz);
				s->size = bytes;
				s->used = 1;
				s->type = type;
				p->size = p->size - sz;

				break;
			}
		}

		// index
		sszz	= p->size;
		idx		= 0;
		if (sszz>>23)
			idx = 11;
		else {
			for (sszz>>=13; sszz > 0; sszz>>=1)
				++idx;
		}
		if (index == idx) {
			s->private		= NULL;
			s->precise_size	= bytes;
			ADDRALIGN(s, alignptr);
			return alignptr;
		}
		else {
			p->pre->next = p->next;
			p->next->pre = p->pre;

			if (pstate[index] == p) {
				if (p->next == q) {
					pstate[index] = 0;
				}
				else {
					sszz = p->next->size;
					if (sszz>>23)
						tmpidx = 11;
					else {
						tmpidx = 0;
						for (sszz>>=13; sszz > 0; sszz>>=1)
							++tmpidx;
					}
					if (index == tmpidx) {
						pstate[index] = p->next;
					}
					else {
						pstate[index] = 0;
					}
				}
			}

			if (pstate[idx] == 0) {
				pstate[idx] = p;
				idx++;
				for (; idx < SIZESTATE; idx++) {
					if (pstate[idx] != 0)
						break;
				}
				if (idx == 12) {
					p->next	= q;
					p->pre	= q->pre;
					q->pre->next = p;
					q->pre	= p;
				}
				else {
					p->next	= pstate[idx];
					p->pre	= pstate[idx]->pre;
					pstate[idx]->pre->next = p;
					pstate[idx]->pre = p;
				}

			}
			else {
				p->next	= pstate[idx]->next;
				p->pre	= pstate[idx];
				pstate[idx]->next->pre	= p;
				pstate[idx]->next		= p;
			}


			s->private		= NULL;
			s->precise_size	= bytes;
			ADDRALIGN(s, alignptr);
			return alignptr;
		}
	}
}

void*
pc_calloc(int type, size_t nmemb, size_t bytes)
{
	void *result;

	result = pc_malloc(type, nmemb * bytes);
	memset(result, 0, nmemb * bytes);

	return result;
}


void*
pc_realloc(int type, void *p, size_t newsize)
{
	memchunk	*phead;
	void		*oldp;
	//char 		offset;
	oldp = p;

	if (((unsigned *)p)[-2] != boot_alloc_mark) {
		return realloc(p, newsize);
	}

	if (p != NULL) {
	/*
		offset = ((char *)p)[-1];
		p = (char *)p - offset;
	*/
		unsigned int offset = ((unsigned *)p)[-1];
		p = (char *)p - offset;
		phead = (memchunk *)(p - head_reserve);
    
		if (phead->used != 1) {
			//printf("error: non-exist memory\n");
			return NULL;
		}

		if (phead->size >= newsize) {
			return oldp;
		}

		if (newsize != 0) {
			p = pc_malloc(type, newsize);
			//memmove(p, oldp, phead->size);
			memcpy(p, oldp, phead->size);
			pc_free(oldp);
			return p;
		}
		else {
			pc_free(p);
			p = NULL;
			return p;
		}
	}
	else {
		if (newsize >= 0) {
			p = pc_malloc(type, newsize);
			return p;
		}
		else {
			return NULL;
		}
	}
}


void 
switch_mapping(void *p, int target_mapping)
{
	if (unlikely(p == NULL)) {
		return;
	}

	memchunk *s;
	
	if (((unsigned *)p)[-2] != boot_alloc_mark) {
		return;
	}

	unsigned int offset = ((unsigned *)p)[-1];
	p = (char *)p - offset;
	s = (memchunk *)((char *)p - head_reserve);

	if ((s->used) != 1 || s->type == target_mapping) {
		return;
	}

	s->type = target_mapping;
	
	mmap(NULL, s->size, PROT_READ|PROT_WRITE, 
				MAP_PRIVATE|MAP_ANONYMOUS|REMAP_CACHE_AWARE_STATE, 
				-1, target_mapping);
}

size_t
get_chunk_size(void *p)
{
	if (unlikely(p == NULL))
		return 0;

	memchunk *tmp;
	/*
	char offset = ((char *)p)[-1];
	p = (char *)p - offset;
	*/
	unsigned int offset = ((unsigned *)p)[-1];
	p = (char *)p - offset;
	tmp = (memchunk *)(p - head_reserve);
	return tmp->precise_size;
}

void
set_chunk_private(void *p, void *private)
{

	if (unlikely(p == NULL))
		return;

	memchunk *tmp;
	/*
	char offset = ((char *)p)[-1];
	p = (char *)p - offset;
	*/
	unsigned int offset = ((unsigned *)p)[-1];
	p = (char *)p - offset;
	tmp = (memchunk *)(p - head_reserve);
	tmp->private = private;
}

void *
get_chunk_private(void *p)
{
	if (unlikely(p == NULL))
		return NULL;

	memchunk *tmp;
	/*
	char offset = ((char *)p)[-1];
	p = (char *)p - offset;
	*/
	unsigned int offset = ((unsigned *)p)[-1];
	p = (char *)p - offset;
	tmp = (memchunk *)(p - head_reserve);
	return tmp->private;
}

