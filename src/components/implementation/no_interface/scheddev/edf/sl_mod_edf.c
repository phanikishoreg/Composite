#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>
#include <heap.h>

#ifdef SL_DEBUG
#define debug printc
#else
#define debug(fmt,...)
#endif

#define SL_EDF_MAX_THDS MAX_NUM_THREADS

struct edf_heap {
	struct heap hs;
	void *data[SL_EDF_MAX_THDS];
	char p;
} edf_heap;

struct heap *hs = (struct heap *)&edf_heap;

void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{ }

struct sl_thd_policy *
sl_mod_schedule(void)
{
	int i;
	struct sl_thd_policy *t;

	assert(heap_size(hs));
	t = heap_peek(hs);
	debug("schedule= peek idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);

	return t;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	assert(t->prio_idx >= 0);

	heap_remove(hs, t->prio_idx);
	t->deadline += t->period;
	t->prio_idx  = -1;
	debug("block= remove idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	assert(t->prio_idx < 0);

	heap_add(hs, t);
	debug("wakeup= add idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{ }

void
sl_mod_thd_create(struct sl_thd_policy *t)
{
	t->period      = 0;
	t->period_usec = 0;
	t->priority    = t->deadline = ~(cycles_t)0;
	t->prio_idx    = -1;
	
	assert((heap_size(hs)+1) < SL_EDF_MAX_THDS);
	heap_add(hs, t);
	debug("create= add idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{ heap_remove(hs, t->prio_idx); }

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	assert(type == SCHEDP_WINDOW);
	t->period_usec = v;
	t->period = sl_usec2cyc(t->period_usec);

	/* first deadline. assuming arrival time = 0 */
	t->deadline = t->priority = t->period;
	heap_adjust(hs, t->prio_idx);
	debug("param_set= adjust idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
}

static int
__compare_min(void *a, void *b)
{ return ((struct sl_thd_policy *)a)->deadline <= ((struct sl_thd_policy *)b)->deadline; }

static void
__update_idx(void *e, int pos)
{ ((struct sl_thd_policy *)e)->prio_idx = pos; }

void
sl_mod_init(void)
{ heap_init(hs, SL_EDF_MAX_THDS, __compare_min, __update_idx); }
