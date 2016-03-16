#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include "vcos_kernel_api.h"

extern void test_run(void *);
extern void term_fn(void *);
int is_vkernel = 1;
int test_status = 0;
extern vaddr_t cos_upcall_entry;
thdcap_t vmthd;
struct cos_compinfo booter_info;
thdcap_t termthd; 		/* switch to this to shutdown */

void
cos_init(void)
{
	printc("cos_init start\n");
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	if (is_vkernel) { 
		printc("\nvirtualization layer init\n");
		is_vkernel = 0;
		compcap_t cc;
		printc("cos_upcall_entry: %x\n", (unsigned int)&cos_upcall_entry);

		cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)&cos_upcall_entry);
		assert(cc);
		vmthd = cos_thd_alloc(&booter_info, cc, test_run, NULL);
		assert(vmthd);

		while (test_status == 0)
			cos_thd_switch(vmthd);

		printc("\n...done. terminating..\n");
		termthd = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
		assert(termthd);

		cos_thd_switch(termthd);
	} 
	printc("cos_init end\n");

	return;
}
