PC-Malloc
=========
Description
---------
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
For this set of interface, PC-Malloc relies on the user's explicit description to acquire the main memory demand. As to cache resource demand, PC-Malloc transparently monitors and predicts whether the current allocated memory chunk is referenced in a low-locality manner or not.

Supporting the standard interface dues to the following reasons:
Firstly, such interface does not require source code modification, thus will benefit a large base of existed program. Secondly, this will greatly reduce the users’ burden, as locality analysis calls for deep understandings of program’s behavior, which beyond the capacity of most non-expert programmers.

In case there are experts that need to manually control the cache resource allocation, we also provide another set of interface, which support explicit cache demand description.
```c
void* pc_malloc(int type, size_t sz);
void* pc_realloc(int type, void *p, size_t newsize);
void* pc_calloc(int type, size_t nmemb, size_t sz);
void pc_free(void *p);
```
The first parameter "int type" is the description of cache demand. The current version of PC-Malloc supports two types of cache demand, namely RESTRICT\_MAPPING and OPEN\_MAPPING. The RESTRICT\_MAPPING is used for low-locality chunks, and OPEN\_MAPPING is used for high-locality chunks. Please note that if a high-locality chunk is mistakenly assigned to RESTRICT\_MAPPING, the chunk’s cache miss may increase, and significant decrease system performance. If the user does not sure the locality of certain chunk, it is highly recommended to use the standard interface, or set type to OPEN_MAPPING in a conservative way.


Setup
---------
Syetem Framework
---------
Background of Page coloring
---------
Evaluation on SPEC CPU2006
---------
