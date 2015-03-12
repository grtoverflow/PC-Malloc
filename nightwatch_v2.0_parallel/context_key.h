
#include <stdint.h>
#include "config.h"


static __thread uint64_t call_stack[TOTAL_CALL_STACK_DEPTH];



#ifdef USE_QUICK_BACKTRACE 

/* This implementation assumes a stack layout that matches the defaults
   used by gcc's `__builtin_frame_address' and `__builtin_return_address'
   (FP is the frame pointer register):

	  +-----------------+     +-----------------+
    FP -> | previous FP --------> | previous FP ------>...
	  |                 |     |                 |
	  | return address  |     | return address  |
	  +-----------------+     +-----------------+

  */

/* This is the APCS stack backtrace structure.  */
struct layout
{
  struct layout *next;
  void *sp;
  void *return_address;
};


/* Get some notion of the current stack.  Need not be exactly the top
   of the stack, just something somewhere in the current frame.  */
#ifndef CURRENT_STACK_FRAME
# define CURRENT_STACK_FRAME  ({ char __csf; &__csf; })
#endif

/* By default we assume that the stack grows downward.  */
#ifndef INNER_THAN
# define INNER_THAN <
#endif

/* By default assume the `next' pointer in struct layout points to the
   next struct layout.  */
#ifndef ADVANCE_STACK_FRAME
# define ADVANCE_STACK_FRAME(next) ((struct layout *) (next))
#endif

/* By default, the frame pointer is just what we get from gcc.  */
#ifndef FIRST_FRAME_POINTER
# define FIRST_FRAME_POINTER  __builtin_frame_address (0)
#endif

#ifndef OUTER_FRAME_POINTER
# define OUTER_FRAME_POINTER \
__builtin_frame_address (INNER_CALL_STACK_DEPTH + 1)
#endif


static int
malloc_context_backtrace()
{
	struct layout *current;
	void *top_frame;
	void *top_stack;
	int depth = INNER_CALL_STACK_DEPTH + 1;
	
	top_frame = OUTER_FRAME_POINTER;
	top_stack = CURRENT_STACK_FRAME;
	
	/* We skip the call to this function, it makes no sense to record it.  */
	current = ((struct layout *) top_frame);

	while (depth < size)
	{
		/* This means the address is out of range. */
		if ((void *) current INNER_THAN top_stack
			|| (long) current <= 0 || current->sp == NULL)
			break;

		call_stack[depth++] = current->sp;
		current = ADVANCE_STACK_FRAME (current->next);
	}

	return depth;
}

#endif /* USE_QUICK_BACKTRACE */




#ifdef USE_GLIBC_BACKTRACE

#include <execinfo.h>

#define malloc_context_backtrace() \
backtrace((void **)call_stack, TOTAL_CALL_STACK_DEPTH)

#endif /* USE_GLIBC_BACKTRACE */



static uint64_t
get_context_key()
{
	int depth;
	uint64_t key;
	int i, offset;

	depth = malloc_context_backtrace();

	key = 0;
	offset = 0;
	i = INNER_CALL_STACK_DEPTH;

	while (i < depth) {
		key ^= (call_stack[i] << offset) 
		       | (call_stack[i] >> (64 - offset));
		offset = (offset + 16) % 64;
		i++;
	}

	return key;
}


