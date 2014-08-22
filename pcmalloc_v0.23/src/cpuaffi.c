#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#define __USE_GNU

#include <sched.h>
#include <ctype.h>

#include "config.h"
#include "build_in.h"


int 
set_cpu_affinity(int id)
{
	int total_num;
	cpu_set_t mask;

	total_num  = sysconf(_SC_NPROCESSORS_CONF);

	assert(id >= 0 && id < total_num);

	CPU_ZERO(&mask);
	CPU_SET(id, &mask);

	return sched_setaffinity(0, sizeof(mask), &mask);
}

int
get_cpu_affinity()
{
	int i, total_num;
	cpu_set_t mask;

	total_num  = sysconf(_SC_NPROCESSORS_CONF);
	CPU_ZERO(&mask);

	if (sched_getaffinity(0, sizeof(mask), &mask) == -1) {
		return -1;
	}

	for(i = 0; i < total_num; i++){
		if(CPU_ISSET(i, &mask)){
			return i;
		}
	}	

	return -1;
}

