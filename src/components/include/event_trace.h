#ifndef EVENT_TRACE_H
#define EVENT_TRACE_H

typedef enum {
	KERNEL_EVENT,
	SL_EVENT,
	
	QUEUE_EVENT,
	MUTEX_EVENT,
	FILE_SYS_EVENT,
} event_type_t;

typedef unsigned short event_sub_type_t;

enum kernel_event_type {
	THD_SWITCH_START,
	THD_SWITCH_END,

	RCV_START,
	RCV_END,

	SCHED_RCV_START,
	SCHED_RCV_END,

	ASND_START,
	ASND_END,

	SCHED_ASND_START,
	SCHED_ASND_END,

	/* TODO: remaining cos_kernel_api */
};

enum sl_event_type {
	SL_BLOCK_START,
	SL_BLOCK_END,
	SL_BLOCK_TIMEOUT_START,
	SL_BLOCK_TIMEOUT_END,

	SL_YIELD_START,
	SL_YIELD_END,

	SL_WAKEUP_START,
	SL_WAKEUP_END,
};

struct event_info {

	event_type_t       type;
	event_sub_type_t   sub_type;
	unsigned long long timestamp;
	unsigned short     thdid;
	unsigned short     objid; /* TODO */

	/* TODO: operation info */
	void *optional;
};

void event_trace_init(void);
/*
 *@return 0 - successful, 1 - failed to trace event
 */
int event_trace(struct event_info *ei);
void event_decode(void *trace, int sz);

/*
 * TODO: 
 * 1. optimized API to allow compiler optimizations.
 * 2. macro API so the tracing feature can be disabled.
 */

#endif /* EVENT_TRACE_H */
