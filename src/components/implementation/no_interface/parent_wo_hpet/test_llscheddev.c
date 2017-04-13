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

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)

#define N_TESTTHDS0 3
#define N_TESTTHDS 3
#define MS_TO_US(m)   (m * 1000)

microsec_t T_array[N_TESTTHDS0] = { 10, 30, 40};
microsec_t C_array[N_TESTTHDS0] = { 1, 3, 4,};
microsec_t W_array[N_TESTTHDS0] = { 990, 2990, 3990}; /* actual spin work in usecs! not including printing and blocking overheads */
u32_t prio_array[N_TESTTHDS0]   = { 1, 3, 4};

void
test_thd_fn(void *data)
{
	struct sl_thd *t = sl_thd_curr();
	struct sl_thd_policy *tp = sl_mod_thd_policy_get(t);
	thdid_t tid = t->thdid;
	cycles_t now, prev;

	while (1) {
		microsec_t workusecs = W_array[(int)data];

#ifdef SL_DEBUG_DEADLINES
		int missed = 0;

		missed = spin_usecs_dl(workusecs, tp->deadline);
//		if (missed) {
//			tp->missed ++;
//			dl_missed ++;
//		} else {
//			tp->made ++;
//			dl_made ++;
//		}
#else
		spin_usecs(workusecs);
#endif

//		if (now - prev > sl_usec2cyc(C_array[(int)data])) {
//			printc(" l=%u ", tid);
//		}

		sl_thd_block(0);
	}
}

void
test_llsched_init(void)
{
	int                     i;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp;
	asndcap_t               snd;

//	printc("!!FPDS!!\n");

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
		assert(threads[i]);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = prio_array[i];
		sl_thd_param_set(threads[i], sp.v);
		sp.c.type  = SCHEDP_WINDOW;
		sp.c.value = MS_TO_US(T_array[i]);
		sl_thd_param_set(threads[i], sp.v);
	}

//	cycles_t now;
//	rdtscll(now);
//	printc("%llu\n", now);
//	while (1);
	sl_sched_loop();

	assert(0);

	return;
}
