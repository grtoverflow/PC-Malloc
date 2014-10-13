#include <errno.h>
#include <string.h>

#include "perf_event.h"


/* we currently support Intel Xeon E7400 processor */
int 
perf_event_attr_setup(struct perf_event_attr *attr, int event_id, 
		uint64_t flag, uint64_t read_format)
{
	if (event_id < 0 || event_id >= PERF_EVENT_MAX) 
		return -EINVAL; 
	memset(attr, 0, sizeof(struct perf_event_attr));

	switch (event_id) {
	case PERF_EVENT_L1_DCM:
		attr->type = PERF_TYPE_RAW;
		attr->config = 0x530f45;
		break;
	case PERF_EVENT_L2_TCM:
		attr->type = PERF_TYPE_RAW;
		attr->config = 0x53f024;
		break;
	case PERF_EVENT_L3_TCM:
		attr->type = PERF_TYPE_RAW;
		attr->config = 0x53412e;
		break;
	case PERF_EVENT_L3_TCA:
		attr->type = PERF_TYPE_RAW;
		attr->config = 0x534f2e;
		break;
	default:
		break;
	}

	attr->disabled = flag & PERF_ATTR_DISABLED ? 1 : 0;
	attr->inherit = flag & PERF_ATTR_INHERIT ? 1 : 0;
	attr->pinned = flag & PERF_ATTR_PINNED ? 1 : 0;
	attr->exclusive = flag & PERF_ATTR_EXCLUSIVE ? 1 : 0;
	attr->exclude_user = flag & PERF_ATTR_EXCL_USER ? 1 : 0;
	attr->exclude_kernel = flag & PERF_ATTR_EXCL_KERL ? 1 : 0;
	attr->exclude_hv = flag & PERF_ATTR_EXCL_HV ? 1 : 0;
	attr->exclude_idle = flag & PERF_ATTR_EXCL_HV ? 1 : 0;

	attr->read_format = read_format;

	return 0;
}



