#include "vk_types.h"
#include "micro_booter.h"
int test_running = 1;

extern struct vms_info vmx_info[];
extern struct vkernel_info vk_info;

long long avg_thd_switch_cycles;

#define TEST_MAX_DELEGS (TCAP_MAX_DELEGATIONS - 2)
thdcap_t thds[TEST_MAX_DELEGS];
arcvcap_t rcvs[TEST_MAX_DELEGS];
tcap_t tcs[TEST_MAX_DELEGS];

static void
tcap_test_fn(void *d)
{
	while (1) cos_thd_switch(vk_info.testthd);
}

static void
test_tcap_init(void)
{
	int i, ret;
	asndcap_t snd;

	for (i = 0 ; i < TEST_MAX_DELEGS ; i ++) {
		tcs[i] = cos_tcap_alloc(&vk_info.cinfo, TCAP_PRIO_MAX);
		assert(tcs[i]);

		thds[i] = cos_thd_alloc(&vk_info.cinfo, vk_info.cinfo.comp_cap, tcap_test_fn, (void *)i);
		assert(thds[i]);

		rcvs[i] = cos_arcv_alloc(&vk_info.cinfo, thds[i], tcs[i], vk_info.cinfo.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(rcvs[i]);

		ret = cos_tcap_transfer(rcvs[i], BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
		assert(ret == 0);
	}
}

static void
test_tcap_prep(int ndelegs)
{
	int i, j, ret;

	if (ndelegs < 4) return;
	printc("TCAPS-PREP NDELEGS:%d\n", ndelegs);

	for (i = 0 ; i < ndelegs-3 ; i ++) {
		for (j = 0 ; j < ndelegs-3 ; j ++) {
			if (i == j) continue;
			if ((cos_tcap_transfer(rcvs[i], tcs[j], TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
		}
	}

	if ((cos_tcap_transfer(vk_info.testarcv, tcs[0], TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
	if ((cos_tcap_transfer(vmx_info[0].testarcv, tcs[0], TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
	if ((cos_tcap_transfer(vk_info.testarcv, vmx_info[0].testtcap, TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
	if ((cos_tcap_transfer(vmx_info[0].testarcv, vk_info.testtcap, TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);

	if (cos_tcap_delegate(vmx_info[0].testasnd, vmx_info[0].testtcap, TCAP_RES_INF, TCAP_PRIO_MAX, TCAP_DELEG_YIELD)) assert(0);	
}

static void
test_tcap_deleg(int yield)
{
	long long start_cycles, end_cycles, total_cycles;
	long long avg_deleg_cycles;
	int i;

	total_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		rdtscll(start_cycles);
		if(cos_tcap_delegate(vk_info.testasnd[0], vk_info.testtcap, TCAP_RES_INF, TCAP_PRIO_MAX, yield)) assert(0);
		rdtscll(end_cycles);

		total_cycles += (end_cycles - start_cycles);
	}

	avg_deleg_cycles = (total_cycles / (long long) ITER);
	avg_deleg_cycles -= avg_thd_switch_cycles;

	printc("Average Tcap-delegate %s (Iterations: %lld ): %lld\n", yield ? "w/ yield" : "w/o yield",
		(long long)ITER, avg_deleg_cycles); 
}

static void
test_tcaps(void)
{
	int ndelegs[] = { 4, 8, 16, 64 }, i;

	test_tcap_init();
	for (i = 0 ; i < (int)(sizeof(ndelegs)/sizeof(ndelegs[0])) ; i ++) {
		test_tcap_prep(ndelegs[i]);
		test_tcap_deleg(1);
		test_tcap_deleg(0);
	}
}

static void
test_thds(void)
{
	int i;
	long long start_cycles, end_cycles, total_cycles;

	total_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		rdtscll(start_cycles);
		cos_thd_switch(vmx_info[0].testthd);
		rdtscll(end_cycles);

		total_cycles += (end_cycles - start_cycles);
	}
	total_cycles /= 2;
	avg_thd_switch_cycles = (total_cycles / (long long) ITER);
	printc("Average THD SWTCH (Total: %lld / Iterations: %lld ): %lld\n",
		total_cycles, (long long) ITER, avg_thd_switch_cycles);
}

static void
test_async(void)
{
	int i;
	long long start_cycles, end_cycles, total_cycles;

	total_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		rdtscll(start_cycles);
		cos_asnd(vk_info.testasnd[0]);
		rdtscll(end_cycles);

		total_cycles += (end_cycles - start_cycles);
	}
	total_cycles /= 2;
	printc("Average ASND/ARCV (Total: %lld / Iterations: %lld ): %lld\n",
		total_cycles, (long long) (ITER), (total_cycles / (long long)(ITER)));
}

static void
test_sinv(void)
{
	/* TODO: */
/*	sinvcap_t ic;
	int i;
	long long total_cycles = 0, total_inv_cycles = 0, total_ret_cycles = 0;

	ic = cos_sinv_alloc(&vk_info.cinfo, vmx_info[0].cinfo.comp_cap, (vaddr_t)__inv_test_serverfn);
	assert(ic);

	for (i = 0 ; i < ITER ; i ++) {
		midinv_cycles = 0;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);
		total_inv_cycles += (midinv_cycles - start_cycles);
		total_ret_cycles += (end_cycles - midinv_cycles);
	}
*/
}

void
vk_test_fn(void *d)
{
	test_running = 1;

	test_thds();

	test_async();

	test_tcaps();

	test_running = 0;
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
vm_test_fn(void *d)
{
	int i;

	for (i = 0 ; i < ITER ; i ++) {
		cos_thd_switch(VM_CAPTBL_TESTVKTHD_BASE);
	}
	cos_thd_switch(VM_CAPTBL_TESTVKTHD_BASE);

	for (i = 0 ; i < ITER ; i ++) {
		cos_rcv(VM_CAPTBL_TESTRCV_BASE);
	}

	while(1) cos_thd_switch(VM_CAPTBL_TESTVKTHD_BASE);
}
