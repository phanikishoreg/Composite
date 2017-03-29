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
void
sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok)
{
	struct sl_thd    *t = sl_thd_curr();
	struct sl_global *g = sl__globals();

	/* recursive locks are not allowed */
	assert(csi->s.owner != sl_thd_thd(t));
	if (!csi->s.contention) {
		csi->s.contention = 1;
		if (!ps_cas(&g->lock.u.v, cached->v, csi->v)) return;
	}
	/* Switch to the owner of the critical section, with inheritance using our tcap/priority */
	cos_defswitch(csi->s.owner, t->prio, g->timeout_next, tok);
	/* if we have an outdated token, then we want to use the same repeat loop, so return to that */
}

/* Return 1 if we need a retry, 0 otherwise */
int
sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok)
{
	struct sl_thd    *t = sl_thd_curr();
	struct sl_global *g = sl__globals();

	if (!ps_cas(&g->lock.u.v, cached->v, 0)) return 1;
	/* let the scheduler thread decide which thread to run next, inheriting our budget/priority */
	cos_defswitch(g->sched_thdcap, t->prio, g->timeout_next, tok);

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
	if (unlikely(t->state == SL_THD_WOKEN)) {
		t->state = SL_THD_RUNNABLE;
		sl_cs_exit();
		return;
	}

	assert(t->state == SL_THD_RUNNABLE);
	t->state = SL_THD_BLOCKED;
	sl_mod_block(sl_mod_thd_policy_get(t));
	sl_cs_exit_schedule();

	return;
}

void
sl_thd_wakeup(thdid_t tid)
{
	struct sl_thd *t;
	tcap_t         tcap;
	tcap_prio_t    prio;

	sl_cs_enter();
	t = sl_thd_lkup(tid);
	if (unlikely(!t)) goto done;

	if (unlikely(t->state == SL_THD_RUNNABLE)) {
		t->state = SL_THD_WOKEN;
		goto done;
	}

	assert(t->state = SL_THD_BLOCKED);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));
	sl_cs_exit_schedule();

	return;
done:
	sl_cs_exit();
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
	sl_mod_thd_param_set(sl_mod_thd_policy_get(t), type, value);
}

void
sl_timeout_period(microsec_t period)
{
	cycles_t p = sl_usec2cyc(period);

	sl__globals()->period = p;
	sl_timeout_relative(p);
}

/* engage space heater mode */
void
sl_idle(void *d)
{ while (1) ; }

void
sl_init(void)
{
	struct sl_global       *g  = sl__globals();
	struct cos_defcompinfo *ci = cos_defcompinfo_curr_get();
	struct cos_aep_info *aep;

	/* must fit in a word */
	assert(sizeof(struct sl_cs) <= sizeof(unsigned long));

	g->cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	g->lock.u.v     = 0;

	sl_thd_init_backend();
	sl_aep_init();
	sl_mod_init();
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

	g->idle_thd     = sl_thd_alloc(sl_idle, NULL);
	assert(g->idle_thd);

	return;
}

void
sl_sched_loop(void)
{
	arcvcap_t sched_rcv = sl_thd_aep(sl__globals()->sched_thd)->rcv;

	while (1) {
		int pending;

		sl_cs_enter();

		do {
			thdid_t        tid;
			int            blocked;
			cycles_t       cycles;
			struct sl_thd *t;

			/*
			 * a child scheduler may receive both scheduling notifications (block/unblock 
			 * states of it's child threads) and normal notifications (mainly activations from
			 * it's parent scheduler).
			 */
			pending = cos_sched_rcv(sched_rcv, &tid, &blocked, &cycles);
			if (!tid) continue;

			t = sl_thd_lkup(tid);
			assert(t);
			/* don't report the idle thread */
			if (unlikely(t == sl__globals()->idle_thd)) continue;
			sl_mod_execution(sl_mod_thd_policy_get(t), cycles);

			if (blocked)     sl_mod_block(sl_mod_thd_policy_get(t));
			/* tcap expended notification will have blocked = 0 and cycles != 0 */
			else if(!cycles) sl_mod_wakeup(sl_mod_thd_policy_get(t));
		} while (pending);

		/* If switch returns an inconsistency, we retry anyway */
		sl_cs_exit_schedule_nospin();
	}
}
