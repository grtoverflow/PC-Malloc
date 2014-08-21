PC-Malloc
=========
Description
---------
The traditional design objectives of dynamic memory allocators are about the topics of main memory resource management, for example, improving the efficiency of memory allocation, reducing the memory fragment, etc. However, for most commodity platforms, both CPU cache sets and physical pages are physically indexed. This implies, data’s mapping to the main memory and CPU cache is closely coupled: once the main memory assignment for a piece of data is finished, the data’s mapping to the cache is automatically settled. With this coupling, it is possible that low locality and high-locality data are mapped to the same cache sets, causing cache performance degradation.

From this point of view, it is necessary to integrate cache resource management into dynamic memory allocator. In other words, the dynamic memory allocator should be extended to perform as a dual-memory-layer-manager, which handles main memory allocations, as well as cache memory allocations.

PC-Malloc is a novel allocator that targeting on this goal. Compared with traditional memory allocator, its major difference lies on the management of cache resources: Once an allocation request arrives, PC-Malloc quantifies the demand on both main memory resources and CPU cache resources, and makes the resource allocation on the two memory layers simultaneously to complete the single allocation request.


Setup
---------
Syetem Framework
---------
Background of Page coloring
---------
Evaluation on SPEC CPU2006
---------
