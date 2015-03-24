NightWatch
=========
Description
---------
NightWatch is an externsion for memory allocator, which is targeting on the resource management of CPU cache.

The traditional memory allocators are designed focusing on main memory resource management, for example, improving the efficiency of memory allocation, reducing memory fragmentation. However, for most commodity platforms, both CPU cache sets and physical pages are physically indexed. This implies, data’s mapping to the main memory and CPU cache is closely coupled: once the main memory assignment for a piece of data is finished, the data’s mapping to the cache is automatically settled. With this coupling, it is possible that low locality and high-locality data are mapped to the same cache sets, causing cache performance degradation.

From this point of view, it is necessary to integrate cache resource management into dynamic memory allocators. In other words, the dynamic memory allocator should be extended to perform as a dual-memory-layer-manager, which handles main memory allocations, as well as cache memory management.

NightWatch is designed for this goal. When integreted with NightWatch, a traditional memory allocator can handle the resource management of cache: once an allocation request arrives, NightWatch quantifies its cache demand, and notifies the memory allocator to allocate memory with proper data-to-cache mapping.


Target Programs
---------
NightWatch benefits your programs in any of the following cases:

1) Single program cases, where weak-locality data and strong-locality data are accessed in parallel.

2) Multi-thread cases, where weak-locality data and strong-locality data are accessed in parallel.

3) Multi-program cases, where some of the programs pollute the shared cache via accessing weak-locality data, while other programs need sufficient cache space for better performence.

NOTE: NightWatch only focuses on dynamic memory allocations. It does not handle the cache assignment for the data in data segment, bss segment, or stack.


Library Interfaces
---------
The service of NightWatch is transparent to user's application. When integreted with NightWatch, a memory allocator does not need to modify the allocation interfaces. 


Setup
---------
1.	OS kernel update. NightWatch relies on page coloring technique to achieve cache resource allocation. Our kernel patch is under /kernel\_patch. The patch is for the linux kernel "kernel-2.6.32-71.el6". See /kernel\_patch/readme.txt for more details.

2.	Install PAPI. You can find the latest version of PAPI at http://icl.cs.utk.edu/papi/.

2.	Install NightWatch library. The source code of NightWatch is under /nightwatch\_v1.0.

3.	Modify memory allocator. If you are an allocator developer, and you may want to integrate NightWatch into your own memory allocator. Then you need to implement the interfaces defined in allocator.h. In this project, we have integrated NightWatch into tcmalloc. You can take the modified allocator (under /gperftools-2.4\_NW_externed\_v2.0) as example. Or if you just want to try a cache-aware allocator, the allocator can be directly used without further modification. To use the allocator, you need to relink your application with flag -ltcmalloc. For more detailed information, see /gperftools-2.4\_NW_externed\_v2.0/readme.txt.

Syetem Framework
---------
The system framework is illustrated in the following figure. There are three main components: memory manager, locality monitor, and locality predictor.

![image](https://github.com/grtoverflow/PC-Malloc/blob/master/figure/system_design.jpg)
The locality monitor collects locality information from previously allocated chunks. It periodically samples the references to pages of the target chunks, and evaluate the chunk’s locality property, which is sent to the locality predictor. Based on the historical locality information, the locality predictor determines the proper mapping for pending allocation requests. When a new request arrives, the predictor first checks its allocation context, and uses its predecessor chunks’ locality profiles to predict the pending chunk’s locality property. Then, the predictor notifies the memory manager to perform the
allocation.

