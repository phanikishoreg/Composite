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

#define N_TOTALTHDS 5
#define N_TESTTHDS (N_TOTALTHDS - 1)

#define HPETAEP_THD (N_TESTTHDS - 1)
microsec_t T_array[N_TOTALTHDS] = { 200, 200, 200, 200, 1000};
microsec_t C_array[N_TOTALTHDS] = { 30, 60, 60, 20, 200};
microsec_t W_array[N_TOTALTHDS] = { 25, 55, 50, 20, 190}; /* actual spin work! not including printing, blocking overheads */

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

		//printc("h=%u:%d", tid, rcvd);
	}

	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);
}

void
test_thd_fn(void *data)
{
	cycles_t now, prev;
	thdid_t tid = cos_thdid();

	while (1) {
		microsec_t workusecs = W_array[(int)data];
	
		rdtscll(prev);
		spin_usecs(workusecs);
		rdtscll(now);

//		if (now - prev > C_array[(int)data]) {
//			printc("h=%u", tid);
//		}

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

		if (now - prev > C_array[(int)data]) {
			printc("h=%u", ctid);
		}

	}
}

void
cos_init(void)
{
	int                     i;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp;

//	printc("EDF!!\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();


//	i = 4;
//	test_loop((void *)i);

	sl_init();

	for (i = 0 ; i < N_TESTTHDS-1 ; i++) {
		if (i == HPETAEP_THD) {
			threads[i] = sl_aepthd_tcap_alloc(test_hpetaep_fn, (void *)i, 
							  sl_thd_aep(sl__globals()->sched_thd)->tc);
			assert(threads[i]);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = T_array[i];
			sl_thd_param_set(threads[i], sp.v);
			
			sp.c.type  = SCHEDP_BUDGET;
			sp.c.value = C_array[i];
			sl_thd_param_set(threads[i], sp.v);
		} else {
			threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
			assert(threads[i]);

			sp.c.type  = SCHEDP_WINDOW;
			sp.c.value = T_array[i];
			sl_thd_param_set(threads[i], sp.v);
		}
	}

	sl_sched_loop();

	assert(0);

	return;
}
