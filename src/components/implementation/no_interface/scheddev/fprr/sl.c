/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <ps.h>
#include <sl.h>
#include <sl_mod_policy.h>
#include <sl_aep.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>

struct sl_global sl_global_data;

/*
 * These functions are removed from the inlined fast-paths of the
 * critical section (cs) code to save on code size/locality
 */
int
sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok)
{
	struct sl_thd    *t = sl_thd_curr();
	struct sl_global *g = sl__globals();
	int ret;

	/* recursive locks are not allowed */
	assert(csi->s.owner != curr);
	if (!csi->s.contention) {
		csi->s.contention = 1;
		if (!ps_cas(&g->lock.u.v, cached->v, csi->v)) { sl_print("C0"); return 1; }
	}

	/* Switch to the owner of the critical section, with inheritance using our tcap/priority */
	ret = cos_defswitch(csi->s.owner, t->prio, g->timeout_next, tok);
	if (ret) { sl_print("C1"); return ret; }

	sl_print("C2");
	/* if switch was successful, return to take the lock. */
	return 1;
}

/* Return 1 if we need a retry, 0 otherwise */
int
sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok)
{
	struct sl_thd    *t = sl_thd_curr();
	struct sl_global *g = sl__globals();

	if (!ps_cas(&g->lock.u.v, cached->v, 0)) { sl_print("X0"); return 1; }
	/* let the scheduler thread decide which thread to run next, inheriting our budget/priority */
	if (cos_defswitch(g->sched_thdcap, t->prio, g->timeout_next, tok)) { sl_print("X3"); return 0; }

	sl_print("X1");
	return 0;
}

int
sl_thd_block_cs(struct sl_thd *t)
{
	assert(t);

	if (t->state == SL_THD_WOKEN) {
		sl_print("Q0:%u ", t->thdid);
		t->state = SL_THD_RUNNABLE;
		return 1;
	}
//	if (t->state == SL_THD_BLOCKED) {
		//if (t == sl_thd_curr()) return 0;
//		sl_print("Q0:%u ", t->thdid); return 1;
//	}
	assert(t->state == SL_THD_RUNNABLE);
	t->state = SL_THD_BLOCKED;
	sl_mod_block(sl_mod_thd_policy_get(t));

	sl_print("Q1:%u ", t->thdid);
	return 0;
}

void
sl_thd_block(thdid_t tid)
{
	struct sl_thd *t;

	/* TODO: dependencies not yet supported */
	assert(!tid);

	sl_cs_enter();
	t = sl_thd_curr();
	if (sl_thd_block_cs(t)) {
		sl_print("V0");
		sl_cs_exit();
		return;
	}
	sl_cs_exit_schedule();

	return;
}

int
sl_thd_wakeup_cs(struct sl_thd *t)
{
	assert(t);

//	if (t->state == SL_THD_RUNNABLE) { sl_print("U0:%u ", t->thdid); return 1; }
	if (t->state == SL_THD_RUNNABLE) {
		sl_print("U0:%u ", t->thdid);
		t->state = SL_THD_WOKEN;
		return 1;
	}

	/* wakeup events can be many. they could come when the thread is in any of the BLOCKED, RUNNABLE, WOKEN states */
	assert(t->state == SL_THD_BLOCKED || t->state == SL_THD_WOKEN);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));

	sl_print("U1:%u ", t->thdid);
	return 0;
}

void
sl_thd_wakeup(thdid_t tid)
{
	struct sl_thd *t;
	tcap_t         tcap;
	tcap_prio_t    prio;

	sl_cs_enter();
	t = sl_thd_lkup(tid);
	if (unlikely(!t) || sl_thd_wakeup_cs(t)) {
		sl_print("K0");
		sl_cs_exit();
		return;
	}
	sl_cs_exit_schedule();

	return;
}

void
sl_thd_yield(thdid_t tid)
{
	struct sl_thd *t = sl_thd_curr();

	/* directed yields not yet supported! */
	assert(!tid);

	sl_cs_enter();
	sl_mod_yield(sl_mod_thd_policy_get(t), NULL);
	sl_cs_exit_schedule();

	return;
}

static struct sl_thd *
sl_thd_alloc_init(thdid_t tid, struct cos_aep_info *aep, asndcap_t snd, sl_thd_type type)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	tp = sl_thd_alloc_backend(tid);
	if (!tp) goto done;
	t = sl_mod_thd_get(tp);

	t->aep    = aep;
	t->thdid  = tid;
	t->state  = SL_THD_RUNNABLE;
	t->type   = type;
	t->sndcap = snd;
	t->budget = 0;
	sl_thd_index_add_backend(sl_mod_thd_policy_get(t));

	t->period = t->wakeup_cycs = 0;
	t->wakeup_idx = -1;
	t->prio   = TCAP_PRIO_MIN;
done:
	return t;
}

static struct sl_thd *
sl_thd_alloc_intern(cos_thd_fn_t fn, void *data)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	thdid_t  tid;

	aep = sl_aep_alloc();
	if (!aep) goto done;

	aep->tc  = sl_thd_aep(sl__globals()->sched_thd)->tc; /* using parent's tcap for simple threads */
	aep->thd = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	if (!aep->thd) goto done;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, 0, SL_THD_SIMPLE);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));
done:
	return t;
}

static struct sl_thd *
sl_aepthd_alloc_intern(cos_aepthd_fn_t fn, void *data, tcap_t tc, struct cos_defcompinfo *comp, sl_thd_type type)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	asndcap_t snd;
	thdid_t   tid;
	int       ret;

	aep = sl_aep_alloc();
	if (!aep) goto done;

	if (type != SL_THD_CHILD_SCHED) {
		snd = 0;
		if (type == SL_THD_AEP) ret = cos_aep_tcap_alloc(aep, sl_thd_aep(sl__globals()->sched_thd)->tc, fn, data);
		else                    ret = cos_aep_alloc(aep, fn, data);
		if (ret) goto done;
		/* we don't need fn & data in sl_thd after this point */
	} else {
		*aep = comp->sched_aep; /* shallow */
		snd  = cos_asnd_alloc(ci, aep->rcv, ci->captbl_cap);
		assert(snd);
	}

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep, snd, type);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));
done:
	return t;
}

static struct sl_thd *
sl_aepthd_init_intern(thdcap_t thd, tcap_t tc, arcvcap_t rcv)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	struct cos_aep_info    *aep = NULL;
	thdid_t   tid;
	int       ret;

	aep = sl_aep_alloc();
	if (!aep) goto done;

	aep->thd = thd;
	aep->tc = tc;
	aep->rcv = rcv;

	tid = cos_introspect(ci, aep->thd, THD_GET_TID);
	assert(tid);

	t = sl_thd_alloc_init(tid, aep, 0, SL_THD_AEP);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;

}

struct sl_thd *
sl_aepthd_init(thdcap_t thd, arcvcap_t rcv)
{ return sl_aepthd_init_intern(thd, sl_thd_aep(sl__globals()->sched_thd)->tc, rcv); }

struct sl_thd *
sl_thd_alloc(cos_thd_fn_t fn, void *data)
{ return sl_thd_alloc_intern(fn, data); }

struct sl_thd *
sl_aepthd_tcap_alloc(cos_aepthd_fn_t fn, void *data, tcap_t tc)
{ return sl_aepthd_alloc_intern(fn, data, tc, NULL, SL_THD_AEP); }

struct sl_thd *
sl_aepthd_alloc(cos_aepthd_fn_t fn, void *data)
{ return sl_aepthd_alloc_intern(fn, data, 0, NULL, SL_THD_AEP_TCAP); }

/* allocate/initialize a sl_thd from component's sched_aep */
struct sl_thd *
sl_thd_comp_init(struct cos_defcompinfo *comp)
{ return sl_aepthd_alloc_intern(NULL, NULL, 0, comp, SL_THD_CHILD_SCHED); }

void
sl_thd_free(struct sl_thd *t)
{
	sl_thd_index_rem_backend(sl_mod_thd_policy_get(t));
	t->state = SL_THD_FREE;
	/* TODO: add logic for the graveyard to delay this deallocation if t == current */
	sl_thd_free_backend(sl_mod_thd_policy_get(t));
}

void
sl_thd_param_set(struct sl_thd *t, sched_param_t sp)
{
	sched_param_type_t type;
	unsigned int       value;

	sched_param_get(sp, &type, &value);
	if (type == SCHEDP_WINDOW) {
		assert(t->period == 0);
		t->period = sl_usec2cyc(value);
	}
	sl_mod_thd_param_set(sl_mod_thd_policy_get(t), type, value);
}

void
sl_timeout_period(microsec_t period)
{
	cycles_t p = sl_usec2cyc(period);
	cycles_t now, diff;

	sl__globals()->period = p;
	now = sl_now();
	diff = now - sl__globals()->start_time;
	sl_timeout_oneshot(now + p + (p - (diff % p)));
	//sl_timeout_relative(p);
}

/* engage space heater mode */
void
sl_idle(void *d)
{ while (1) ; }

void
sl_init(void)
{ sl_init_sync(0, 0); }

void
sl_init_sync(cycles_t start, cycles_t task_start)
{
	struct sl_global       *g  = sl__globals();
	struct cos_defcompinfo *ci = cos_defcompinfo_curr_get();
	struct cos_aep_info *aep;

	/* must fit in a word */
	assert(sizeof(struct sl_cs) <= sizeof(unsigned long));

	g->cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	g->lock.u.v     = 0;
	g->sched_tok    = 0;
	g->start_time   = start;

	sl_thd_init_backend();
	sl_aep_init();
	sl_mod_init_sync(task_start);
	sl_timeout_mod_init();

	/* init a aep struct for scheduler thread */
	aep      = sl_aep_alloc();
	aep->thd = BOOT_CAPTBL_SELF_INITTHD_BASE;
	aep->rcv = BOOT_CAPTBL_SELF_INITRCV_BASE;
	aep->tc  = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	/* Create the scheduler thread for us */
	g->sched_thd    = sl_thd_alloc_init(cos_thdid(), aep, 0, SL_THD_AEP);
	assert(g->sched_thd);
	g->sched_thdcap = BOOT_CAPTBL_SELF_INITTHD_BASE;
	sl_thd_setprio(g->sched_thd, TCAP_PRIO_MAX);

	g->idle_thd     = sl_thd_alloc(sl_idle, NULL);
	assert(g->idle_thd);

	return;

}

void
sl_sched_loop(void)
{
	arcvcap_t sched_rcv = sl_thd_aep(sl__globals()->sched_thd)->rcv;
//	int iters = 10;

	while (1) {
		int pending, ret;
		rcv_flags_t rf_def = 0, rf_use;

		rf_use = rf_def | RCV_NON_BLOCKING;
		do {
			thdid_t        tid;
			int            blocked, rcvd;
			cycles_t       cycles;
			//rcv_flags_t    rf = RCV_ALL_PENDING | RCV_NON_BLOCKING;
			struct sl_thd *t;

			/*
			 * a child scheduler may receive both scheduling notifications (block/unblock 
			 * states of it's child threads) and normal notifications (mainly activations from
			 * it's parent scheduler).
			 */
retry_rcv:
			sl_print("a");
			pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, rf_use, &rcvd, &tid, &blocked, &cycles);
			if (pending == -EAGAIN) {
//				rf_use = rf_def;
//				sl_print("j");
				break;
				//continue;
			}
			if (!tid) { 
				sl_print("i");
				continue;
			}

			//printc(" %u:%llu ", sl_thd_curr()->thdid, sl_exec_cycles());
			sl_print("b=%d,%d,%u,%d,%llu", pending, rcvd, tid, blocked, cycles);
			ret = sl_cs_enter_sched();
			if (ret == -EBUSY) { 
			sl_print("c");
				goto retry_rcv; /* if there are pending notifications.. */
			}

			sl_print("d");
			t = sl_thd_lkup(tid);
			assert(t);

			sl_mod_execution(sl_mod_thd_policy_get(t), cycles);
			if (blocked)     { sl_print("B+"); /*sl_thd_block_cs(t);*/ sl_mod_block(sl_mod_thd_policy_get(t)); }
			/* tcap expended notification will have blocked = 0 and cycles != 0 */
			else if(!cycles) { sl_print("W+"); /*sl_thd_wakeup_cs(t);*/ sl_mod_wakeup(sl_mod_thd_policy_get(t)); }

			sl_print("e");
			sl_cs_exit();
		} while (pending);

		sl_print("f");
		ret = sl_cs_enter_sched();
		if (ret == -EBUSY) {

			sl_print("g");
			continue; /* if there are pending notifications.. */
		}
		sl_mod_deadlines();

			sl_print("h");
		/* If switch returns an inconsistency, we retry anyway */
//		iters --;
//		if (iters <= 0) while (1);
		sl_cs_exit_schedule_nospin();
			sl_print("k");
	}
}
