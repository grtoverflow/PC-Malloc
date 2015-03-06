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
#include "nightwatch.h"
#include "locality_profile.h"
#include "stdlib_hook.h"
#include "chunk_monitor.h"
#include "chunk_predictor.h"



#define NightWatch_UNINIT		0
#define NightWatch_ACTIVE		1
#define NightWatch_DESTROYED	2



static int nightwatch_state = NightWatch_UNINIT;

#define tiny_chunk(size) ((size) <= CACHE_LINE_SZ)




static inline void
set_nightwatch_active()
{
	nightwatch_state |= NightWatch_ACTIVE;
}

static inline int
nightwatch_active()
{
	return nightwatch_state & NightWatch_ACTIVE;
}

int
NightWatch_active()
{
	return nightwatch_active();
}


int
NightWatch_init()
{
	int ret;

	disable_cache_management();
	
	ret = 0;
	if (nightwatch_active())
		goto out;
	set_nightwatch_active();

	ret = hash_map_init();
	if (!!ret) goto out;

	ret = install_stdlibapi_hook();
	if (!!ret) goto out;

	ret = chunk_monitor_init();
	if (!!ret) goto out;

	ret = locality_profile_init();
	if (!!ret) goto out;
		
out:
	enable_cache_management();

	if (ret) {
		printf("NightWatch_init Failed!\n");
	} else {
		printf("NightWatch_init Successful!\n");
	}
	return ret;
}

void
NightWatch_destroy()
{
	if (nightwatch_state == NightWatch_DESTROYED)
		return;

	disable_cache_management();

	locality_profile_destroy();

	chunk_monitor_destroy();

	hash_map_destroy();

	nightwatch_state = NightWatch_DESTROYED;

	printf("NightWatch_destroy\n");
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



