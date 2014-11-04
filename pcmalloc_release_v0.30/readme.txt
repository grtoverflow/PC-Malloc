Usage:

1.install papi
 1) get papi at http://icl.cs.utk.edu/papi/software/index.html, and install.
 2) find the native events of l2 cache miss, l3 cache miss, and l3 cache access.
 3) config the event names in ${pcmalloc_dir}/src/conifg.h according to the ones you have found in 2).
    where the event names are
    L2_MISS_EVENT_NAME
    L3_MISS_EVENT_NAME
    L3_ACCESS_EVENT_NAME
2.pcmalloc configuration
 1) check your system's cache architecture in /sys/devices/system/cpu/,
 2) modify the cache size descriptions in ${pcmalloc_dir}/src/conifg.h:
    L1D_SZ_IN_LINE
    L2_SZ_IN_LINE
    L3_SZ_IN_LINE
    note that all the cache sizes are in cache line granularity.
3.install pcmalloc
 $ make
 $ make install



