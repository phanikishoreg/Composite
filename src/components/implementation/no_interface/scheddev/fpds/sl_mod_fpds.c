#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#define SL_FPDS_NPRIOS  32
#define SL_FPDS_HIGHEST 0
#define SL_FPDS_LOWEST  (SL_FPDS_NPRIOS-1)

#define SL_FPDS_USEC_MIN 1
#define SL_FPDS_USEC_MAX 10000

struct ps_list_head threads[SL_FPDS_NPRIOS];

/* No RR yet */
void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{
	if (!cycles) return;

	if (t->budget) {
		assert(cycles < (cycles_t)TCAP_RES_MAX);
		t->expended += (tcap_res_t)cycles;

		if (t->expended >= t->budget) sl_mod_yield(t, NULL);
	}
}

struct sl_thd_policy *
sl_mod_schedule(void)
{
	int i;

	for (i = 0 ; i < SL_FPDS_NPRIOS ; i++) {
		struct sl_thd_policy *t;
		struct sl_thd *td;
		cycles_t now;

		if (ps_list_head_empty(&threads[i])) continue;
		t = ps_list_head_first_d(&threads[i], struct sl_thd_policy);

		rdtscll(now);
		td = sl_mod_thd_get(t);
		assert(td);

		/* if this thread is using DS budget server and we have reached/passed replenishment period */
		if (t->budget && ((t->last_period == 0) || (t->last_period && (t->last_period + t->period <= now)))) {
			t->last_period = now;
			td->budget     = t->budget;
			t->expended    = 0;
		}

		return t;
	}
	assert(0);

	return NULL;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	ps_list_rem_d(t);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	/*
	 * TODO:
	 * there are different reasons for getting a number of wakeup events.. 
	 * 1. cos_asnd on a blocked thread, every such asnd will add a unblocked event to the sched.
	 * 2. expending tcap-budget, will get a unblocked (not really that) event to the sched.
	 */
	sl_mod_yield(t, NULL);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{
	assert(t->priority <= SL_FPDS_LOWEST);

	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[t->priority], t);
}

void
sl_mod_thd_create(struct sl_thd_policy *t)
{
	t->priority    = SL_FPDS_LOWEST;
	t->period      = 0;
	t->period_usec = 0;
	t->budget      = 0;
	t->expended    = 0;
	t->budget_usec = 0;
	t->last_period = 0;
	ps_list_init_d(t);
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{ ps_list_rem_d(t); }

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	/*
	 * TODO: Gets a bit tricky on the order of the params set to validate
	 *       For now, assume the order param_set(BUDGET)->param_set(WINDOW)->param_set(PRIO)
	 *
	 *       Also, it doesn't have to be DS. If you decide to ignore budget/period param_set, 
	 *       that makes the thread simple FPRR. 
	 */
	switch (type) {
	case SCHEDP_PRIO:
	{
		assert(v < SL_FPDS_NPRIOS);
		ps_list_rem_d(t); /* if we're already on a list, and we're updating priority */
		t->priority = v;
		ps_list_head_append_d(&threads[t->priority], t);

		break;
	}
	case SCHEDP_WINDOW:
	{
		assert(v >= SL_FPDS_USEC_MIN && v <= SL_FPDS_USEC_MAX);
		assert (v >= t->budget_usec);
		t->period_usec = v;
		t->period = sl_usec2cyc(v);

		break;
	}
	case SCHEDP_BUDGET:
	{
		assert(v >= SL_FPDS_USEC_MIN && v <= SL_FPDS_USEC_MAX);
		t->budget_usec = v;
		t->budget = (tcap_res_t)sl_usec2cyc(v);

		break;
	}
	default: assert(0);
	}
}

void
sl_mod_init(void)
{
	int i;
	struct sl_thd *t;

	for (i = 0 ; i < SL_FPDS_NPRIOS ; i++) {
		ps_list_head_init(&threads[i]);
	}
}
