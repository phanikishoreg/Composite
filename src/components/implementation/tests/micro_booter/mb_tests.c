#include "micro_booter.h"

static int
fork_comp(struct cos_compinfo *ccinfo)
{
	compcap_t cc;
	/* forking */
	pgtblcap_t ccpt;
	vaddr_t range, addr;

	ccpt = cos_pgtbl_alloc(&booter_info);
	assert(ccpt);

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, ccpt, (vaddr_t)&cos_upcall_entry);
	assert(cc);

	cos_meminfo_init(&ccinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(ccinfo, ccpt, booter_info.captbl_cap, cc,
			(vaddr_t)BOOT_MEM_VM_BASE, BOOT_CAPTBL_FREE, &booter_info);

	range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
	assert(range > 0);
	for (addr = 0 ; addr < range ; addr += PAGE_SIZE) {
		vaddr_t src_pg = (vaddr_t)cos_page_bump_alloc(&booter_info), dst_pg;
		assert(src_pg);

		memcpy((void *)src_pg, (void *)(BOOT_MEM_VM_BASE + addr), PAGE_SIZE);

		dst_pg = cos_mem_alias(ccinfo, &booter_info, src_pg);
		assert(dst_pg);
	}

	return 0;
}

static void
thd_fn_perf(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);

	while(1) {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_perf(void)
{
	thdcap_t ts;
	long long total_swt_cycles = 0;
	long long start_swt_cycles = 0, end_swt_cycles = 0;
	int i;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_perf, NULL);
	assert(ts);
	cos_thd_switch(ts);

	rdtscll(start_swt_cycles);
	for (i = 0 ; i < ITER ; i++) {
		cos_thd_switch(ts);
	}
	rdtscll(end_swt_cycles);
	total_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;

	PRINTC("Average THD SWTCH (Total: %lld / Iterations: %lld ): %lld\n",
		total_swt_cycles, (long long) ITER, (total_swt_cycles / (long long)ITER));
}

static void
thd_fn(void *d)
{
	PRINTC("\tNew thread %d with argument %d, capid %ld\n", cos_thdid(), (int)d, tls_test[(int)d]);
	/* Test the TLS support! */
	assert(tls_get(0) == tls_test[(int)d]);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		assert(ts[i]);
		tls_test[i] = i;
		cos_thd_mod(&booter_info, ts[i], &tls_test[i]);
		PRINTC("switchto %d @ %x\n", (int)ts[i], cos_introspect(&booter_info, ts[i], THD_GET_IP));
		cos_thd_switch(ts[i]);
	}

	PRINTC("test done\n");
}

#define TEST_NPAGES (1024*2) 	/* Testing with 8MB for now */

static void
test_mem(void)
{
	char *p, *s, *t, *prev;
	int i;
	const char *chk = "SUCCESS";

	p = cos_page_bump_alloc(&booter_info);
	assert(p);
	strcpy(p, chk);

	assert(0 == strcmp(chk, p));
	PRINTC("%s: Page allocation\n", p);

	s = cos_page_bump_alloc(&booter_info);
	assert(s);
	prev = s;
	for (i = 0 ; i < TEST_NPAGES ; i++) {
		t = cos_page_bump_alloc(&booter_info);
		assert(t && t == prev + 4096);
		prev = t;
	}
	memset(s, 0, TEST_NPAGES * 4096);
	PRINTC("SUCCESS: Allocated and zeroed %d pages.\n", TEST_NPAGES);
}

volatile arcvcap_t rcc_global, rcp_global;
volatile asndcap_t scp_global;
int async_test_flag = 0;

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	int i;

	cos_rcv(rc);

	for (i = 0 ; i < ITER + 1 ; i++) {
		cos_rcv(rc);
	}

	cos_thd_switch(tc);
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	long long total_asnd_cycles = 0;
	long long start_asnd_cycles = 0, end_arcv_cycles = 0;
	int i;

	cos_asnd(sc);

	rdtscll(start_asnd_cycles);
	for (i = 0 ; i < ITER ; i++) {
		cos_asnd(sc);
	}
	rdtscll(end_arcv_cycles);
	total_asnd_cycles = (end_arcv_cycles - start_asnd_cycles) / 2;

	PRINTC("Average ASND/ARCV (Total: %lld / Iterations: %lld ): %lld\n",
		total_asnd_cycles, (long long) (ITER), (total_asnd_cycles / (long long)(ITER)));

	async_test_flag = 0;
	cos_thd_switch(tc);
}

static void
async_thd_fn(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	int pending;

	PRINTC("Asynchronous event thread handler.\n");
	PRINTC("<-- rcving...\n");
	pending = cos_rcv(rc);
	PRINTC("<-- pending %d\n", pending);
	PRINTC("<-- rcving...\n");
	pending = cos_rcv(rc);
	PRINTC("<-- pending %d\n", pending);
	PRINTC("<-- rcving...\n");
	pending = cos_rcv(rc);
	PRINTC("<-- Error: manually returning to snding thread.\n");
	cos_thd_switch(tc);
	PRINTC("ERROR: in async thd *after* switching back to the snder.\n");
	while (1) ;
}

static void
async_thd_parent(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	int ret, pending;
	thdid_t tid;
	int rcving;
	cycles_t cycles;

	PRINTC("--> sending\n");
	ret = cos_asnd(sc);
	if (ret) PRINTC("asnd returned %d.\n", ret);
	PRINTC("--> Back in the asnder.\n");
	PRINTC("--> sending\n");
	ret = cos_asnd(sc);
	if (ret) PRINTC("--> asnd returned %d.\n", ret);
	PRINTC("--> Back in the asnder.\n");
	PRINTC("--> receiving to get notifications\n");
	pending = cos_sched_rcv(rc, &tid, &rcving, &cycles);
	PRINTC("--> pending %d, thdid %d, rcving %d, cycles %lld\n", pending, tid, rcving, cycles);

	async_test_flag = 0;
	cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
	thdcap_t  tcp,  tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp,  rcc;
	int ret;

	PRINTC("Creating threads, and async end-points.\n");
	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcp);
	tccp = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX + 2);
	assert(tccp);
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);
	if ((ret = cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1))) {
		PRINTC("transfer failed: %d\n", ret);
		assert(0);
	}

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void*)tcp);
	assert(tcc);
	tccc = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX + 1);
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

	PRINTC("Async end-point test successful.\n");
	PRINTC("Test done.\n");
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
	tccp = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX + 2);
	assert(tccp);
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);
	if (cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1)) assert(0);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void*)tcp);
	assert(tcc);
	tccc = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX + 1);
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

#define TEST_MAX_DELEGS (TCAP_MAX_DELEGATIONS - 1)
tcap_t tcs[TEST_MAX_DELEGS];
thdcap_t thds[TEST_MAX_DELEGS];
arcvcap_t rcvs[TEST_MAX_DELEGS];
asndcap_t asnds[TEST_MAX_DELEGS][TEST_MAX_DELEGS];
long long total_tcap_cycles = 0, start_tcap_cycles = 0, end_tcap_cycles = 0;

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

	/* Preperation for tcaps ubenchmarks */
	for (i = 0 ; i < TEST_MAX_DELEGS ; i ++) {
		tcs[i] = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX);
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

	total_tcap_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		start_tcap_cycles = 0;
		end_tcap_cycles = 0;
		rdtscll(start_tcap_cycles);
		if (cos_tcap_transfer(rcvs[0], tcs[1], TCAP_RES_INF, TCAP_PRIO_MAX)) assert(0);
		rdtscll(end_tcap_cycles);
		total_tcap_cycles += (end_tcap_cycles - start_tcap_cycles);
	}
	PRINTC("Average Tcap-transfer (Total: %lld / Iterations: %lld ): %lld\n",
		total_tcap_cycles, (long long)ITER, (total_tcap_cycles / (long long)ITER)); 
}

static void
tcap_perf_test_delegate(int yield)
{
	int i;

	total_tcap_cycles = 0;
	for (i = 0 ; i < ITER ; i ++) {
		start_tcap_cycles = 0;
		end_tcap_cycles = 0;
		rdtscll(start_tcap_cycles);
		if (cos_tcap_delegate(asnds[0][1], tcs[0], TCAP_RES_INF, TCAP_PRIO_MAX, yield)) assert(0);
		total_tcap_cycles += (end_tcap_cycles - start_tcap_cycles);
	}

	PRINTC("Average Tcap-delegate%s (Total: %lld / Iterations: %lld ): %lld\n", yield ? " w/ yield" : "",
		total_tcap_cycles, (long long)ITER, (total_tcap_cycles / (long long)ITER)); 
}

static void
test_tcaps_perf(void)
{
	int ndelegs[] = { 3, 4, 8, 16, 64};
	int i;

	tcap_perf_test_prepare();
	for ( i = 0; i < (int)(sizeof(ndelegs)/sizeof(ndelegs[0])); i ++) {
		PRINTC("Tcaps-ubench for ndelegs = %d\n", ndelegs[i]);
		tcap_perf_test_ndeleg(ndelegs[i]);
		tcap_perf_test_transfer();
		tcap_perf_test_delegate(1);
		tcap_perf_test_delegate(0);
	}
}

static void
spinner(void *d)
{ while (1) ; }

static void
test_timer(void)
{
	int i;
	thdcap_t tc;
	cycles_t c = 0, p = 0, t = 0;

	PRINTC("Starting timer test.\n");
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0 ; i <= 16 ; i++) {
		thdid_t tid;
		int rcving;
		cycles_t cycles;

		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
		cos_thd_switch(tc);
		p = c;
		rdtscll(c);
		if (i > 0) t += c-p;
	}

	PRINTC("\tCycles per tick (10 microseconds) = %lld\n", t/16);

	PRINTC("Timer test completed.\n");
	PRINTC("Success.\n");
}

long long spinner_cycles = 0;
static void
spinner_perf(void *d)
{ while (1) rdtscll(spinner_cycles); }

static void
test_timer_perf(void)
{
	int i;
	thdcap_t tc;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	long long end_cycles, total_cycles = 0;

	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner_perf, NULL);
	assert(tc);

	cos_thd_switch(tc);

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(end_cycles);
		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);

		total_cycles += (end_cycles - spinner_cycles);
		cos_thd_switch(tc);
	}

	PRINTC("Average Timer-int latency (Total: %lld / Iterations: %lld ): %lld\n",
		total_cycles, (long long) ITER, (total_cycles / (long long)ITER));
}

long long midinv_cycles = 0LL;

int
test_serverfn(int a, int b, int c)
{
	rdtscll(midinv_cycles);
	if (a == 0) {
		unsigned int ub = b, uc = c;
		long long start = ((long long) uc) << 32 | (long long) ub;
		return (unsigned int) (midinv_cycles - start);
	}
	return 0xDEADBEEF;
}

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
test_inv(void)
{
	compcap_t cc;
	sinvcap_t ic;
	unsigned int r;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);

	r = call_cap_mb(ic, 1, 2, 3);
	PRINTC("Return from invocation: %x (== DEADBEEF?)\n", r);
	PRINTC("Test done.\n");
}

static void
test_inv_perf(int intra)
{
	compcap_t cc;
	sinvcap_t ic;
	int i;
	long long total_cycles = 0LL;
	long long total_inv_cycles = 0LL, total_ret_cycles = 0LL;
	unsigned int ret;

	if (intra == 1) {
		cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
		assert(cc > 0);
	} else {
		struct cos_compinfo ccinfo;

		ret = fork_comp(&ccinfo);
		assert (ret == 0);
		cc = ccinfo.comp_cap;
	}
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);
	ret = call_cap_mb(ic, 1, 2, 3);
	assert(ret == 0xDEADBEEF);

	for (i = 0 ; i < ITER ; i++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		if (intra) {
			midinv_cycles = 0LL;
			rdtscll(start_cycles);
			call_cap_mb(ic, 1, 2, 3);
			rdtscll(end_cycles);
			total_inv_cycles += (midinv_cycles - start_cycles);
			total_ret_cycles += (end_cycles - midinv_cycles);
		} else {
			unsigned int inv_diff_cycles = 0LL, total_diff_cycles = 0LL;

			rdtscll(start_cycles);
			inv_diff_cycles = call_cap_mb(ic, 0, (unsigned int)((start_cycles << 32) >> 32), (unsigned int)(start_cycles >> 32));
			rdtscll(end_cycles);
			
			total_diff_cycles = (unsigned int) (end_cycles - start_cycles);
			total_inv_cycles += (long long) inv_diff_cycles;
			total_ret_cycles += (long long) (total_diff_cycles - inv_diff_cycles);
		}
	}

	PRINTC("Average %s SINV (Total: %lld / Iterations: %lld ): %lld\n", intra ? "intra-pgtbl" : "inter-pgtbl",
		total_inv_cycles, (long long) (ITER), (total_inv_cycles / (long long)(ITER)));
	PRINTC("Average %s SRET (Total: %lld / Iterations: %lld ): %lld\n", intra ? "intra-pgtbl" : "inter-pgtbl",
		total_ret_cycles, (long long) (ITER), (total_ret_cycles / (long long)(ITER)));
}

void
test_captbl_expand(void)
{
	int i;
	compcap_t cc;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc);
	for (i = 0 ; i < 1024 ; i++) {
		sinvcap_t ic;

		ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
		assert(ic > 0);
	}
	PRINTC("Captbl expand SUCCESS.\n");
}


static void
spin(void)
{
#define SPIN_ITERS 0x7FFFFFFF 
	long long threshold_cycles = 9000;
	long long threshold_int_cycles = 2000, int_cycles = 0;
	long long curr_cycles = 0, prev_cycles = 0, diff_cycles;
	long long max_diff_cycles = 0, min_diff_cycles = 0;//, intr_cycles = 0;
	long long count_max = 0, count_int = 0;
	long long count_x = 0, count_y = 0, count_z = 0;
	long long min_max_cycles = 0, min_min_cycles = 0;
	long i;
#if 0
	for (i = 0 ; i < SPIN_ITERS ; i ++) {
		diff_cycles[i] = 0;
	}
	rdtscll(prev_cycles);

	for (i = 0 ; i < SPIN_ITERS ; i ++) {
		rdtscll(curr_cycles);
		diff_cycles[i] = (curr_cycles - prev_cycles);
		prev_cycles = curr_cycles;
		if (max_diff_cycles < diff_cycles[i]) max_diff_cycles = diff_cycles[i];
		if (diff_cycles[i] > 1000 && diff_cycles[i] < max_diff_cycles) intr_cycles = diff_cycles[i];
	}

	for (i = 0 ; i < SPIN_ITERS ; i ++) {
		//PRINTC("%lld ", diff_cycles[i]);
	}
#endif	
	rdtscll(prev_cycles);
	min_diff_cycles = min_min_cycles = prev_cycles;

	for (i = 0; i < SPIN_ITERS ; i ++) {
		rdtscll(curr_cycles);
		diff_cycles = (curr_cycles - prev_cycles);
		prev_cycles = curr_cycles;

		if (max_diff_cycles < diff_cycles) max_diff_cycles = diff_cycles;
		if (diff_cycles > threshold_int_cycles && min_diff_cycles > diff_cycles) { min_diff_cycles = diff_cycles; count_max ++; }
		if (diff_cycles > threshold_int_cycles && diff_cycles < threshold_cycles) { int_cycles = diff_cycles; count_int ++; }
		if (diff_cycles <= threshold_int_cycles) {
				count_z ++;
				if (diff_cycles > min_max_cycles) { min_max_cycles = diff_cycles; count_x ++; }
				if (diff_cycles < min_min_cycles) { min_min_cycles = diff_cycles; count_y ++; }
		}
		
	//	if (diff_cycles > 1000 && diff_cycles < max_diff_cycles) intr_cycles = diff_cycles;
	}
	PRINTC("int:%lld:%lld max:%lld:%lld min:%lld\n", int_cycles, count_int, max_diff_cycles, count_max, min_diff_cycles);
	PRINTC("%lld:%lld, %lld:%lld %lld\n", min_max_cycles, count_x, min_min_cycles, count_y, count_z);
	while (1);
}

void
test_run(void)
{
//	timer_attach();
//	test_timer();
//	test_timer_perf();
//	timer_detach();
	spin();

	/* 
	 * It is ideal to ubenchmark kernel API with timer interrupt detached,
	 * Not so much for unit-tests
	 */
//	test_tcaps_perf();

//	test_thds();
//	test_thds_perf();

//	test_mem();

//	test_async_endpoints();
//	test_async_endpoints_perf();

//	test_inv();
//	test_inv_perf(0);
//	test_inv_perf(1);

//	test_captbl_expand();
}

