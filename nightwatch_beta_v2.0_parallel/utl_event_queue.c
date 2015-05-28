#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "utl_config.h"
#include "utl_list.h"
#include "utl_hash_map.h"
#include "utl_event_queue.h"
#include "utl_wrapper.h"


/* timer config */
#define TIMER_TYPE	ITIMER_REAL
#define TIMER_SIG_TYPE	SIGALRM

/* event queue state */
#define EVENT_QUEUE_EMPTY	0
#define EVENT_QUEUE_PENDING	1
#define EVENT_QUEUE_ACTIVE	2


struct event {
	time_event_handler handler;
	void *private;
	struct timeval start_time;
	struct timeval trigger_time;
	struct list_head p;
};

struct time_event_queue {
	volatile uint32_t state;
	int nr_event;
	struct list_head events;
};


static struct time_event_queue event_queue; 
static struct list_head free_event;

static struct itimerval itv;
static struct timeval timer_resolution;

static struct hash_map *private2event_map;

static utl_spinlock_t queue_lock;


static inline struct event*
time_event_alloc()
{
	struct event *event;

	if(list_empty(&free_event)) {
		event = (struct event*)	
			internal_malloc(OPEN_MAPPING, sizeof(struct event));
#ifdef USE_ASSERT
		assert(!!event);
#endif /* USE_ASSERT */
	} else {
		event = next_entry(&free_event, struct event, p);	
		list_del(&event->p);
	}

	return event;
}

static inline void
time_event_free(struct event *event)
{
	list_del(&event->p);
	list_add(&event->p, &free_event);
}

static inline void
time_event_del(struct event *event)
{
	list_del(&event->p);
}

static inline void
set_timer(struct timeval *time)
{
	itv.it_value = *time;
	setitimer(TIMER_TYPE, &itv, NULL);
}

/* must be called after do_pend_event_queue() */
static inline void
do_trigger_time_event(struct timeval now)
{
	struct event *triggered;
	struct list_head *events;

	tv_add(&now, &timer_resolution);
	events = &event_queue.events;

	while (!list_empty(events)){
		triggered = next_entry(events, struct event, p);
		if (tv_cmp(&now, &triggered->trigger_time) != -1) {
			hash_map_delete_member(private2event_map, 
					(uint64_t)triggered->private);
			time_event_del(triggered);
			if (likely(triggered->handler != NULL)) {
				triggered->handler(triggered->private, &now);
			}
			time_event_free(triggered);
		} else {
			break;	
		}
	}
}

static inline uint32_t
do_pend_event_queue()
{
	uint32_t state;
	struct timeval tv;

	state = sync_set(&event_queue.state, EVENT_QUEUE_PENDING);

	/* the queue is already pended */
	if (unlikely(state == EVENT_QUEUE_PENDING))
		return state;
	
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	set_timer(&tv);

	return state;
}

static inline void
do_resume_event_queue(struct timeval *now)
{
	uint32_t state;
	struct timeval tv0, tv1;
	struct event *event;

	state = sync_set(&event_queue.state, EVENT_QUEUE_ACTIVE);

	/* the queue is active */
	if (unlikely(state == EVENT_QUEUE_ACTIVE))
		return;

	if (unlikely(now == NULL))
		gettimeofday(&tv0, NULL);
	else
		tv0 = *now;
	do_trigger_time_event(tv0);

	/* the time event queue is empty */
	if (unlikely(list_empty(&event_queue.events)))
		return;

	event = next_entry(&event_queue.events, struct event, p);
	tv1 = event->trigger_time;

#ifdef UTL_USE_ASSERT
	assert(tv_cmp(&tv1, &tv0) != -1);
#endif /* UTL_USE_ASSERT */

	tv_sub(&tv1, &tv0);
	set_timer(&tv1);
}

static inline void
do_add_time_event(struct timeval *tv, time_event_handler handler, 
			void *private)
{
	struct event *event, *event_iter;
	struct timeval now, trigger_tv;
	int state;

	state = do_pend_event_queue();

	gettimeofday(&now, NULL);	
	trigger_tv = now;
	tv_add(&trigger_tv, tv);

	event = time_event_alloc();
	event->start_time = now;
	event->trigger_time = trigger_tv;
	event->handler = handler;
	event->private = private;

	list_for_each_entry(event_iter, &event_queue.events, p) {
		if (tv_cmp(&trigger_tv, &event_iter->trigger_time) == -1)
			break;
	}

	hash_map_add_member(private2event_map, (uint64_t)private, event);
	list_add(&event->p, event_iter->p.prev);

	do_trigger_time_event(now);

	/* do not resume event queue if add_time_event
	 * is called during pending state */
	if (state == EVENT_QUEUE_ACTIVE 
			|| state == EVENT_QUEUE_EMPTY) {
		do_resume_event_queue(&now);
	}
}


void
add_time_event_when_pending(struct timeval *tv, time_event_handler handler, 
			void *private)
{
	do_add_time_event(tv, handler, private);
}


void
add_time_event(struct timeval *tv, time_event_handler handler, 
			void *private)
{
	utl_spin_lock(&queue_lock);
	do_add_time_event(tv, handler, private);
	utl_spin_unlock(&queue_lock);
}


static inline void
do_remove_time_event(void *private) 
{
	struct event *event;

	do_pend_event_queue();

	event = (struct event *)
		hash_map_find_member(private2event_map, (uint64_t)private);

	if (unlikely(event == NULL)) {
		return;
	}
	
	time_event_free(event);
	hash_map_delete_member(private2event_map, (uint64_t)private);

	if (list_empty(&event_queue.events)) {
		event_queue.state = EVENT_QUEUE_EMPTY;
	} else {
		do_resume_event_queue(NULL);
	}
}

void
remove_time_event(void *private) 
{
	utl_spin_lock(&queue_lock);
	do_remove_time_event(private); 
	utl_spin_unlock(&queue_lock);
}
	
void
remove_time_event_when_pending(void *private) 
{
	do_remove_time_event(private); 
}
	

static void
trigger_time_event(int signo)
{
	struct timeval now;
	uint32_t state;

	/* we are trapped in event_queue processing context, try later */
	if (utl_spin_trylock(&queue_lock) != 0)
		return;

	state = do_pend_event_queue();
	if (state == EVENT_QUEUE_PENDING) {
		utl_spin_unlock(&queue_lock);
		return;
	}

	gettimeofday(&now, NULL);	

	do_trigger_time_event(now);

	if (list_empty(&event_queue.events)) {
		event_queue.state = EVENT_QUEUE_EMPTY;
	} else {
		tv_sub(&now, &timer_resolution);
		do_resume_event_queue(&now);
	}

	utl_spin_unlock(&queue_lock);
}

int 
time_event_queue_init()
{
	struct sigaction sa;
	int ret;

	event_queue.state = EVENT_QUEUE_EMPTY;
	event_queue.nr_event = 0;
	list_init(&event_queue.events);
	list_init(&free_event);

	itv.it_interval.tv_usec = 0;
	itv.it_interval.tv_sec = 0;
	usec2tv(&timer_resolution, TIMER_RESOLUTION);

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, TIMER_SIG_TYPE);

	sa.sa_flags = 0;
	sa.sa_handler = trigger_time_event;
	ret = sigaction(TIMER_SIG_TYPE, &sa, NULL);

	private2event_map =  new_hash_map();
	utl_spinlock_init(&queue_lock, UTL_PROCESS_SHARED);

	return ret;
}

void
time_event_queue_destroy()
{
	struct list_head *event_list;
	struct event *event;

	do_pend_event_queue();

	event_list = &event_queue.events;
	while (!list_empty(event_list)) {
		event = next_entry(event_list, struct event, p);
		list_del(&event->p);
		internal_free(event);
	}

	event_list = &free_event;
	while (!list_empty(event_list)) {
		event = next_entry(event_list, struct event, p);
		list_del(&event->p);
		internal_free(event);
	}

	delete_hash_map(private2event_map);
	utl_spinlock_destroy(&queue_lock);
}

void
pend_time_event_queue()
{
	utl_spin_lock(&queue_lock);
	do_pend_event_queue();
}

void
resume_time_event_queue()
{
	do_resume_event_queue(NULL);
	utl_spin_unlock(&queue_lock);
}


