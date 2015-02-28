NightWatch
=========
Description
---------
NightWatch is an externsion designed for memory allocator, which is targeting on the resource management of CPU cache.

The traditional design objectives of dynamic memory allocators are about the topics of main memory resource management, for example, improving the efficiency of memory allocation, reducing the memory fragment, etc. However, for most commodity platforms, both CPU cache sets and physical pages are physically indexed. This implies, data’s mapping to the main memory and CPU cache is closely coupled: once the main memory assignment for a piece of data is finished, the data’s mapping to the cache is automatically settled. With this coupling, it is possible that low locality and high-locality data are mapped to the same cache sets, causing cache performance degradation.

From this point of view, it is necessary to integrate cache resource management into dynamic memory allocator. In other words, the dynamic memory allocator should be extended to perform as a dual-memory-layer-manager, which handles main memory allocations, as well as cache memory allocations.

NightWatch is designed targeting on this goal. When integreted with NightWatch, a traditional memory allocator can handle the resource management of cache: once an allocation request arrives, NightWatch quantifies the cache demand, and notifies the memory allocator to allocate memory with proper data-to-cache mapping.


Library Interfaces
The service of NightWatch is transparent to user's application. When integreted with NightWatch, a memory allocator does not need to modify the allocation interfaces. 


Setup
---------
1.	OS kernel update. NightWatch relies on page coloring technique to achieve cache resource allocation. Our kernel patch is under /kernel\_patch. This patch is for the linux kernel "kernel-2.6.32-71.el6". Please see /kernel\_patch/readme.txt for more details.

2.	Install NightWatch library. The source code of NightWatch library is under /nightwatch\_v1.0.
    # make && make install

Syetem Framework
---------
The system framework is illustrated in the following figure. There are three main components: memory manager, locality monitor, and locality predictor.

![image](https://github.com/grtoverflow/PC-Malloc/blob/master/figure/system_design.jpg)
The locality monitor collects locality information from previously allocated chunks. It periodically samples the references to pages of the target chunks, and evaluate the chunk’s locality property, which is sent to the locality predictor. Based on the historical locality information, the locality predictor determines the proper mapping for pending allocation requests. When a new request arrives, the predictor first checks its allocation context, and uses its predecessor chunks’ locality profiles to predict the pending chunk’s locality property. Then, the predictor notifies the memory manager to perform the
allocation.

