PC-Malloc
=========
Description
---------
PC-Malloc is a memory allocator targeting on the resource allocation across two layers of memory hierarchy --- the main memory and CPU cache.

The traditional design objectives of dynamic memory allocators are about the topics of main memory resource management, for example, improving the efficiency of memory allocation, reducing the memory fragment, etc. However, for most commodity platforms, both CPU cache sets and physical pages are physically indexed. This implies, data’s mapping to the main memory and CPU cache is closely coupled: once the main memory assignment for a piece of data is finished, the data’s mapping to the cache is automatically settled. With this coupling, it is possible that low locality and high-locality data are mapped to the same cache sets, causing cache performance degradation.

From this point of view, it is necessary to integrate cache resource management into dynamic memory allocator. In other words, the dynamic memory allocator should be extended to perform as a dual-memory-layer-manager, which handles main memory allocations, as well as cache memory allocations.

PC-Malloc is a novel allocator that targeting on this goal. Compared with traditional memory allocator, its major difference lies on the management of cache resources: Once an allocation request arrives, PC-Malloc quantifies the demand on both main memory resources and CPU cache resources, and makes the resource allocation on the two memory layers simultaneously to complete the single allocation request.

Library Interfaces
---------
There are two sets of interface available. The first set of interface is the standard allocation interface:
```c
void* malloc(size_t sz);
void* realloc(void *p, size_t newsize);
void* calloc(size_t nmemb, size_t sz);
void free(void *p);
```
For this set of interface, PC-Malloc relies on the user's explicit description to acquire the main memory demand. As to cache resource demand, PC-Malloc transparently predicts whether the currently allocated memory chunk will be referenced in a low-locality manner or not.

Supporting the standard interface in PC-Malloc dues to the following reasons:
Firstly, such interface does not require source code modification, thus will benefit a large base of existed program. Secondly, this will greatly reduce the users’ burden, as locality analysis calls for deep understandings of program’s behavior, which beyond the capacity of most non-expert programmers.

In case there are experts that need to manually control the cache resource allocation, we also provide another set of interface, which support explicit cache demand description.
```c
void* pc_malloc(int type, size_t sz);
void* pc_realloc(int type, void *p, size_t newsize);
void* pc_calloc(int type, size_t nmemb, size_t sz);
void pc_free(void *p);
```
The first parameter "int type" is the description of cache demand. The current version of PC-Malloc supports two types of cache demand, namely RESTRICT\_MAPPING and OPEN\_MAPPING. The RESTRICT\_MAPPING is used for low-locality chunks, and OPEN\_MAPPING is used for high-locality chunks. Please be caution, if RESTRICT\_MAPPING is mistakenly assigned to a high-locality chunk, the chunk’s cache miss may increase, and may significantly decrease system performance. If the user is uncertain about the locality of a given chunk, it is highly recommended to use the standard interface, or set "type" to OPEN\_MAPPING in a conservative way.


Setup
---------
In order to use PC-Malloc, there needs to make two efforts.

1.	OS kernel modification. PC-Malloc relies on page coloring technique to achieve cache resource allocation. Our kernel patch is under /kernel\_patch. This patch is for the linux kernel "kernel-2.6.32-71". Please see /kernel_patch/readme.txt for more details.

2.	Install PC-Malloc library. The source code of PC-Malloc library is under /pcmalloc. Please see /pcmalloc/readme.txt for more details.

Syetem Framework
---------
The system framework is illustrated in the following figure. There are three main components: memory manager, locality monitor, and locality predictor.

![image](https://github.com/grtoverflow/PC-Malloc/blob/master/figure/system_design.jpg)

The memory manager organizes memory into two structures, one for free memory maintenance and the other for guiding cache mapping selection. With the first structure, free memory is organized in four types of containers of different sizes and purposes, in a way similar to the approach of glibc. The main difference is that the memory manager uses two sets of such containers for open mapping and restrictive mapping separately. The second structure targets allocated chunks, which are grouped by allocation context. The chunks’ locality profiles will serve as guidance for future mapping type decision within the same context.

The locality monitor collects locality information from previously allocated chunks. It periodically samples the references to pages of the target chunks, and evaluate the chunk’s locality property, which is sent to the locality predictor. Based on the historical locality information, the locality predictor determines the proper mapping for pending allocation requests. When a new request arrives, the predictor first checks its allocation context, and uses its predecessor chunks’ locality profiles to predict the pending chunk’s locality property. Then, the predictor notifies the memory manager to perform the
allocation.

