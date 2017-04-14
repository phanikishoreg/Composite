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

#define N_TESTTHDS 3
#define HPETAEP_THD (N_TESTTHDS - 1)

microsec_t T_array[N_TESTTHDS] = { 1000, 1000, 1000};
microsec_t C_array[N_TESTTHDS] = { 50, 50, 50};
microsec_t W_array[N_TESTTHDS] = { 45, 45, 45}; /* actual spin work! not including printing and blocking overheads */
u32_t prio_array[N_TESTTHDS]   = { 1, 3, 4};
static asndcap_t child_hpet_asnd = 0;

struct comp_cap_info;
extern struct cos_compinfo *boot_ci;
extern struct comp_cap_info new_comp_cap_info[]; 

extern void *__inv_hierchild_serverfn(int a, int b, int c);

int
hierchild_serverfn(int a, int b, int c)
{
	int ret;
	int spdid = 1;
	asndcap_t asnd_in_child = a;

	switch(a) {
	case HPET_SNDCAP:
	{
		struct cos_compinfo *ci       = boot_ci;
		struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;

		child_hpet_asnd = cos_cap_cpy(ci, child_ci, CAP_ASND, asnd_in_child);
		assert(child_hpet_asnd);

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
test_hpetaep_fn(arcvcap_t rcv, void *data)
{
	struct sl_thd       *t   = sl_thd_curr();
	struct cos_aep_info *aep = sl_thd_aep(t);
	thdid_t              tid = t->thdid;

	if (cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, aep->rcv, T_array[HPETAEP_THD])) assert(0);

	while (1) {
		rcv_flags_t flg = RCV_ALL_PENDING;
		int rcvd;

		cos_rcv(rcv, flg, &rcvd);

		if (child_hpet_asnd) cos_asnd(child_hpet_asnd, 1);
	}

	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);
}

void
test_thd_fn(void *data)
{
	thdid_t tid = cos_thdid();

	while (1) {
		microsec_t workusecs = W_array[(int)data];

	//	printc(" l=%u ", tid);
		spin_usecs(workusecs);

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

	hier_child_setup(1);
//	printc("!!FPDS!!\n");

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		if (i == HPETAEP_THD) { 
			threads[i] = sl_aepthd_tcap_alloc(test_hpetaep_fn, (void *)i, 
							  sl_thd_aep(sl__globals()->sched_thd)->tc);
		} else {
			threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
		}
		assert(threads[i]);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = prio_array[i];
		sl_thd_param_set(threads[i], sp.v);
		sp.c.type  = SCHEDP_WINDOW;
		sp.c.value = T_array[i];
		sl_thd_param_set(threads[i], sp.v);
	}

	sl_sched_loop();

	assert(0);

	return;
}
