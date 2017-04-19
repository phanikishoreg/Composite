#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>
#include <heap.h>

#define debug(fmt,...)
#ifdef SL_DEBUG_DEADLINES
#define sl_mod_print printc
//static unsigned long long dl_missed = 0, dl_made = 0;
static cycles_t prev = 0;
static thdid_t thd_start = 0, thd_end = 0;
#else
#define sl_mod_print debug
#endif

#define SL_EDF_MAX_THDS MAX_NUM_THREADS
#define SL_EDF_DL_LOW   TCAP_PRIO_MIN

struct edf_heap {
	struct heap h;
	void       *data[SL_EDF_MAX_THDS];
	char        p; /* pad */
} edf_heap;

static struct heap *hs = (struct heap *)&edf_heap;
cycles_t task_start_time = 0;

void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{
	if (!cycles) return;

	if (t->budget) {
		assert(cycles < (cycles_t)TCAP_RES_MAX);
		t->expended += (tcap_res_t)cycles;

		if (t->expended >= t->budget) {
			t->expended = 0;
			sl_mod_block(t);
		}
	}
}

struct sl_thd_policy *
sl_mod_schedule(void)
{
	int i;
	cycles_t now;
	struct sl_thd_policy *t;
	struct sl_thd *td;

	assert(heap_size(hs));
	if (!heap_size(hs)) return NULL;
	rdtscll(now);
	t = heap_peek(hs);
	debug("schedule= peek idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);

	td = sl_mod_thd_get(t);
	if (t->budget && ((t->last_period == 0) || (t->last_period + t->period <= now) || sl_cycles_same(t->last_period + t->period, now))) {
		t->last_period = now;
		td->budget     = t->budget;
//		td->budget     = 0;
		t->expended    = 0;
	}

	return t;
}

void
sl_mod_deadlines(void)
{
#ifdef SL_DEBUG_DEADLINES
	cycles_t now;
	static long print_counter = 0;

	rdtscll(now);
	//if (now - prev > sl_usec2cyc(240 * 1000)) {
	if (now - prev > sl_usec2cyc(SL_DEBUG_DL_MISS_DIAG_USEC)) {
		thdid_t tmp = thd_start;

		prev = now;
		sl_mod_print(" C@%ld=>%llu:%llu ", print_counter, dl_made, dl_missed);
		//sl_mod_print(" %ld:C%u:%llu ", print_counter ++, sl_mod_thd_get(t)->thdid, dl_missed);

		while (tmp <= thd_end) {
			struct sl_thd *td = sl_thd_lkup(tmp);
			struct sl_thd_policy *tdp = sl_mod_thd_policy_get(td);

			if (td->thdid != (sl__globals()->idle_thd)->thdid)
				sl_mod_print(" ct%u:%ld:%ld ", td->thdid, tdp->made, tdp->missed);

			tmp ++;
		}

		print_counter ++;
		if (print_counter >= 20) 
			while (1) ;
	}
#endif
}

static void
__sl_mod_block(struct sl_thd_policy *t)
{
	if (t->prio_idx > -1) heap_remove(hs, t->prio_idx);
	t->prio_idx  = -1;

	sl_timeout_mod_block(sl_mod_thd_get(t), 1, 0);
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	//printc(" B%u ", sl_mod_thd_get(t)->thdid);
//	static aep_block_count;
//	if (sl_mod_thd_get(t)->type == SL_THD_AEP_TCAP) {
//		aep_block_count ++;
//		if (aep_block_count > 30) {
//			cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);
//			while (1);
//		}
//	}

#ifdef SL_DEBUG_DEADLINES
	cycles_t now;
//	static long print_counter = 0;

	rdtscll(now);

	if (sl_mod_thd_get(t)->type != SL_THD_AEP_TCAP) {
		if (now <= t->deadline) {
			t->made ++;
			dl_made ++;
		} else {
			t->missed ++;
			dl_missed ++;
		}
	}

//	//if (now - prev > sl_usec2cyc(240 * 1000)) {
//	if (now - prev > sl_usec2cyc(10 * SL_DEBUG_DL_MISS_DIAG_USEC)) {
//		thdid_t tmp = thd_start;
//
//		prev = now;
//		sl_mod_print(" C@%ld=>%llu:%llu ", print_counter, dl_made, dl_missed);
//		//sl_mod_print(" %ld:C%u:%llu ", print_counter ++, sl_mod_thd_get(t)->thdid, dl_missed);
//
//		while (tmp <= thd_end) {
//			struct sl_thd *td = sl_thd_lkup(tmp);
//			struct sl_thd_policy *tdp = sl_mod_thd_policy_get(td);
//
//			if (td->thdid != (sl__globals()->idle_thd)->thdid)
//				sl_mod_print(" ct%u:%ld:%ld ", td->thdid, tdp->made, tdp->missed);
//
//			tmp ++;
//		}
//
//		print_counter ++;
//		if (print_counter >= 20) 
//			while (1) ;
//	}
#endif

	if (t->prio_idx > -1) heap_remove(hs, t->prio_idx);
	if (sl_mod_thd_get(t)->type != SL_THD_AEP_TCAP) {
		t->deadline += t->period;
		t->priority  = t->deadline;
		assert(t->priority <= SL_EDF_DL_LOW);
		sl_thd_setprio(sl_mod_thd_get(t), t->priority);
	} else {
		struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
		tcap_res_t pbudget = (tcap_res_t)cos_introspect(ci, sl_thd_aep(sl__globals()->sched_thd)->tc, TCAP_GET_BUDGET);

		/* prio and deadline are updated in the thread right before COS_RCV!*/
		if (pbudget) cos_deftransfer_aep(sl_thd_aep(sl_mod_thd_get(t)), 1, sl_mod_thd_get(t)->prio);
	}

//	if (sl_mod_thd_get(t)->type == SL_THD_AEP_TCAP) {
//		struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
//		struct cos_aep_info *aep = sl_thd_aep(sl_mod_thd_get(t));
//		tcap_res_t pbudget = (tcap_res_t)cos_introspect(ci, sl_thd_aep(sl__globals()->sched_thd)->tc, TCAP_GET_BUDGET);	
//		tcap_res_t budget = (tcap_res_t)cos_introspect(ci, aep->tc, TCAP_GET_BUDGET);
//		assert(t->period != 0);
//
//		if (budget == 0) {
//			if (pbudget && cos_deftransfer_aep(sl_thd_aep(sl_mod_thd_get(t)), pbudget / 4, sl_mod_thd_get(t)->prio)) assert(0);
//		} else {
//      		//if (cos_deftransfer_aep(sl_thd_aep(sl_mod_thd_get(t)), 2, sl_mod_thd_get(t)->prio)) assert(0);
//      		if (cos_tcap_transfer(aep->rcv, aep->tc, 0, sl_mod_thd_get(t)->prio)) assert(0);
//      		
//		}
//
//		struct cos_aep_info *aep = sl_thd_aep(sl_mod_thd_get(t));
//		tcap_res_t budget = 0, pbudget = 0;
//
//
//		if (budget > pbudget) break;
//		if (budget >= t->budget) break;
//
//		if (budget && pbudget <= t->budget) break;
//		if (budget == 0) {
//			if (cos_deftransfer_aep(sl_thd_aep(t), budget, t->prio)) assert(0);
//		}
//	}

	debug("block= remove idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
	t->prio_idx  = -1;

	sl_timeout_mod_block(sl_mod_thd_get(t), 1, 0);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	struct sl_thd *th = sl_mod_thd_get(t);
	//printc(" W%u ", sl_mod_thd_get(t)->thdid);
	//assert(t->prio_idx < 0);
	if (t->prio_idx >= 0) return;

	heap_add(hs, t);
	debug("wakeup= add idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
        if (th->type == SL_THD_AEP || th->type == SL_THD_AEP_TCAP) sl_timeout_mod_remove(th);
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
	t->last_period = 0;
	t->budget_usec = 0;
	t->budget      = 0;

#ifdef SL_DEBUG_DEADLINES
	t->missed = t->made = 0;
#endif
	
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
	static cycles_t start_now;
	cycles_t diff;

	assert(type == SCHEDP_WINDOW || type == SCHEDP_BUDGET);
	if (type == SCHEDP_WINDOW) {
		t->period_usec = v;
		t->period = sl_usec2cyc(t->period_usec);

		/* first deadline. */
		if (!start_now) start_now = task_start_time;
		if (!start_now) {
			start_now = sl_now();
			diff = start_now - sl__globals()->start_time;

			start_now += (sl_timeout_period_get() - (diff % sl_timeout_period_get()));
		//	start_now += (sl_timeout_period_get() - (start_now % sl_timeout_period_get()));
		}
		if (!task_start_time) task_start_time = start_now;
		t->deadline = start_now + t->period;
		sl_mod_thd_get(t)->wakeup_cycs = start_now;
#ifdef SL_DEBUG_DEADLINES
		prev = start_now;
#endif
		/*
		 * TODO: 1. tcap_prio_t=48bit! mapping 64bit value to 48bit value.
		 *          (or, can we make cos_switch/cos_tcap_delegate support prio=64bits?).
		 *       2. wraparound (64bit or 48bit) logic for deadline-based-heap!
		 */
		t->priority = t->deadline;
		assert(t->priority <= SL_EDF_DL_LOW);
		heap_adjust(hs, t->prio_idx);
		debug("param_set= adjust idx: %d, deadline: %llu\n", t->prio_idx, t->deadline);
		sl_thd_setprio(sl_mod_thd_get(t), t->priority);
		printc("Thd:%u Dl:%llu P:%llu Wkup:%llu\n", sl_mod_thd_get(t)->thdid, t->deadline, t->period, sl_mod_thd_get(t)->wakeup_cycs);
	} else if (type == SCHEDP_BUDGET) {
		int ret;
		t->budget_usec = v;
		t->budget = sl_usec2cyc(t->budget_usec);

		assert(t->period != 0);
		if (sl_mod_thd_get(t)->type == SL_THD_AEP_TCAP) {
			ret = cos_deftransfer_aep(sl_thd_aep(sl_mod_thd_get(t)), t->budget, sl_mod_thd_get(t)->prio);
			if (ret == 0) __sl_mod_block(t);
		}
		t->last_period = start_now;
	}

#ifdef SL_DEBUG_DEADLINES
	if (thd_start == 0) thd_start = sl_mod_thd_get(t)->thdid;
	thd_end = sl_mod_thd_get(t)->thdid;
	assert(thd_start <= thd_end);
#endif
}

static int
__compare_min(void *a, void *b)
{ return ((struct sl_thd_policy *)a)->deadline <= ((struct sl_thd_policy *)b)->deadline; }

static void
__update_idx(void *e, int pos)
{ ((struct sl_thd_policy *)e)->prio_idx = pos; }

void
sl_mod_init(void)
{
	sl_mod_init_sync(0);
}

void
sl_mod_init_sync(cycles_t time)
{
	heap_init(hs, SL_EDF_MAX_THDS, __compare_min, __update_idx);
#ifdef SL_DEBUG_DEADLINES
	dl_missed = dl_made = 0;
#endif
	task_start_time = time;
}
