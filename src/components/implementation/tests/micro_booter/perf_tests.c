#include "micro_booter.h"

extern unsigned int cyc_per_usec;

#define INTR_LATENCY_CYCS 100
#define INTR_LATENCY_ITER 1000000
static void
spinner_cycs1(void *d)
{
	cycles_t *p = (cycles_t *)d;

	while (1) rdtscll(*p);
}

#define TEST_USEC_INTERVAL 100 /* in microseconds */

static long long spin_cycs = 0LL;

static void
test_intr_latency_perf(void)
{
	thdcap_t ts;
	int i;
	long long intr_cycs = 0LL;
	long long total_cycles = 0LL;
	long long wcet_cycles = 0LL, pwcet_cycles = 0LL;
	cycles_t now;
	tcap_time_t timer;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner_cycs1, (void *)&spin_cycs);
	assert(ts);

//	cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, BOOT_CAPTBL_SELF_INITRCV_BASE, TEST_USEC_INTERVAL);

//	rdtscll(now);
//	timer = tcap_cyc2time(now + INTR_LATENCY_CYCS * cyc_per_usec);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < INTR_LATENCY_ITER ; i++) {
		cycles_t cycles;
		thdid_t tid;
		int blocked;
		long long curr_cycles = 0LL;

		//rdtscll(now);
		//timer = tcap_cyc2time(now + INTR_LATENCY_CYCS * cyc_per_usec);
		cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, 0, 0, 0);

		rdtscll(intr_cycs);
		curr_cycles = (intr_cycs - spin_cycs);
		if (curr_cycles > wcet_cycles) {
			pwcet_cycles = wcet_cycles;
			wcet_cycles = curr_cycles;
		}
		total_cycles += curr_cycles;

	//	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &blocked, &cycles) != 0) ;
	}

	PRINTC("HPET Interrupt Latency WCET:%lld(prev:%lld) Average:%lld (Total:%lld)\n",
		wcet_cycles, pwcet_cycles, (total_cycles / (long long)(INTR_LATENCY_ITER)),
		total_cycles);
}

static void
test_timer_intr_latency_perf(void)
{
	thdcap_t ts;
	int i;
	long long intr_cycs = 0LL;
	long long total_cycles = 0LL;
	long long wcet_cycles = 0LL, pwcet_cycles = 0LL;
	cycles_t now;
	tcap_time_t timer;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner_cycs1, (void *)&spin_cycs);
	assert(ts);

	rdtscll(now);
	timer = tcap_cyc2time(now + INTR_LATENCY_CYCS * cyc_per_usec);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < INTR_LATENCY_ITER ; i++) {
		cycles_t cycles;
		thdid_t tid;
		int blocked, rcvd;
		tcap_time_t timeout = 0, thd_timeout;
		long long curr_cycles = 0LL;

		rdtscll(now);
		timer = tcap_cyc2time(now + INTR_LATENCY_CYCS * cyc_per_usec);
		cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, timer, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());

		rdtscll(intr_cycs);
		curr_cycles = (intr_cycs - spin_cycs);
		if (curr_cycles > wcet_cycles) {
			pwcet_cycles = wcet_cycles;
			wcet_cycles = curr_cycles;
		}
		total_cycles += curr_cycles;

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, RCV_ALL_PENDING, timeout,
				     &rcvd, &tid, &blocked, &cycles, &thd_timeout) > 0) ;
	}

	PRINTC("Timer Interrupt Latency WCET:%lld(prev:%lld) Average:%lld (Total:%lld)\n",
		wcet_cycles, pwcet_cycles, (total_cycles / (long long)(INTR_LATENCY_ITER)),
		total_cycles);
}

static int apic_timer_overhead_test = 0;
#define TIMEOUT_CYCS 1000000
static cycles_t timeout = 0;

static void
thd_fn_perf(void *d)
{
	cycles_t now;
	tcap_time_t timer;

	assert(timeout);
	rdtscll(now);
	timer = tcap_cyc2time(now + timeout);
	cos_switch(BOOT_CAPTBL_SELF_INITTHD_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, apic_timer_overhead_test ? timer : TCAP_TIME_NIL, 0, 0);

	while (1) {
		rdtscll(now);
		timer = tcap_cyc2time(now + timeout);
		cos_switch(BOOT_CAPTBL_SELF_INITTHD_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, apic_timer_overhead_test ? timer : TCAP_TIME_NIL, 0, 0);
	}
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_perf(int with_apic_timer_prog)
{
	thdcap_t ts;
	long long total_swt_cycles = 0;
	long long start_swt_cycles = 0, end_swt_cycles = 0;
	long long curr_swt_cycles = 0;
	long long wcet_swt_cycles = 0, pwcet_swt_cycles = 0;
	int i;
	cycles_t now;
	tcap_time_t timer;

	timeout = (cycles_t)TIMEOUT_CYCS * (cycles_t)cyc_per_usec;
	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_perf, NULL);
	assert(ts);

	if (with_apic_timer_prog) apic_timer_overhead_test = 1;
	else apic_timer_overhead_test = 0;

	rdtscll(now);
	timer = tcap_cyc2time(now + timeout);
	cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, apic_timer_overhead_test ? timer : TCAP_TIME_NIL, 0, 0);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start_swt_cycles);

		rdtscll(now);
		timer = tcap_cyc2time(now + timeout);
		cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, apic_timer_overhead_test ? timer : TCAP_TIME_NIL, 0, 0);
		rdtscll(end_swt_cycles);
		curr_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;
		
		total_swt_cycles += curr_swt_cycles;
		if (curr_swt_cycles > wcet_swt_cycles) {
			pwcet_swt_cycles = wcet_swt_cycles;
			wcet_swt_cycles = curr_swt_cycles;
		}
	}

	PRINTC("THD SWTCH WCET:%lld(prev:%lld), Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_swt_cycles, pwcet_swt_cycles, total_swt_cycles, (long long) ITER, (total_swt_cycles / (long long)ITER));
}

//static void
//thd_fn_perf(void *d)
//{
//	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
//
//	while (1) {
//		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
//	}
//	PRINTC("Error, shouldn't get here!\n");
//}
//
//static void
//test_thds_perf(void)
//{
//	thdcap_t ts;
//	long long total_swt_cycles = 0;
//	long long start_swt_cycles = 0, end_swt_cycles = 0;
//	long long curr_swt_cycles = 0;
//	long long wcet_swt_cycles = 0, pwcet_swt_cycles = 0;
//	int i, iter = 0;
//
//	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_perf, NULL);
//	assert(ts);
//	cos_thd_switch(ts);
//
//	for (i = 0 ; i < ITER ; i++) {
//		rdtscll(start_swt_cycles);
//		cos_thd_switch(ts);
//		rdtscll(end_swt_cycles);
//		curr_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;
//		
//		total_swt_cycles += curr_swt_cycles;
//		if (curr_swt_cycles > wcet_swt_cycles) {
//			iter = i;
//			pwcet_swt_cycles = wcet_swt_cycles;
//			wcet_swt_cycles = curr_swt_cycles;
//		}
//	}
//
//	PRINTC("THD SWTCH WCET:%lld:%d:%lld, Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_swt_cycles, iter, pwcet_swt_cycles, total_swt_cycles, (long long) ITER, (total_swt_cycles / (long long)ITER));
//}

static volatile arcvcap_t rcc_global, rcp_global;
static volatile asndcap_t scp_global;
static int async_test_flag = 0;

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	int i;

	cos_rcv(rc, 0, NULL);

	for (i = 0 ; i < ITER+1 ; i++) {
		cos_rcv(rc, 0, NULL);
	}

	assert(0);
	cos_thd_switch(tc);
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	long long total_asnd_cycles = 0, curr_asnd_cycles = 0;
	long long wcet_asnd_cycles = 0, pwcet_asnd_cycles = 0;
	long long start_asnd_cycles = 0, end_arcv_cycles = 0;
	int i, iter = 0;

	cos_asnd(sc, 1);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start_asnd_cycles);
		cos_asnd(sc, 1);
		rdtscll(end_arcv_cycles);

		curr_asnd_cycles = (end_arcv_cycles - start_asnd_cycles) / 2;
		total_asnd_cycles += curr_asnd_cycles;
		if (curr_asnd_cycles > wcet_asnd_cycles) {
			iter = i;
			pwcet_asnd_cycles = wcet_asnd_cycles;
			wcet_asnd_cycles = curr_asnd_cycles;
		}
	}

	PRINTC("ASND/ARCV WCET:%llu:%d:%llu, Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_asnd_cycles, iter, pwcet_asnd_cycles, total_asnd_cycles, (long long) (ITER), (total_asnd_cycles / (long long)(ITER)));

	async_test_flag = 0;
	while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints_perf(void)
{
	thdcap_t tcp, tcc;
	tcap_t tccp, tccc;
	arcvcap_t rcp, rcc;

	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcp);
	tccp = cos_tcap_alloc(&booter_info);
	assert(tccp);
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);
	if (cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1)) assert(0);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void*)tcp);
	assert(tcc);
	tccc = cos_tcap_alloc(&booter_info);
	assert(tccc);
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	assert(rcc);
	if (cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX)) assert(0);

	/* make the snd channel to the child */
	scp_global = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global);

	rcc_global = rcc;
	rcp_global = rcp;

	async_test_flag = 1;
	while (async_test_flag) cos_thd_switch(tcp);
}


static cycles_t child_cycs = 0;
static arcvcap_t child_rcv = 0;
static void
child_thd_fn(void *d)
{
	while(1) {
		rdtscll(child_cycs);
		cos_rcv(child_rcv, 0, NULL);
	} 
}

static void
test_sched_activation_perf(void)
{
	long long total_sched_cycles = 0, curr_sched_cycles = 0;
	long long wcet_sched_cycles = 0, pwcet_sched_cycles = 0;
	long long end_sched_cycles = 0;
	int i;
	thdcap_t tcc;
	asndcap_t sc;

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, child_thd_fn, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcc);
	child_rcv = cos_arcv_alloc(&booter_info, tcc, BOOT_CAPTBL_SELF_INITTCAP_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(child_rcv);

	/* make the snd channel to the child */
	sc = cos_asnd_alloc(&booter_info, child_rcv, booter_info.captbl_cap);
	assert(sc);

	cos_asnd(sc, 1);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < ITER ; i++) {
		cos_asnd(sc, 1);
		rdtscll(end_sched_cycles);

		curr_sched_cycles = (end_sched_cycles - child_cycs);
		total_sched_cycles += curr_sched_cycles;
		if (curr_sched_cycles > wcet_sched_cycles) {
			pwcet_sched_cycles = wcet_sched_cycles;
			wcet_sched_cycles = curr_sched_cycles;
		}
	}

	PRINTC("Sched Activation WCET:%llu(prev:%llu), Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_sched_cycles, pwcet_sched_cycles, total_sched_cycles, (long long) (ITER), (total_sched_cycles / (long long)(ITER)));


}

extern long long midinv_cycles;

extern void *__inv_test_serverfn(int a, int b, int c);

static inline
int call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (arg1), "S" (arg2), "D" (arg3) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

static void
test_inv_perf(void)
{
	compcap_t cc;
	sinvcap_t ic;
	int i;
	long long total_cycles = 0LL;
	long long wcet_inv_cycles = 0, wcet_ret_cycles = 0;
	long long pwcet_inv_cycles = 0, pwcet_ret_cycles = 0;
	long long curr_inv_cycles = 0, curr_ret_cycles = 0;
	long long total_inv_cycles = 0LL, total_ret_cycles = 0LL;
	unsigned int ret;
	int iiter = 0, riter = 0;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);
	ret = call_cap_mb(ic, 1, 2, 3);
	assert(ret == 0xDEADBEEF);
	cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);

	for (i = 0 ; i < ITER ; i++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		midinv_cycles = 0LL;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);
		curr_inv_cycles = (midinv_cycles - start_cycles);
		curr_ret_cycles = (end_cycles - midinv_cycles);
		total_inv_cycles += curr_inv_cycles;
		total_ret_cycles += curr_ret_cycles;

		if (curr_inv_cycles > wcet_inv_cycles) {
			iiter = i;
			pwcet_inv_cycles = wcet_inv_cycles;
			wcet_inv_cycles = curr_inv_cycles;
		}

		if (curr_ret_cycles > wcet_ret_cycles) {
			riter = i;
			pwcet_ret_cycles = wcet_ret_cycles;
			wcet_ret_cycles = curr_ret_cycles;
		}
	}

	PRINTC("SINV WCET:%llu:%d:%llu, Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_inv_cycles, iiter, pwcet_inv_cycles, total_inv_cycles, (long long) (ITER), (total_inv_cycles / (long long)(ITER)));
	PRINTC("SRET WCET:%llu:%d:%llu, Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_ret_cycles, riter, pwcet_ret_cycles, total_ret_cycles, (long long) (ITER), (total_ret_cycles / (long long)(ITER)));
}

/* Make sure TCAP_MAX_DELEGATIONS = TEST_MAX_DELEGS-1 to be able to test upto MAX levels*/
#define TEST_MAX_DELEGS (15)
static volatile tcap_t tcs[TEST_MAX_DELEGS];
static volatile thdcap_t thds[TEST_MAX_DELEGS];
static volatile arcvcap_t rcvs[TEST_MAX_DELEGS];
static volatile asndcap_t asnds[TEST_MAX_DELEGS][TEST_MAX_DELEGS];
static volatile long long total_tcap_cycles = 0, start_tcap_cycles = 0, end_tcap_cycles = 0;

static void
tcap_test_fn(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	while (1) {
		rdtscll(end_tcap_cycles);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

static void
tcap_perf_test_prepare(void)
{
	int i, j, ret;

	memset(tcs, 0, sizeof(tcap_t)*TEST_MAX_DELEGS);
	memset(thds, 0, sizeof(thdcap_t)*TEST_MAX_DELEGS);
	memset(rcvs, 0, sizeof(arcvcap_t)*TEST_MAX_DELEGS);
	memset(asnds, 0, sizeof(asndcap_t)*TEST_MAX_DELEGS*TEST_MAX_DELEGS);
	/* Preperation for tcaps ubenchmarks */
	for (i = 0 ; i < TEST_MAX_DELEGS ; i ++) {
		tcs[i] = cos_tcap_alloc(&booter_info);
		assert(tcs[i]);
		
		thds[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, tcap_test_fn, (void *)i);
		assert(thds[i]);
		cos_thd_switch(thds[i]);

		rcvs[i] = cos_arcv_alloc(&booter_info, thds[i], tcs[i], booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(rcvs[i]);

		ret = cos_tcap_transfer(rcvs[i], BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
		assert(ret == 0);
	}

	for (i = 0 ; i < TEST_MAX_DELEGS ; i ++) {
		for (j = i + 1 ; j < TEST_MAX_DELEGS ; j ++) {

			asnds[i][j] = cos_asnd_alloc(&booter_info, rcvs[j], booter_info.captbl_cap);
			assert(asnds[i][j]);

			asnds[j][i] = cos_asnd_alloc(&booter_info, rcvs[i], booter_info.captbl_cap);
			assert(asnds[j][i]);
		}
	}
}

static void
tcap_perf_test_ndeleg(int ndelegs)
{
	int i, j;

	/* TODO: cleanup with for-loop */
	for (i = 1 ; i <= (ndelegs - 1) ; i ++) {
		for (j = 1 ; j <= (ndelegs - 1) ; j ++) {
			if (i == j) continue;
			if ((cos_tcap_transfer(rcvs[i-1], tcs[j-1], TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
		}
	}

	/*
	 * Prep for tcap-deleg ubench
	 * Required because the current tcap being the ROOT tcap, has ndelegs = 1.
	 */
	if (cos_tcap_delegate(asnds[0][1], tcs[0], TCAP_RES_INF, TCAP_PRIO_MAX, TCAP_DELEG_YIELD)) assert(0);
}

static void
tcap_perf_test_transfer(void)
{
	int i;
	long long wcet_tcap_cycles = 0, pwcet_tcap_cycles = 0;

//	perfdata_init(&pd, "Transfer");
	total_tcap_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		long long curr_tcap_cycles = 0;

		start_tcap_cycles = 0;
		end_tcap_cycles = 0;
		rdtscll(start_tcap_cycles);
		if (unlikely(cos_tcap_transfer(rcvs[0], tcs[1], TCAP_RES_INF, TCAP_PRIO_MAX))) assert(0);
		rdtscll(end_tcap_cycles);

		curr_tcap_cycles = (end_tcap_cycles - start_tcap_cycles);
//		perfdata_add(&pd, (double)curr_tcap_cycles);
		if (curr_tcap_cycles > wcet_tcap_cycles) {
			pwcet_tcap_cycles = wcet_tcap_cycles;
			wcet_tcap_cycles = curr_tcap_cycles;
		}
		total_tcap_cycles += curr_tcap_cycles;
	}

//	perfdata_calc(&pd);
//	perfdata_print(&pd);
	//PRINTC("Tcap-transfer WCET:%lld:%d:%lld, Average (Total: %lld / Iterations: %lld ): %lld\n", wcet_tcap_cycles, iter, pwcet_tcap_cycles,
	//	total_tcap_cycles, (long long)ITER, (total_tcap_cycles / (long long)ITER)); 
	PRINTC("Tcap-transfer WCET:%lld(prev:%lld), Average:%lld, Total:%lld\n", wcet_tcap_cycles, pwcet_tcap_cycles,
		(total_tcap_cycles / (long long)ITER), total_tcap_cycles); 
}

static void
tcap_perf_test_delegate(int yield)
{
	int i;
	long long wcet_tcap_cycles = 0, pwcet_tcap_cycles = 0;

	total_tcap_cycles = 0;
//	if (yield) perfdata_init(&pd, "Delegate(Y)");
//	else       perfdata_init(&pd, "Delegate");
	for (i = 0 ; i < ITER ; i ++) {
		long long curr_tcap_cycles = 0;

		start_tcap_cycles = 0;
		end_tcap_cycles = 0;
		rdtscll(start_tcap_cycles);
		if (unlikely(cos_tcap_delegate(asnds[0][1], tcs[0], TCAP_RES_INF, TCAP_PRIO_MAX, yield?TCAP_DELEG_YIELD:0))) assert(0);

		curr_tcap_cycles = (end_tcap_cycles - start_tcap_cycles);
//		perfdata_add(&pd, (double)curr_tcap_cycles);
		if (curr_tcap_cycles > wcet_tcap_cycles) {
			pwcet_tcap_cycles = wcet_tcap_cycles;
			wcet_tcap_cycles = curr_tcap_cycles;
		}
		total_tcap_cycles += curr_tcap_cycles;
	}

//	perfdata_calc(&pd);
//	perfdata_print(&pd);
	PRINTC("Tcap-delegate%s WCET:%lld(prev:%lld), Average:%lld, Total:%lld\n", yield ? "(Y)" : "", 
		wcet_tcap_cycles, pwcet_tcap_cycles,
		(total_tcap_cycles / (long long)ITER), total_tcap_cycles); 
}

static void
test_tcaps_perf(void)
{
	int ndelegs[] = { 3, 4, 8, 16};
	int i;

	tcap_perf_test_prepare();
	for ( i = 0; i < (int)(sizeof(ndelegs)/sizeof(ndelegs[0])); i ++) {
		PRINTC("Tcaps-ubench for ndelegs = %d\n", ndelegs[i]);
		tcap_perf_test_ndeleg(ndelegs[i]);
		cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);
		tcap_perf_test_transfer();
		cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);
		tcap_perf_test_delegate(0);
		cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH);
		tcap_perf_test_delegate(1);
	}
}

/* USE THIS FOR HPET TIMER */
//long long spinner_cycles = 0;
//static void
//spinner_perf(void *d)
//{ while (1) rdtscll(spinner_cycles); }
//
//static void
//test_timer_perf(void)
//{
//	int i;
//	thdcap_t tc;
//	thdid_t tid;
//	int rcving;
//	cycles_t cycles;
//	long long end_cycles, total_cycles = 0;
//	long long wcet_timer_cycles = 0, pwcet_timer_cycles = 0;
//	int iter = 0;
//
//	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner_perf, NULL);
//	assert(tc);
//
//	cos_thd_switch(tc);
//
//	for (i = 0 ; i < ITER ; i++) {
//		long long curr_timer_cycles = 0;
//
//		rdtscll(end_cycles);
//		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
//
//		curr_timer_cycles = (end_cycles - spinner_cycles);
//		total_cycles += curr_timer_cycles;
//		if (curr_timer_cycles > wcet_timer_cycles) {
//			iter = i;
//			pwcet_timer_cycles = wcet_timer_cycles;
//			wcet_timer_cycles = curr_timer_cycles;
//		}
//		cos_thd_switch(tc);
//	}
//
//	PRINTC("Timer-int latency WCET:%lld:%d:%lld, Average (Total: %lld / Iterations: %lld ): %lld\n",
//		wcet_timer_cycles, iter, pwcet_timer_cycles,
//		total_cycles, (long long) ITER, (total_cycles / (long long)ITER));
//}

/* Executed in micro_booter environment */
void
test_run_perf(void)
{
//	test_thds_perf(0);
//	test_thds_perf(1);

//	test_async_endpoints_perf();
//	test_sched_activation_perf();
//
//	test_inv_perf();
//
	test_tcaps_perf();
//	test_intr_latency_perf();
//	test_timer_intr_latency_perf();
}

