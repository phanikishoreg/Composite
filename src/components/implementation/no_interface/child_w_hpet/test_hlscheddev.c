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

#define MS_TO_US(m) (m*1000)
#define N_TOTALTHDS 4
#define N_TESTTHDS (3)//N_TOTALTHDS - 1)

#define HPETAEP_THD (10)
microsec_t T_array[N_TOTALTHDS] = { HPET_PERIOD_USEC/1000, 60, 80, 1000}; /* in ms */
microsec_t C_array[N_TOTALTHDS] = { 4, 6, 8, 200}; /* in ms */
microsec_t W_array[N_TOTALTHDS] = { 3900, 5900, 7900, 190}; /* in usecs, actual spin work! not including printing, blocking overheads */

void
test_hpetaep_fn(arcvcap_t rcv, void *data)
{
	int first = 1;
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct sl_thd       *t   = sl_thd_curr();
	struct sl_thd_policy *tp = sl_mod_thd_policy_get(t);
	struct cos_aep_info *aep = sl_thd_aep(t);
	thdid_t              tid = t->thdid;
	cycles_t             now;

//	if (cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, aep->rcv, MS_TO_US(T_array[HPETAEP_THD]))) assert(0);

	while (1) {
		microsec_t workusecs = W_array[(int)data];
		rcv_flags_t flg = RCV_ALL_PENDING;
		int rcvd;
		int ret;

		if (!first && (ret = cos_tcap_transfer(aep->rcv, aep->tc, 0, t->prio))) {
			printc("ret=%d", ret);
			assert(0);
		}
		first = 0;
		cos_rcv(rcv, flg, &rcvd);
		//sl_mod_thd_policy_get(t)->missed += (rcvd - 1);
		spin_usecs(workusecs);

		//printc("h=%u:%d", tid, rcvd);
		//sl_mod_block(sl_mod_thd_policy_get(t));
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
		microsec_t workusecs = W_array[(int)data];
	
		missed = spin_usecs_dl(workusecs, tp->deadline);
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
	thdid_t ctid = cos_thdid();
	while (1) {
		cycles_t now, prev;
		cycles_t cycles;
		thdid_t tid;
		int pending, rcvd, blocked;
		rcv_flags_t rf_use = 0;
		microsec_t workusecs = W_array[(int)data];

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
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp;
	cycles_t                sched_start_time, task_start_time, now;
	unsigned int b = 0, c = 0;

//	printc("EDF!!\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	//cos_defcompinfo_init();
        cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_BASE, BOOT_CAPTBL_SELF_INITTHD_BASE, 
                                 BOOT_CAPTBL_SELF_INITRCV_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
                                 BOOT_CAPTBL_SELF_COMP, (vaddr_t)cos_get_heap_ptr(), CHILD_HIER_FREE);

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

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		if (i == HPETAEP_THD) {
			threads[i] = sl_aepthd_alloc(test_hpetaep_fn, (void *)i);
			//threads[i] = sl_aepthd_tcap_alloc(test_hpetaep_fn, (void *)i, 
			//				  sl_thd_aep(sl__globals()->sched_thd)->tc);
			assert(threads[i]);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = MS_TO_US(T_array[i]);
			sl_thd_param_set(threads[i], sp.v);
			
			sp.c.type  = SCHEDP_BUDGET;
			sp.c.value = MS_TO_US(C_array[i]);
			sl_thd_param_set(threads[i], sp.v);

			assert(MS_TO_US(T_array[i]) == HPET_PERIOD_USEC);
			if (cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_aep(threads[i])->rcv, MS_TO_US(T_array[i]))) assert(0);
		} else {
			threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
			assert(threads[i]);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = MS_TO_US(T_array[i]);
			sl_thd_param_set(threads[i], sp.v);
		}
	}

	sl_sched_loop();

	assert(0);

	return;
}
