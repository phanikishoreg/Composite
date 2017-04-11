#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#ifdef SL_DEBUG_DEADLINES
#define sl_mod_print printc
static unsigned long long dl_missed = 0;
static cycles_t prev = 0;
static thdid_t thd_start = 0, thd_end = 0;
#else
#define sl_mod_print(fmt,...)
#endif

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

		sl_print("M:%u\n", td->thdid);
		return t;
	}

	return NULL;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	sl_print("B%u", sl_mod_thd_get(t)->thdid);
	if (t->blocked) return;

	t->blocked = 1;
#ifdef SL_DEBUG_DEADLINES
	cycles_t now;
	static long print_counter = 0, child_miss_count = 0;

	rdtscll(now);

	if (now < t->deadline) {
		t->made ++;
	} else {
		t->missed ++;
		dl_missed ++;
//		child_miss_count ++;
//		if (child_miss_count > 100) while (1);
//		if (sl_mod_thd_get(t)->type == SL_THD_CHILD_SCHED) {
//			child_miss_count ++;
//			if (child_miss_count > 10) while (1);
//		}
	}
//	sl_mod_print(" %u:%llu:%llu ", sl_mod_thd_get(t)->thdid, now, t->deadline);
	t->deadline += t->period;
	if (now - prev > sl_usec2cyc(10 * SL_DEBUG_DL_MISS_DIAG_USEC)) {
		thdid_t tmp = thd_start;
	//	char buf[1024] = { 0 };

		prev = now;
		//sprintc(buf, " %ld:P:%llu ", print_counter ++, dl_missed);
		sl_mod_print(" %ld:P:%llu ", print_counter ++, dl_missed);
//		while (tmp <= thd_end) {
//			struct sl_thd *td = sl_thd_lkup(tmp);
//			struct sl_thd_policy *tdp = sl_mod_thd_policy_get(td);
//			//sprintc(buf + strlen(buf), " %u:%lu:%lu ", td->thdid, tdp->made, tdp->missed);
//			sl_mod_print(" %u:%ld:%ld ", td->thdid, tdp->made, tdp->missed);
//
//			tmp ++;
//		}
		//sl_mod_print("%s", buf);
		if (print_counter > 100) while (1);
	}
#endif
	ps_list_rem_d(t);
	sl_timeout_mod_block(sl_mod_thd_get(t), 1, 0);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	struct sl_thd *th = sl_mod_thd_get(t);
	sl_print("W%u", sl_mod_thd_get(t)->thdid);
	/*
	 * TODO:
	 * there are different reasons for getting a number of wakeup events.. 
	 * 1. cos_asnd on a blocked thread, every such asnd will add a unblocked event to the sched.
	 * 2. expending tcap-budget, will get a unblocked (not really that) event to the sched.
	 */
	t->blocked = 0;
	sl_mod_yield(t, NULL);
	if (th->type == SL_THD_AEP || th->type == SL_THD_AEP_TCAP) sl_timeout_mod_wakeup(th);
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

	t->blocked = 0;

#ifdef SL_DEBUG_DEADLINES
	t->deadline    = ~(cycles_t)0;
	t->missed = t->made = 0;
#endif
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{ ps_list_rem_d(t); }

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	static cycles_t now;
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
		t->period_usec = v;
		t->period = sl_usec2cyc(v);

#ifdef SL_DEBUG_DEADLINES
		if (!now) rdtscll(now);
		t->deadline = now + t->period;
//		printc(" %u:%llu:%llu ", sl_mod_thd_get(t)->thdid, t->deadline, t->period);
		prev = now;
#endif

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

#ifdef SL_DEBUG_DEADLINES
	if (thd_start == 0) thd_start = sl_mod_thd_get(t)->thdid;
	thd_end = sl_mod_thd_get(t)->thdid;
	assert(thd_start <= thd_end);
#endif
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
