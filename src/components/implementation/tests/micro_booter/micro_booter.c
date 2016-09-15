#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t termthd; 		/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

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

/* For Div-by-zero test */
int num = 1, den = 0;

void
term_fn(void *d)
{ while(1); }
//{ BUG_DIVZERO(); }

void
timer_attach(void)
{
//	cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC, BOOT_CAPTBL_SELF_INITRCV_BASE);
	printc("\t%d cycles per microsecond\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));
}

void
wait_timer_calib(void)
{
	int cycles;

//	cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC, BOOT_CAPTBL_SELF_INITRCV_BASE);
	while ((cycles = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE)) == 0) {
		int i = 0;

		/* I want to do this because I want to avoid many system calls while timer is calibrated.. */
		while (i < 100000) i ++;
	}
	printc("\t%d cycles per microsecond\n", cycles);
}

void
timer_detach(void)
{
	int pending;
	thdid_t  tid;
	int      rcving;
	cycles_t cycles;

//	cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);

	/*
	 * FIXME: This could block if you've already processed all the events..
	 * There is no way to know! Timer can fire any moment.. 
	 */
//	while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles)) != 0) ;
}

void
cos_init(void)
{
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	termthd = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
	assert(termthd);

	wait_timer_calib();

	PRINTC("\nMicro Booter started.\n");
	test_run();
	PRINTC("\nMicro Booter done.\n");

	cos_thd_switch(termthd);

	return;
}
