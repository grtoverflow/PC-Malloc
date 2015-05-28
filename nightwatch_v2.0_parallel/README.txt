Dependency:
Before you use NightWatch, please make sure PAPI has been installed.
You can get the latest version of PAPI at http://icl.cs.utk.edu/papi/.

Config NightWatch:
Modify 'config.h' according to your system configuration.
You should modify the following values:
CACHE_LINE_SZ		cache line size of the last level cache
CACHE_LINE_SHIFT	bit width of cache line size
NR_CACHE_LEVEL		# of levels of the cache hierarchy.
L1D_SZ_IN_LINE		# of cache lines in L1 data cache
L2_SZ_IN_LINE		# of cache lines in L2 cache
L3_SZ_IN_LINE		# of cache lines in L3 cache
L2_MISS_EVENT_NAME		L2 miss event name in PAPI
L3_MISS_EVENT_NAME		L3 miss event name in PAPI
L3_ACCESS_EVENT_NAME	L3 access event name in PAPI

Install:
make
make install



The following designs may need further study:
1. Should the threads in a single process share allocation contexts?
   If the memory chunks of different threads allocated from the same allocation context share the similar access locality, NightWatch will get better predition accuracy with less overhead.
2. The remapping may introduce fragmentations. If this is a critical problem, then how to make the tradeoff between pollution control and fragmentation? 

