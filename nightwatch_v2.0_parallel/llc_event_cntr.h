
#ifndef LLC_EVENT_CNTR_H_
#define LLC_EVENT_CNTR_H_

#include <stdint.h> 


#define LLC_MISS_CNTR_IDX		0
#define LLC_ACCESS_CNTR_IDX		1

#define NR_LLC_PERFEVENT		2	


int llc_event_cntr_init();
int llc_event_cntr_destroy();
int llc_event_cntr_read(uint64_t *cntr_buf, int size);
int llc_event_cntr_start();
int llc_event_cntr_stop();

#endif	/* LLC_EVENT_CNTR_H_ */
