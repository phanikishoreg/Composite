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
thdcap_t vmthd0;
struct cos_compinfo vkern_info;
struct cos_compinfo vmbooter_info;
thdcap_t termthd; 		/* switch to this to shutdown */
captblcap_t vmct;
/* Unused for now!
compcap_t vmcc;
pgtblcap_t vmpt;
arcvcap_t vmrcv;
tcap_t vmtcp;
sinvcap_t invthd;
sinvcap_t invinv;
sinvcap_t invrcv;
sinvcap_t invsnd;
sinvcap_t invcomp;
sinvcap_t invcaptbl;
sinvcap_t invpgtbl;
*/

int
vkern_api_handler(int a, int b, int c)
{
	
	return 0;
}

void
cos_init(void)
{
	printc("cos_init start\n");
	cos_meminfo_init(&vkern_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&vkern_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &vkern_info);

	if (is_vkernel) { 
		printc("\nvirtualization layer init\n");
		is_vkernel = 0;
		termthd = cos_thd_alloc(&vkern_info, vkern_info.comp_cap, term_fn, NULL);
		printc("cos_upcall_entry: %x\n", (unsigned int)&cos_upcall_entry);

		
		vmct = cos_captbl_alloc(&vkern_info);
		assert(vmct);

		cos_compinfo_init(&vmbooter_info, BOOT_CAPTBL_SELF_PT, vmct, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &vkern_info);
		

		//cos_compinfo_alloc(&vmbooter_info, (vaddr_t)cos_get_heap_ptr(), &cos_upcall_entry, &vkern_info);
		//cos_cap_init(&vkern_info, BOOT_CAPTBL_SELF_PT, &vmbooter_info);
		
		cos_cap_init(&vkern_info, BOOT_CAPTBL_SELF_INITTHD_BASE, &vmbooter_info);
		cos_cap_init(&vkern_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, &vmbooter_info);
		cos_cap_init(&vkern_info, BOOT_CAPTBL_SELF_INITRCV_BASE, &vmbooter_info);
		cos_cap_init(&vkern_info, BOOT_CAPTBL_SELF_INITHW_BASE, &vmbooter_info);

		vmthd0 = cos_thd_alloc(&vkern_info, vmbooter_info.comp_cap, test_run, NULL);
		vmthd = cos_cap_copy(&vkern_info, vmthd0, CAP_THD, &vmbooter_info);
		assert(vmthd);

		while (test_status == 0)
			cos_thd_switch(vmthd0);

		printc("\n...done. terminating..\n");
		assert(termthd);

		cos_thd_switch(termthd);
	} 
	printc("cos_init end\n");

	return;
}
