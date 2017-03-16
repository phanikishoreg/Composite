#ifndef SPINNER_H
#define SPINNER_H

#include <sl.h>

static inline void
spin_usecs(microsec_t usecs)
{
	cycles_t cycs = sl_usec2cyc(usecs);
	cycles_t now;

	rdtscll(now);
	cycs += now;

	while (now < cycs) rdtscll(now);
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

#endif /* SPINNER_H */
