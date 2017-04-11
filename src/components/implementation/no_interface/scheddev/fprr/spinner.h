#ifndef SPINNER_H
#define SPINNER_H

#include <sl.h>

#define CALIB_ITERS 10

static inline void
spin_usecs(microsec_t usecs)
{
	cycles_t cycs = sl_usec2cyc(usecs), cycs0;
	cycles_t now;

	cycs0 = cycs;
	cycles_t exec_now = sl_exec_cycles(), exec_end, end_act;
	exec_end += cycs;
	rdtscll(now);
	cycs += now;

	while (now < cycs) rdtscll(now);
	end_act = sl_exec_cycles();

	if (end_act < exec_end) {
		// haven't spinned as long..
		// another attempt at it..
		rdtscll(now);
		cycs = now + cycs0 - (exec_end - end_act);
			
		while (now < cycs) rdtscll(now);
	}
}

/*
 * returns 1 if deadline missed.
 *         0 if made.
 */
static inline int
spin_usecs_dl(microsec_t usecs, cycles_t dl)
{
	cycles_t now;

	spin_usecs(usecs);
	rdtscll(now);

	if (now > dl) return 1;

	return 0;
}

/*
 * Calibrates or returns the iters_per_usec.
 * Caution: Call this at the bootup phase for the first time. 
 *	    Call to this any other time, can make it inaccurate with preemptions, etc!
 */
static inline int
spin_iters_per_usec(void)
{
	static int iters_per_usec = 0;
	int iter = 0;
	int avg_iters = 0, total_iters = 0;
	cycles_t cycs = sl_usec2cyc(1); /* cycs per usec */

	if (iters_per_usec) goto done;

	while (iter < CALIB_ITERS) {
		int this_iters = 0;
		cycles_t now, end;

		rdtscll(now);
		end = cycs + now;

		while (now < end) {
			this_iters ++;
			rdtscll(now);
		}

		total_iters += this_iters;
		iter ++;
	}

	avg_iters = total_iters / CALIB_ITERS;
	iters_per_usec = avg_iters;
	
done:
	return iters_per_usec;
}

#endif /* SPINNER_H */
