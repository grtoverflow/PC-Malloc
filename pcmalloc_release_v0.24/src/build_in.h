#ifndef BUILD_IN_H_
#define BUILD_IN_H_

#ifndef __LINUX_COMPILER_H
#define __LINUX_COMPILER_H
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#endif	/* __LINUX_COMPILER_H */

#define sync_set(p, val) \
__sync_lock_test_and_set(p, val)

#endif /* BUILD_IN_H_ */
