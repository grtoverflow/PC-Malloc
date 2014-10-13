#ifndef TIME_EVENT_QUEUE_H_
#define TIME_EVENT_QUEUE_H_

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

#include "config.h"
#include "build_in.h"


/* timer resolution in usecond */
#define TIMER_RESOLUTION 50

static inline void
usec2tv(struct timeval *t, uint64_t usec) 
{
	t->tv_usec = usec % 1000000;
	t->tv_sec = usec / 1000000;
}

static inline uint64_t
tv2usec(struct timeval *t)
{
	return t->tv_sec * 1000000 + t->tv_usec;
}

static inline int
tv_cmp(struct timeval *t0, struct timeval *t1) 
{
	return t0->tv_sec > t1->tv_sec ?
		1 : t0->tv_sec < t1->tv_sec ?
		-1 : t0->tv_usec > t1->tv_usec ?
		1 : t0->tv_usec < t1->tv_usec ?
		-1 : 0;
}

static inline void
tv_add(struct timeval *t0, struct timeval *t1)
{
	t0->tv_usec += t1->tv_usec;
	t0->tv_sec += t1->tv_sec;

	if(unlikely(t0->tv_usec >= 1000000)) {
		t0->tv_sec += t0->tv_usec / 1000000;
		t0->tv_usec %= 1000000;
	}
}

static inline void
tv_sub(struct timeval *t0, struct timeval *t1)
{
	if(unlikely(t0->tv_usec < t1->tv_usec)) {
		t0->tv_usec += 1000000 - t1->tv_usec;
		t0->tv_sec -= t1->tv_sec + 1;
	} else {
		t0->tv_usec -= t1->tv_usec;
		t0->tv_sec -= t1->tv_sec;
	}
}

typedef void(*time_event_handler)(void*, struct timeval*);

int time_event_queue_init();
void time_event_queue_destroy();

void pend_time_event_queue();
void resume_time_event_queue();

void add_time_event(struct timeval *time, time_event_handler handler, void *private);
void remove_time_event(void *private); 

#endif /* TIME_EVENT_QUEUE_H_ */



