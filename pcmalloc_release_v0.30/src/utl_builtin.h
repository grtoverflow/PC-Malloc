#ifndef UTL_BUILTIN_H_
#define UTL_BUILTIN_H_


#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#endif	/* __LINUX_COMPILER_H */


#define sync_set(p, val) \
__sync_lock_test_and_set(p, val)


/* Returns the number of leading 0-bits in x, 
 * starting at the most significant bit position. 
 * If x is 0, the result is undefined. */
#define clz(x) __builtin_clz(x)
/* Similar to clz, except the argument type is unsigned long. */
#define clzl(x) __builtin_clzl(x)
/* Returns the index of the most significant 1-bit of x.
 * IF x is 0, return -1 */
#define bsf(x) ((sizeof(unsigned int) << 3) - clz(x) - 1)
/* Similar to bsf, except the argument type is unsigned long. */
#define bsfl(x) ((sizeof(unsigned long) << 3) - clzl(x) - 1)
/* Returns one plus the index of the 
 * least significant 1-bit of x, or if x is zero, 
 * returns zero. */
#define ffs(x) __builtin_ffs(x)
/* Similar to ffs, except the argument type is unsigned long. */
#define ffsl(x) __builtin_ffsl(x)
/* Returns the number of 1-bits in x. */
#define popcount(x) __builtin_popcount(x)
/* Similar to popcount, except the argument type is unsigned long. */
#define popcountl(x) __builtin_popcountl(x)


#define __aligned(x) __attribute__((aligned(x)))


#endif /* UTL_BUILTIN_H_ */
