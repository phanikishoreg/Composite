/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

/*
 * Scheduler library (sl) abstractions and functions.
 *
 * This library does a few things:
 * 1. hide the esoteric nature of the cos_kernel_api's dispatch
 *    methods, and the scheduler event notifications + scheduler
 *    thread,
 * 2. provide synchronization around scheduler data-strutures, and
 * 3. abstract shared details about thread blocking/wakeup, lookup,
 *    and inter-thread dependency management.
 *
 * This library interacts with a number of other libraries/modules.
 *
 * - uses: dispatching functions in the cos_kernel_api (which uses,
 *   the kernel system call layer)
 * - uses: parsec (ps) for atomic instructions and synchronization
 * - uses: memory allocation functions provided by a run-time (either
 *   management of static memory, or something like parsec)
 * - uses: scheduler modules that implement the scheduling policy
 *
 */

#ifndef SL_H
#define SL_H

#include <cos_defkernel_api.h>
#include <ps.h>
#include <res_spec.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#if SL_DEBUG
#define sl_print(fmt, args...) printc(" %u:" fmt " ", sl_thd_curr()->thdid , ##args)
//#define sl_print printc
#else
#define sl_print(fmt,...)
#endif

/* Critical section (cs) API to protect scheduler data-structures */
struct sl_cs {
	union sl_cs_intern {
		struct {
			thdcap_t owner      :31;
			u32_t    contention :1;
		} PS_PACKED s;
		unsigned long v;
	} u;
};

typedef u64_t sl_sched_tok_t;

struct sl_global {
	struct sl_cs   lock;

	thdcap_t       sched_thdcap;
	struct sl_thd *sched_thd;
	struct sl_thd *idle_thd;

	int            cyc_per_usec;
	cycles_t       period;
	cycles_t       timer_next;
	tcap_time_t    timeout_next;
	sl_sched_tok_t sched_tok;
};

extern struct sl_global sl_global_data;

static inline struct sl_global *
sl__globals(void)
{ return &sl_global_data; }

static inline cycles_t
sl_exec_cycles(void)
{
	cycles_t exec;

	if (cos_introspect64(cos_compinfo_get(cos_defcompinfo_curr_get()), 
	    sl_thd_aep(sl__globals()->sched_thd)->tc, TCAP_GET_EXECCYCS, &exec)) assert(0);

	return exec;
}

static inline cycles_t
sl_now(void)
{ return ps_tsc(); }

/*
 * Time and timeout API.
 *
 * This can be used by the scheduler policy module *and* by the
 * surrounding component code.  To avoid race conditions between
 * reading the time, and setting a timeout, we avoid relative time
 * measurements.  sl_now gives the current cycle count that is on an
 * absolute timeline.  The periodic function sets a period that can be
 * used when a timeout has happened, the relative function sets a
 * timeout relative to now, and the oneshot timeout sets a timeout on
 * the same absolute timeline as returned by sl_now.
 */
void sl_timeout_period(cycles_t period);

static inline cycles_t
sl_timeout_period_get(void)
{ return sl__globals()->period; }

static inline void
sl_timeout_oneshot(cycles_t absolute_us)
{
	sl__globals()->timer_next   = absolute_us;
	sl__globals()->timeout_next = tcap_cyc2time(absolute_us);
}

static inline void
sl_timeout_relative(cycles_t offset)
{ sl_timeout_oneshot(sl_now() + offset); }

static inline microsec_t
sl_cyc2usec(cycles_t cyc)
{ return cyc / sl__globals()->cyc_per_usec; }

static inline microsec_t
sl_usec2cyc(microsec_t usec)
{ return usec * sl__globals()->cyc_per_usec; }

/* FIXME: integrate with param_set */
static inline void
sl_thd_setprio(struct sl_thd *t, tcap_prio_t p)
{ t->prio = p; }

static inline struct sl_thd *
sl_thd_lkup(thdid_t tid)
{ return sl_mod_thd_get(sl_thd_lookup_backend(tid)); }

static inline struct sl_thd *
sl_thd_curr(void)
{ return sl_thd_lkup(cos_thdid()); }

/* are we the owner of the critical section? */
static inline int
sl_cs_owner(void)
{ return sl__globals()->lock.u.s.owner == sl_thd_thdcap(sl_thd_curr()); }

/* ...not part of the public API */
int sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok);
int sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok);

/* Enter into the scheduler critical section */
static inline int
sl_cs_enter_nospin(void)
{
	union sl_cs_intern csi, cached;
	struct sl_thd     *t = sl_thd_curr();
	sched_tok_t        tok;
	int                ret;

	assert(t);
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.owner)) {
		sl_print("S0");
		return sl_cs_enter_contention(&csi, &cached, sl_thd_thdcap(t), tok);
	}

	csi.s.owner = sl_thd_thdcap(t);
	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, csi.v)) {
		sl_print("S1");
		return 1;
	}

	sl_print("S2");
	return 0;
}

/* Enter into the scheduler critical section */
static inline void
sl_cs_enter(void)
{ while (sl_cs_enter_nospin()) ; }

/* Enter into the scheduler critical section from scheduler's context */
static inline int
sl_cs_enter_sched(void)
{
	int ret;

	do {
		ret = sl_cs_enter_nospin();
	} while (ret && ret != -EBUSY);

	return ret;
}

/*
 * Release the scheduler critical section, switch to the scheduler
 * thread if there is pending contention
 */
static inline void
sl_cs_exit(void)
{
	union sl_cs_intern csi, cached;
	sched_tok_t tok;

retry:
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.contention)) {
		sl_print("E3");
		if (sl_cs_exit_contention(&csi, &cached, tok)) {
			sl_print("E0");
			goto retry;
		}
		sl_print("E1");
		return;
	}

	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, 0)) {
		sl_print("E2");
		goto retry;
	}
	sl_print("E4");
}

static inline int
sl_thd_activate(struct sl_thd *t, sched_tok_t tok, tcap_res_t budget, sl_sched_tok_t sltok, tcap_t hitc, tcap_prio_t hiprio)
{
        struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
        struct cos_compinfo    *ci  = &dci->ci;
	struct cos_aep_info *aep;

	aep = sl_thd_aep(t);
	assert(aep);

	//printc("#%u=>%u#", cos_thdid(), t->thdid);
	sl_print("A0=%u ", t->thdid);
	switch (t->type) {
	case SL_THD_SIMPLE:
	case SL_THD_AEP:
	case SL_THD_CHILD_NOSCHED:
	{
		assert(aep->tc);

		sl_print("A*\n");
		return cos_hithd_defswitch_aep(aep, t->prio, sl__globals()->timeout_next, tok, hitc, hiprio);
	}
	case SL_THD_AEP_TCAP: /* Transfer budget if it needs replenisment! */
	{
		sl_print("A2\n");

		return cos_hithd_defswitch_aep(aep, t->prio, sl__globals()->timeout_next, tok, hitc, hiprio);
	}
	case SL_THD_CHILD_SCHED: /* delegate if it requires replenishment or just send notification */
	{
		sl_print("A3:%llu:%llu ", sltok, sl__globals()->sched_tok);
		if (budget) {
			sl_print("A5\n");
			if (sl__globals()->sched_tok == sltok) {
				t->budget = 0;
				if(cos_defdelegate(t->sndcap, budget, t->prio, TCAP_DELEG_YIELD)) assert(0);
				return 0;
			}
			return 1;
		}
//		tcap_res_t repl = t->budget;
//
//		t->budget = 0;
//		if (repl) {
//			tcap_res_t budget;
//
//			budget = (tcap_res_t)cos_introspect(ci, aep->tc, TCAP_GET_BUDGET);
//			sl_print("A3\n");
//			if (budget < repl) {
//			sl_print("A5:%lu\n", repl-budget);
//				if(cos_defdelegate(t->sndcap, repl - budget, t->prio, TCAP_DELEG_YIELD)) assert(0);
//				else                                                                     return 0;
//			}
//		}

			sl_print("A4\n");
		if (sl__globals()->sched_tok == sltok) return cos_asnd(t->sndcap, 1);

		return 0;
	}
	default: assert(0);
	}

	assert(0);
}

/*
 * Do a few things: 1. take the critical section if it isn't already
 * taken, 2. call schedule to find the next thread to run, 3. release
 * the critical section (note this will cause visual asymmetries in
 * your code if you call sl_cs_enter before this function), and
 * 4. switch to the given thread.  It hides some races, and details
 * that would make this difficult to write repetitively.
 *
 * Preconditions: if synchronization is required with code before
 * calling this, you must call sl_cs_enter before-hand (this is likely
 * a typical case).
 *
 * Return: the return value from cos_switch.  The caller must handle
 * this value correctly.
 *
 * A common use-case is:
 *
 * sl_cs_enter();
 * scheduling_stuff()
 * sl_cs_exit_schedule();
 *
 * ...which correctly handles any race-conditions on thread selection and
 * dispatch.
 */
static inline int
sl_cs_exit_schedule_nospin(void)
{
        struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
        struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd_policy   *pt;
	struct sl_thd          *t, *ht;
	struct sl_global       *globals = sl__globals();
	sched_tok_t    tok;
	cycles_t       now;
	s64_t          offset;
	int            ret;
	tcap_res_t     budget = 0, repl = 0;
	sl_sched_tok_t sltok = 0;
	tcap_t         hitc = 0;
	tcap_prio_t    hiprio = 0;

	if (unlikely(!sl_cs_owner()) && (ret = sl_cs_enter_nospin())) return ret;

	tok    = cos_sched_sync();
	now    = sl_now();
	offset = (s64_t)(globals->timer_next - now);
	if (globals->timer_next && offset <= 0) sl_timeout_mod_expended(now, globals->timer_next);
	sl_timeout_mod_wakeup_expired(now);

	/*
	 * Once we exit, we can't trust t's memory as it could be
	 * deallocated/modified, so cache it locally.  If these values
	 * are out of date, the scheduler synchronization tok will
	 * catch it.  This is a little twitchy and subtle, so lets put
	 * it in a function, here.
	 */
	pt     = sl_mod_schedule();
	if (unlikely(!pt)) {
		t = sl__globals()->idle_thd;
	}
	else {
		t = sl_mod_thd_get(pt);
		repl = t->budget;
		//t->budget = 0;
		
		if (repl) {
			struct cos_aep_info *aep = sl_thd_aep(t);

			assert(aep->tc);
			budget = (tcap_res_t)cos_introspect(ci, aep->tc, TCAP_GET_BUDGET);

			if (budget < repl) budget = repl - budget;
			else               budget = 0;
		}
	}

	if (t->type == SL_THD_CHILD_SCHED) {
		sltok = __sync_add_and_fetch(&globals->sched_tok, 1);
//		sltok = globals->sched_tok;
		sl_print(" K:%llu ", sltok);
	//	if (budget) {
	//		sl_print("A5\n");
	//		if(cos_defdelegate(t->sndcap, budget, t->prio, TCAP_DELEG_YIELD)) assert(0);
	//	} else {
	//		sl_print("A4\n");
	//		cos_asnd(t->sndcap, 1);
	//	}
	} else if (t->type == SL_THD_AEP_TCAP) {
		tcap_res_t pbudget = (tcap_res_t)cos_introspect(ci, sl_thd_aep(sl__globals()->sched_thd)->tc, TCAP_GET_BUDGET);

		if (pbudget < budget) budget = pbudget;
		if (budget && cos_deftransfer_aep(sl_thd_aep(t), budget, t->prio)) ;
		else t->budget -= budget;
	}
	ht = sl_timeout_mod_block_peek();
	if (ht && ht->wakeup_cycs < (now + sl_usec2cyc(200))) {
		hitc = sl_thd_aep(ht)->tc;
		hiprio = ht->prio;
	}

	sl_cs_exit();

	return sl_thd_activate(t, tok, budget, sltok, hitc, hiprio);
}

static inline void
sl_cs_exit_schedule(void)
{ while (sl_cs_exit_schedule_nospin()) ; }

/*
 * if tid == 0, just block the current thread; otherwise, create a
 * dependency from this thread on the target tid (i.e. when the
 * scheduler chooses to run this thread, we will run the dependency
 * instead (note that "dependency" is transitive).
 */
void sl_thd_block(thdid_t tid);
int sl_thd_block_cs(struct sl_thd *t);
/* wakeup a thread that has (or soon will) block */
void sl_thd_wakeup(thdid_t tid);
int sl_thd_wakeup_cs(struct sl_thd *t);
void sl_thd_yield(thdid_t tid);

/* The entire thread allocation and free API */
struct sl_thd *sl_thd_alloc(cos_thd_fn_t fn, void *data);
/*
 * complying to the cos_defkernel_api,
 * (child) comp should already have it's initthd/initrcv alloc'd if
 * we have it's cos_defcompinfo object!
 *
 * TODO: we might need to create a sndcap to comp->sched_aep->sched_rcv!
 */
struct sl_thd *sl_thd_comp_init(struct cos_defcompinfo *comp);
/*
 * TODO: free only if alloc'd. 
 *       free sndcap of child-sched
 */
void sl_thd_free(struct sl_thd *t);

/*
 * uses sched_rcv of the current component for notif!
 * uses tc for tcap!
 */
struct sl_thd *sl_aepthd_tcap_alloc(cos_aepthd_fn_t fn, void *data, tcap_t tc);

/*
 * uses sched_rcv of the current component for notif!
 * uses sched_tc of the current component for tcap!
 */
struct sl_thd *sl_aepthd_alloc(cos_aepthd_fn_t fn, void *data);

void sl_thd_param_set(struct sl_thd *t, sched_param_t sp);

/*
 * Initialization protocol in cos_init: initialization of
 * library-internal data-structures, and then the ability for the
 * scheduler thread to start its scheduling loop.
 *
 * sl_init();
 * sl_*;            <- use the sl_api here
 * ...
 * sl_sched_loop(); <- loop here
 */
void sl_init(void);
void sl_sched_loop(void);

#endif	/* SL_H */
