#ifndef PERF_EVENT_H_
#define PERF_EVENT_H_

#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>


struct perf_event_attr {

	uint32_t			type;
	uint32_t			size;
	uint64_t			config;

	union {
		uint64_t		sample_period;
		uint64_t		sample_freq;
	};

	uint64_t			sample_type;
	uint64_t			read_format;

	uint64_t	disabled       :  1, /* off by default        */
				inherit	       :  1, /* children inherit it   */
				pinned	       :  1, /* must always be on PMU */
				exclusive      :  1, /* only group on PMU     */
				exclude_user   :  1, /* don't count user      */
				exclude_kernel :  1, /* ditto kernel          */
				exclude_hv     :  1, /* ditto hypervisor      */
				exclude_idle   :  1, /* don't count when idle */
				mmap           :  1, /* include mmap data     */
				comm	       :  1, /* include comm data     */
				freq           :  1, /* use freq, not period  */
				inherit_stat   :  1, /* per task counts       */
				enable_on_exec :  1, /* next exec enables     */
				task           :  1, /* trace fork/exit       */
				watermark      :  1, /* wakeup_watermark      */
				/*
				 * precise_ip:
				 *
				 *  0 - SAMPLE_IP can have arbitrary skid
				 *  1 - SAMPLE_IP must have constant skid
				 *  2 - SAMPLE_IP requested to have 0 skid
				 *  3 - SAMPLE_IP must have 0 skid
				 *
				 *  See also PERF_RECORD_MISC_EXACT_IP
				 */
				precise_ip     :  2, /* skid constraint       */

				__reserved_1   : 47;

	union {
		uint32_t		wakeup_events;	  /* wakeup every n events */
		uint32_t		wakeup_watermark; /* bytes before wakeup   */
	};

	union {
		/* break point */
		struct {
			uint32_t	bp_type;
			uint64_t	bp_addr; 
			uint64_t	bp_len;
		};

		/* specific event */
		struct {
			uint64_t	spec_hw_event_addr;
			uint64_t	spec_hw_config_addr; 
			/* 
			 * 0 : 7    low bit of config mask
			 * 8 : 15   high bit of config mask
			 * 16 : 23  low bit of event mask
			 * 24 : 31  high bit of event mask
			 */
			uint64_t	spec_hw_mask_bits; 
			uint64_t	spec_hw_disable_config; 
		};
	};
};

/* attr.type */
enum perf_type_id {
	PERF_TYPE_HARDWARE          = 0,
	PERF_TYPE_SOFTWARE          = 1,
	PERF_TYPE_TRACEPOINT        = 2,
	PERF_TYPE_HW_CACHE          = 3,
	PERF_TYPE_RAW               = 4,
	PERF_TYPE_BREAKPOINT        = 5,
	PERF_TYPE_RAW_SPEC          = 6,
	PERF_TYPE_MAX,
};

/* perf_events ioctl commands, use with event fd */
#define PERF_EVENT_IOC_ENABLE       _IO ('$', 0)
#define PERF_EVENT_IOC_DISABLE      _IO ('$', 1)
#define PERF_EVENT_IOC_REFRESH      _IO ('$', 2)
#define PERF_EVENT_IOC_RESET        _IO ('$', 3)
#define PERF_EVENT_IOC_PERIOD       _IOW('$', 4, uint64_t)
#define PERF_EVENT_IOC_SET_OUTPUT   _IO ('$', 5)
#define PERF_EVENT_IOC_SET_FILTER   _IOW('$', 6, char *)
#define PERF_EVENT_IOC_ENABLE       _IO ('$', 0)


#define __NR_perf_event_open	298 

#define sys_perf_event_open(attr, pid, cpu, group_fd, flags) \
syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags)

/* event list */
enum event_id {
	PERF_EVENT_L1_DCM = 0,		/* level 1 data cache miss, demand and prefetch */
	PERF_EVENT_L2_TCM,			/* level 2 cache miss, demand and prefetch */
	PERF_EVENT_L3_TCM,			/* level 3 cache miss, demand and prefetch */
	PERF_EVENT_L3_TCA,			/* level 3 cache access, demand and prefetch */
	PERF_EVENT_MAX,
};

/* flag bits */
#define PERF_ATTR_DISABLED	0x1
#define PERF_ATTR_INHERIT	0x2
#define PERF_ATTR_PINNED	0x4
#define PERF_ATTR_EXCLUSIVE	0x8
#define PERF_ATTR_EXCL_USER	0x10
#define PERF_ATTR_EXCL_KERL 0x20
#define PERF_ATTR_EXCL_HV	0x40
#define PERF_ATTR_EXCL_IDLE	0x80

/* read format */
enum perf_event_read_format {
	PERF_FORMAT_TOTAL_TIME_ENABLED      = 1U << 0,
	PERF_FORMAT_TOTAL_TIME_RUNNING      = 1U << 1,
	PERF_FORMAT_ID              = 1U << 2,
	PERF_FORMAT_GROUP           = 1U << 3,

	PERF_FORMAT_MAX = 1U << 4,      /* non-ABI */
};

int perf_event_attr_setup(struct perf_event_attr *attr, int event_id, 
		uint64_t flag, uint64_t read_format);

#endif /* PERF_EVENT_H_ */
