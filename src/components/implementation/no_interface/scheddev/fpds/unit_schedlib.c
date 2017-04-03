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

#define N_TESTTHDS 8

#define DS_E 5
#define DS_C 10
#define DS_T 1000
#define AEPTHD (N_TESTTHDS-2)
#define SNDTHD (N_TESTTHDS-1)

#define WORKUSECS  DS_E
#define WORKDLSECS DS_T
static int iters_per_usec;

void
test_thd_fn(void *data)
{
	thdid_t tid = cos_thdid();

	while (1) {
		printc("%d", (int)data);
		microsec_t workcycs = WORKUSECS;
		cycles_t   deadline, now;

		rdtscll(now);
		deadline = now + sl_usec2cyc(WORKDLSECS);
	
		if (spin_usecs_dl(workcycs, deadline)) {
			/* printc("%u:miss", tid); */
		}
		/* TODO: use block! and some way to wakeup! */
		sl_thd_yield(0);
	}
}

void
test_aepthd_fn(arcvcap_t rcv, void *data)
{
	thdid_t tid = cos_thdid();

	while (1) {
		printc("%d", AEPTHD);
		cos_rcv(rcv, 0, NULL);

		spin_usecs(DS_E);
	}
}

void
test_sndthd_fn(void *data)
{
	thdid_t tid = cos_thdid();
	asndcap_t snd = (asndcap_t)data;

	while (1) {
		spin_usecs(DS_T);

		printc("%d", SNDTHD);
		cos_asnd(snd, 0);
		sl_thd_yield(0);
	}
}

void
cos_init(void)
{
	int                     i;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp    = {.c = {.type = SCHEDP_PRIO, .value = 10}}, sp1;
	asndcap_t               snd;

	printc("Unit-test for the scheduling library (sl) with fpds mod\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init();
	iters_per_usec = spin_iters_per_usec();
	printc("Iterations per microsecond = %d\n", iters_per_usec);

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		switch(i) {
		case AEPTHD:
		{
			threads[i] = sl_aepthd_alloc(test_aepthd_fn, NULL);
			assert(threads[i]);
			sl_thd_param_set(threads[i], sp.v);
			sp1.c.type  = SCHEDP_WINDOW;
			sp1.c.value = DS_T;
			sl_thd_param_set(threads[i], sp1.v);
			sp1.c.type  = SCHEDP_BUDGET;
			sp1.c.value = DS_C;
			sl_thd_param_set(threads[i], sp1.v);

			break;
		}
		case SNDTHD:
		{
			assert (SNDTHD > AEPTHD);
			snd = cos_asnd_alloc(ci, sl_thd_aep(threads[AEPTHD])->rcv, ci->captbl_cap);
			assert(snd);

			threads[i] = sl_thd_alloc(test_sndthd_fn, (void *)snd);
			assert(threads[i]);
			sl_thd_param_set(threads[i], sp.v);

			break;
		}
		default:
		{
			threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
			assert(threads[i]);
			sl_thd_param_set(threads[i], sp.v);

			break;
		}
		}
	}

	sl_sched_loop();

	assert(0);

	return;
}