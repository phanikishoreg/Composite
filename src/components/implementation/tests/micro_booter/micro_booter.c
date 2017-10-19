#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t            termthd; /* switch to this to shutdown */
unsigned long       tls_test[TEST_NTHDS];
extern unsigned int cyc_per_usec;

#include <llprint.h>

/* For Div-by-zero test */
int num = 1, den = 0;

void
term_fn(void *d)
{
	SPIN();
}

extern void test_run_perf();

void
cos_init(void)
{
	int cycs;

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	termthd = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
	assert(termthd);

	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%u cycles per microsecond\n", cyc_per_usec);

	PRINTC("\nMicro Booter started.\n");
//	PRINTC("\nUnit-tests start\n");
//	test_run_mb();
//	PRINTC("\nUnit-tests end\n");
	PRINTC("\nPerformance-tests start\n");
	test_run_perf();
	PRINTC("\nPerformance-tests end\n");
	PRINTC("\nMicro Booter done.\n");

	cos_thd_switch(termthd);

	return;
}
