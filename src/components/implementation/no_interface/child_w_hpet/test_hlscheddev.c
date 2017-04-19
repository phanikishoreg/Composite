/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <sl.h>
#include "spinner.h"
#include "hier_layout.h"

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

#undef STANDALONE_TEST

void
test_hpetaep_fn(arcvcap_t rcv, void *data)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct sl_thd       *t   = sl_thd_curr();
	struct sl_thd_policy *tp = sl_mod_thd_policy_get(t);
	struct cos_aep_info *aep = sl_thd_aep(t);
	thdid_t              tid = t->thdid;
	cycles_t             now;
	int counter = 0;

	while (1) {
		microsec_t workusecs = child_thds_W[(int)data];
		rcv_flags_t flg = 0;
		int rcvd = 0;
		int ret, missed = 0;
		int pending;
		tcap_res_t pbudget = (tcap_res_t)cos_introspect(ci, sl_thd_aep(sl__globals()->sched_thd)->tc, TCAP_GET_BUDGET);

                tp->deadline += tp->period;
                tp->priority  = tp->deadline;
                sl_thd_setprio(t, tp->priority);
//		if ((ret = cos_tcap_transfer(aep->rcv, aep->tc, 0, t->prio))) {
//			printc("ret=%d", ret);
//			assert(0);
//		}

//		if (pbudget) cos_deftransfer_aep(aep, 1, t->prio);
//		pending = cos_rcv(rcv, flg, &rcvd);
//		cos_deftransfer_aep(aep, 1, t->prio);
		/*
		 * HACK HACK HACK!
		 * this is very much close to a ugly hack..
		 * optimizing to remove a transfer call before the call to cos_rcv..
		 * thus, updating tcap prio from the sched thread.. 
		 * if it's not already updated at BLOCK.. Which could be out of sequence, because
		 * scheduler is the one that calls block on this aep-thread.. 
		 * also, some blocks can be missed and be interpreted as WAKEUPs.. That could miss 
		 * calculating deadlines.. hence moved deadline calculation, miss/made calculation right around
		 * cos_rcv in the same thread..!
		 */
		pending = cos_rcv_schedprio(rcv, flg, sl_thd_aep(sl__globals()->sched_thd)->rcv, t->prio, &rcvd);
		missed = spin_usecs_dl(workusecs, tp->deadline);
	//	spin_usecs(workusecs);
#ifdef SL_DEBUG_DEADLINES
		if (rcvd > 1) {
			printc("R:%d", rcvd);
			rcvd --;
			dl_missed += rcvd;
			tp->missed += rcvd;
		}
		if (missed) {
			dl_missed ++;
			tp->missed ++;
		} else {
			dl_made ++;
			tp->made ++;
		}
#endif

		//sl_thd_yield(0);
	}

	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);
}

void
test_thd_fn(void *data)
{
	struct sl_thd *t = sl_thd_curr();
	struct sl_thd_policy *tp = sl_mod_thd_policy_get(t);
	thdid_t tid = cos_thdid();

	while (1) {
		int missed = 0;
		microsec_t workusecs = child_thds_W[(int)data];
	
		spin_usecs(workusecs);
		//missed = spin_usecs_dl(workusecs, tp->deadline);
#ifdef SL_DEBUG_DEADLINES
//		if (missed) {
//			dl_missed ++;
//			tp->missed ++;
//		} else {
//			dl_made ++;
//			tp->made ++;
//		}
#endif
		sl_thd_block(0);
	}
}

void
test_loop(void *data)
{
	assert (0);
	thdid_t ctid = cos_thdid();
	while (1) {
		cycles_t now, prev;
		cycles_t cycles;
		thdid_t tid;
		int pending, rcvd, blocked;
		rcv_flags_t rf_use = 0;
		microsec_t workusecs = child_thds_W[(int)data];

		pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, rf_use, &rcvd, &tid, &blocked, &cycles);
	
		rdtscll(prev);
		spin_usecs(workusecs);
		rdtscll(now);

//		if (now - prev > C_array[(int)data]) {
//			printc("h=%u", ctid);
//		}

	}
}

void
cos_init(void)
{
	int                     i, ret;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct sl_thd          *threads[N_CHILD_THDS];
	union sched_param       sp;
	cycles_t                sched_start_time, task_start_time, now;
	unsigned int b = 0, c = 0;
	cycles_t cycles;
	thdid_t tid;
	int rcvd, blocked;
	cycles_t s, e;

//	printc("EDF!!\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	//cos_defcompinfo_init();
        cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_BASE, BOOT_CAPTBL_SELF_INITTHD_BASE, 
                                 BOOT_CAPTBL_SELF_INITRCV_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
                                 BOOT_CAPTBL_SELF_COMP, (vaddr_t)cos_get_heap_ptr(), CHILD_HIER_FREE);

#ifndef STANDALONE_TEST
	rdtscll(now);
	b = (now >> 32);
	c = ((now << 32)>>32);

	/* sched start time */
	ret = cos_sinv(CHILD_HIER_SINV, SCHED_STARTTIME, b, c, 0);
	sched_start_time = now - (cycles_t)ret;
	
	ret = cos_sinv(CHILD_HIER_SINV, TASK_STARTTIME, b, c, 0);
	task_start_time = now - (cycles_t)ret;
	
//	i = 4;
//	test_loop((void *)i);

	sl_init_sync(sched_start_time, task_start_time);
#else
	sl_init();
#endif

	for (i = 0 ; i < N_CHILD_THDS ; i++) {
		if (i == HPET_IN_CHILD) {
			threads[i] = sl_aepthd_alloc(test_hpetaep_fn, (void *)i);
			//threads[i] = sl_aepthd_tcap_alloc(test_hpetaep_fn, (void *)i, 
			//				  sl_thd_aep(sl__globals()->sched_thd)->tc);
			assert(threads[i]);

			ret = cos_sinv(CHILD_HIER_SINV, HPET_RCVCAP, sl_thd_aep(threads[i])->rcv, 0, 0);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = MS_TO_US(child_thds_T[i]);
			sl_thd_param_set(threads[i], sp.v);
			
			sp.c.type  = SCHEDP_BUDGET;
			sp.c.value = MS_TO_US(child_thds_C[i]);
			sl_thd_param_set(threads[i], sp.v);

			assert(MS_TO_US(child_thds_T[i]) == HPET_PERIOD_USEC);
		} else {
			threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
			assert(threads[i]);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = MS_TO_US(child_thds_T[i]);
			sl_thd_param_set(threads[i], sp.v);
		}
		sp.c.type = SCHEDP_WEIGHT;
		sp.c.value = MS_TO_US(child_thds_C[i]);
		sl_thd_param_set(threads[i], sp.v);
	}

#ifndef STANDALONE_TEST
	/* Because of boot up scheduling.. I'm sure there are events in the queue! Clear them! */
	cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, RCV_ALL_PENDING, &rcvd, &tid, &blocked, &cycles);

	ret = cos_sinv(CHILD_HIER_SINV, CHILD_BOOTUP_DONE, 0, 0, 0);
#endif
//	if (cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_aep(threads[HPETAEP_THD])->rcv, MS_TO_US(T_array[HPETAEP_THD]))) assert(0);
	
	e = sl_mod_get_task_starttime();

	rdtscll(s);
	while (s < e) rdtscll(s);
	if (cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_aep(threads[HPET_IN_CHILD])->rcv, MS_TO_US(child_thds_T[HPET_IN_CHILD]))) assert(0);

	sl_sched_loop();

	assert(0);

	return;
}
