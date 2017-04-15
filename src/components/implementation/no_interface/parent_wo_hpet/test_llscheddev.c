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
#include "comp_cap_info.h"

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)

static asndcap_t child_hpet_asnd = 0;
static int child_bootup_done = 0;

struct comp_cap_info;
extern struct cos_compinfo *boot_ci;
extern struct comp_cap_info new_comp_cap_info[]; 

extern void *__inv_hierchild_serverfn(int a, int b, int c);

int
hierchild_serverfn(int a, int b, int c)
{
	int ret;
	int spdid = 1;

	switch(a) {
	case HPET_SNDCAP:
	{
		asndcap_t asnd_in_child = b;
		struct cos_compinfo *ci       = boot_ci;
		struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;

		child_hpet_asnd = cos_cap_cpy(ci, child_ci, CAP_ASND, asnd_in_child);
		assert(child_hpet_asnd);

		break;
	}
	case HPET_RCVCAP:
	{
		struct cos_compinfo *ci       = boot_ci;
		struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;
		arcvcap_t child_hpet_rcv, rcv_in_child = b;

		child_hpet_rcv = cos_cap_cpy(ci, child_ci, CAP_ARCV, rcv_in_child);
		assert(child_hpet_rcv);
		cos_deftransfer(child_hpet_rcv, sl_usec2cyc(8*1000), sl_childsched_get()->prio); 

		break;
	}
	case TASK_STARTTIME:
	{
		cycles_t task_start_time = sl_mod_get_task_starttime(), child_now;
		unsigned int diff;

		child_now = (((u64_t)b)<<32) | ((u64_t)c);
		diff = (unsigned int)(child_now - task_start_time);

		return diff;
	}
	case SCHED_STARTTIME:
	{
		cycles_t start_time = sl__globals()->start_time, child_now;
		unsigned int diff;

		child_now = (((u64_t)b)<<32) | ((u64_t)c);
		diff = (unsigned int)(child_now - start_time);

		return diff;
	}
	case CHILD_BOOTUP_DONE:
	{
		child_bootup_done = 1;

		break;
	}
	default: assert(0);
	}

	return 0;
}
 
void
hier_child_setup(int spdid)
{
	int ret;
	sinvcap_t child_hier_sinv;
        struct cos_compinfo *ci       = boot_ci;
        struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;

	child_hier_sinv = cos_sinv_alloc(ci, ci->comp_cap, (vaddr_t)__inv_hierchild_serverfn);
	assert(child_hier_sinv);

	ret = cos_cap_cpy_at(child_ci, CHILD_HIER_SINV, ci, child_hier_sinv);
	assert(ret == 0);
}

void
test_thd_fn(void *data)
{
	struct sl_thd *t = sl_thd_curr();
	struct sl_thd_policy *tp = sl_mod_thd_policy_get(t);
	thdid_t tid = t->thdid;
	cycles_t now, prev;

	while (1) {
		microsec_t workusecs = parent_thds_W[(int)data];

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
	struct sl_thd          *threads[N_PARENT_THDS];
	union sched_param       sp;
	struct sl_thd          *child_thd;
	cycles_t s, e;

	hier_child_setup(1);
//	printc("!!FPDS!!\n");

	for (i = 0 ; i < N_PARENT_THDS ; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
		assert(threads[i]);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = parent_thds_Prio[i];
		sl_thd_param_set(threads[i], sp.v);
		sp.c.type  = SCHEDP_WINDOW;
		sp.c.value = MS_TO_US(parent_thds_T[i]);
		sl_thd_param_set(threads[i], sp.v);
	}

//	cycles_t now;
//	rdtscll(now);
//	printc("%llu\n", now);
//	while (1);

	child_thd = sl_childsched_get();
	assert(child_thd);
	
	while (!child_bootup_done) {
		tcap_res_t child_budget = (tcap_res_t)cos_introspect(ci, sl_thd_aep(child_thd)->tc, TCAP_GET_BUDGET);

		if (child_budget == 0) {
			if(cos_defdelegate(child_thd->sndcap, (tcap_res_t)sl_usec2cyc(10*1000), child_thd->prio, TCAP_DELEG_YIELD)) assert(0);
		} else {
			if(cos_asnd(child_thd->sndcap, 1)) assert(0);
		}
	}

	e = sl_mod_get_task_starttime() - sl_usec2cyc(10000);

	rdtscll(s);
	while (s < e) rdtscll(s);
	
	sl_sched_loop();

	assert(0);

	return;
}
