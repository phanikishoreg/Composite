#include <event_trace.h>

#ifndef LINUX_DECODE

#include <cringbuf.h>

#define TRACE_SIZE (1<<22)
#define TRACE_SOUT_THRESH (sizeof(struct event_info) * 16)
/*
 * FIXME:
 * For now, tracing only in this component.
 */
static char            __trace_buf[TRACE_SIZE];
static struct cringbuf __trace_rb;
#define TRACE_RB (&__trace_rb)

void
event_trace_init(void)
{
	memset(__trace_buf, 0, TRACE_SIZE);
	cringbuf_init(TRACE_RB, (void *)__trace_buf, TRACE_SIZE);
}

int
event_trace(struct event_info *ei)
{
	int amnt = 0, len = 0;

	amnt = cringbuf_sz(TRACE_RB);
	/* TODO: should be while (amnt >= thresh) */
	if (unlikely(amnt >= (int)TRACE_SOUT_THRESH)) {
		void *tr = cringbuf_active_extent(TRACE_RB, &len, TRACE_SOUT_THRESH);

		serial_print(tr, TRACE_SOUT_THRESH);
		cringbuf_delete(TRACE_RB, TRACE_SOUT_THRESH);
	}

	cringbuf_produce(TRACE_RB, (void *)ei, sizeof(struct event_info));

	return 0;
}

void
event_decode(void *trace, int sz)
{ assert(0); }

#else 
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

void
event_trace_init(void)
{ assert(0); }

int
event_trace(struct event_info *ei)
{ assert(0); }

void
event_decode(void *trace, int sz)
{
	struct event_info *evt = NULL;
	int curr = 0, eisz = sizeof(struct event_info);

	assert(sz >= eisz);

	do {
		evt = (struct event_info *)(trace + curr);

		switch(evt->type) {
			case KERNEL_EVENT:
				{
					switch(evt->sub_type) {
						case THD_SWITCH_START:
							printf("KERNEL_EVENT THD_SWITCH_START TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case THD_SWITCH_END:
							printf("KERNEL_EVENT THD_SWITCH_END TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case RCV_START:
							printf("KERNEL_EVENT RCV_START TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case RCV_END:
							printf("KERNEL_EVENT RCV_END TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case SCHED_RCV_START:
							printf("KERNEL_EVENT SCHED_RCV_START TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case SCHED_RCV_END:
							printf("KERNEL_EVENT SCHED_RCV_END TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case ASND_START:
							printf("KERNEL_EVENT ASND_START TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case ASND_END:
							printf("KERNEL_EVENT ASND_END TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case SCHED_ASND_START:
							printf("KERNEL_EVENT SCHED_ASND_START TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;
						case SCHED_ASND_END:
							printf("KERNEL_EVENT SCHED_ASND_END TIMESTAMP:%llu THD:%u\n", evt->timestamp, evt->thdid);
							break;

						default: assert(0);
					}
					break;
				}
			default: /* TODO: remaining events */
				assert(0);
		}

		curr += eisz;

	} while (curr < sz);
}

int
main(int argc, char **argv)
{
	int fd = -1;
	void *trace = NULL;
	struct stat tsb;

	if (argc != 2) {
		printf("Usage: %s <input_file_path>\n", argv[0]);
		goto abort;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		goto abort;
	}

	if (fstat(fd, &tsb) > 0) {
		perror("fstat");
		goto error;
	}

	trace = malloc(tsb.st_size);
	if (!trace) {
		printf("malloc failed\n");
		goto error;
	}

	memset(trace, 0, tsb.st_size);
	if (read(fd, trace, tsb.st_size) != tsb.st_size) {
		perror("read");
		goto error;
	}
	close(fd);

	event_decode(trace, tsb.st_size);

	return 0;

error:
	close(fd);
abort:
	exit(1);
}

#endif
