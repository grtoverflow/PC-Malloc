#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>

#include "config.h"
#include "utl_builtin.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "allocator.h"
#include "pc_malloc.h"
#include "locality_profile.h"
#include "stdlib_hook.h"
#include "chunk_monitor.h"
#include "chunk_predictor.h"



#define PC_MALLOC_UNINIT		0
#define PC_MALLOC_ACTIVE		1
#define CACHE_CONTROL_ENABLED		2
#define PC_MALLOC_DESTROYED		0



static int pc_malloc_state = PC_MALLOC_UNINIT;
static int malloc_depth = 0;


#define inc_malloc_depth() (malloc_depth++)
#define dec_malloc_depth() (malloc_depth--)
#define inner_malloc() (malloc_depth > 0)

#define tiny_chunk(size) ((size) <= CACHE_LINE_SZ)

void force_inc_malloc_depth() { malloc_depth++; }
void force_dec_malloc_depth() { malloc_depth--; }




static inline void
set_pc_malloc_active()
{
	pc_malloc_state |= PC_MALLOC_ACTIVE;
}

static inline int
pc_malloc_active()
{
	return pc_malloc_state & PC_MALLOC_ACTIVE;
}

static inline void
enable_cache_control()
{
	pc_malloc_state |= CACHE_CONTROL_ENABLED;
}

static inline int
cache_control_enabled()
{
	return pc_malloc_state & CACHE_CONTROL_ENABLED;
}


void malloc_destroy();


int
malloc_init()
{
	int ret;

	inc_malloc_depth();
	
	ret = 0;
	if (pc_malloc_active())
		goto out;
	set_pc_malloc_active();

	ret = allocator_init();
	if (!!ret) goto out;

	ret = hash_map_init();
	if (!!ret) goto out;

	ret = install_stdlibapi_hook();
	if (!!ret) goto out;

	ret = chunk_monitor_init();
	if (!!ret) goto out;

	ret = locality_profile_init();
	if (!!ret) goto out;
		
out:
	dec_malloc_depth();
	return ret;
}

void
malloc_destroy()
{
	if (pc_malloc_state == PC_MALLOC_DESTROYED)
		return;

	inc_malloc_depth();

	locality_profile_destroy();

	chunk_monitor_destroy();

	hash_map_destroy();

	pc_malloc_state = PC_MALLOC_DESTROYED;

	dec_malloc_depth();
}



/* If pc_malloc_enable() is not called,
 * then pc_malloc is a traditional allocator,
 * and no cache control will be performed.*/ 
void
pc_malloc_enable()
{
	if (unlikely(!pc_malloc_active())) {
		malloc_init();
	}

	inc_malloc_depth();

	process_active_chunk(monit_chunk);

	enable_cache_control();

	atexit(malloc_destroy);

	dec_malloc_depth();
}


void *
malloc(size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	if (inner_malloc() || tiny_chunk(size)
			|| !cache_control_enabled()) {
		p = pc_malloc(OPEN_MAPPING, size);	
		goto done;
	}

	inc_malloc_depth();

	context = get_alloc_context(size);

	if (context == NULL) {
		p = pc_malloc(OPEN_MAPPING, size);	
		dec_malloc_depth();
		goto done;
	}

	type = get_mapping_type(context);

	p = pc_malloc(type, size);	

	p = chunk_level_sampling(p, size, type, ACT_MALLOC, context);

	dec_malloc_depth();

done:
	return p;
}

void *
calloc(size_t nmemb, size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	if (inner_malloc() || tiny_chunk(nmemb * size)
			|| !cache_control_enabled()) {
		p = pc_calloc(OPEN_MAPPING, nmemb, size);	
		goto done;
	}

	inc_malloc_depth();

	context = get_alloc_context(nmemb * size);

	if (context == NULL) {
		p = pc_calloc(OPEN_MAPPING, nmemb, size);	
		dec_malloc_depth();
		goto done;
	}

	type = get_mapping_type(context);

	p = pc_calloc(type, nmemb, size);	

	p = chunk_level_sampling(p, nmemb * size, type, ACT_CALLOC, context);

	dec_malloc_depth();

done:
	return p;
}

void
free(void *p)
{
	if (unlikely(p == NULL))
		return;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	collect_chunk_level_sample(p);

	pc_free(p);
}

void *
realloc(void *old, size_t size)
{
	struct alloc_context *context;
	void *p;
	int type;

	if (unlikely(!pc_malloc_active()))
		malloc_init();

	if (unlikely(size == 0)) {
		if (old != NULL)
			free(old);
		p = NULL;
		goto done;
	}

	if (inner_malloc() || tiny_chunk(size)
			|| !cache_control_enabled()) {
		if (old == NULL)
			p = pc_malloc(OPEN_MAPPING, size);	
		else
			p = pc_realloc(OPEN_MAPPING, old, size);	
		goto done;
	}

	inc_malloc_depth();

	context = get_alloc_context(size);

	if (context == NULL) {
		if (old == NULL)
			p = pc_malloc(OPEN_MAPPING, size);	
		else
			p = pc_realloc(OPEN_MAPPING, old, size);	
		dec_malloc_depth();
		goto done;
	}

	type = get_mapping_type(context);

	if (old == NULL) {
		p = pc_malloc(type, size);	
	} else {
		collect_chunk_level_sample(old);
		p = pc_realloc(type, old, size);	
	}

	p = chunk_level_sampling(p, size, type, ACT_REALLOC, context);

	dec_malloc_depth();

done:
	return p;
}


int
cache_size_under_restrict_mapping() 
{
	return CACHE_REGION_SZ;
}

int
cache_size_under_open_mapping() 
{
	return SHARED_CACHE_SZ;
}

int
cache_line_under_restrict_mapping()
{
	return CACHE_REGION_SZ_IN_LINE;	
}

int
cache_line_under_open_mapping()
{
	return SHARED_CACHE_SZ_IN_LINE;	
}



