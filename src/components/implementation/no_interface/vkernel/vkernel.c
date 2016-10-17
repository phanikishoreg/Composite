#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "vk_types.h"

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN() do { while (1) ; } while (0)

#if 0
struct vms_info {
	struct cos_compinfo cinfo;
	thdcap_t initthd, exitthd;
	thdid_t inittid;
	tcap_t inittcap;
	arcvcap_t initrcv;
};

struct vkernel_info {
	struct cos_compinfo cinfo;

	thdcap_t termthd;
	asndcap_t vminitasnd[VM_COUNT];
};
#endif

extern vaddr_t cos_upcall_entry;
extern int test_running;
extern void vm_init(void *);

extern void vk_test_fn(void *);
extern void vm_test_fn(void *);

struct vms_info vmx_info[VM_COUNT];
struct vkernel_info vk_info;
unsigned int ready_vms = VM_COUNT;
struct cos_compinfo *vk_cinfo = (struct cos_compinfo *)&vk_info.cinfo;

void
vk_terminate(void *d)
{ SPIN(); }

void
vm_exit(void *d)
{
	printc("%d: EXIT\n", (int)d);
	ready_vms --;
	vmx_info[(int)d].initthd = 0;	

	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
scheduler(void) 
{
	static unsigned int i;
	thdid_t             tid;
	int                 blocked;
	cycles_t            cycles;
	int                 index;

	while (ready_vms) {
		index = i++ % VM_COUNT;
		
		if (vmx_info[index].initthd) {
			assert(vk_info.vminitasnd[index]);

			if (cos_tcap_delegate(vk_info.vminitasnd[index], BOOT_CAPTBL_SELF_INITTCAP_BASE,
					      VM_BUDGET_FIXED, VM_PRIO_FIXED, TCAP_DELEG_YIELD)) assert(0);
		}

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &blocked, &cycles)) ;
	}
}


void
cos_init(void)
{
	int id, cycs;
	int ret;

	printc("vkernel: START\n");
	assert(VM_COUNT >= 2);

	cos_meminfo_init(&vk_cinfo->mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(vk_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, vk_cinfo);

	vk_info.termthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vk_terminate, NULL);
	assert(vk_info.termthd);

	while (!(cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE))) ;
	printc("\t%d cycles per microsecond\n", cycs);
	printc("vkernel: Creating test capabilites in vkernel\n");
	
	vk_info.testthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vk_test_fn, NULL);
	assert(vk_info.testthd);

	vk_info.testtcap = cos_tcap_alloc(vk_cinfo, TCAP_PRIO_MAX);
	assert(vk_info.testtcap);

	vk_info.testarcv = cos_arcv_alloc(vk_cinfo, vk_info.testthd, vk_info.testtcap, vk_cinfo->comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(vk_info.testarcv);

	ret = cos_tcap_transfer(vk_info.testarcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
	assert(ret == 0);

	for (id = 0 ; id < VM_COUNT ; id ++) {
		struct cos_compinfo *vm_cinfo = &vmx_info[id].cinfo;
		struct vms_info *vm_info = &vmx_info[id];
		vaddr_t vm_range, addr;
		pgtblcap_t vmpt, vmutpt;
		captblcap_t vmct;
		compcap_t vmcc;
		int ret;

		printc("vkernel: VM%d Init START\n", id);
		printc("\tForking VM\n");
		vm_info->exitthd = cos_thd_alloc(vk_cinfo, vk_cinfo->comp_cap, vm_exit, (void *)id);
		assert(vm_info->exitthd);
		
		vmct = cos_captbl_alloc(vk_cinfo);
		assert(vmct);

		vmpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmpt);

		vmutpt = cos_pgtbl_alloc(vk_cinfo);
		assert(vmutpt);

		vmcc = cos_comp_alloc(vk_cinfo, vmct, vmpt, (vaddr_t)&cos_upcall_entry);
		assert(vmcc);

		cos_meminfo_init(&vm_cinfo->mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, vmutpt);
		cos_compinfo_init(vm_cinfo, vmpt, vmct, vmcc,
				(vaddr_t)BOOT_MEM_VM_BASE, VM_CAPTBL_FREE, vk_cinfo);

		vm_info->initthd = cos_thd_alloc(vk_cinfo, vm_cinfo->comp_cap, vm_init, (void *)id);
		assert(vm_info->initthd);
		vm_info->inittid = (thdid_t)cos_introspect(vk_cinfo, vm_info->initthd, THD_GET_TID);
		printc("\tInit thread= cap:%x tid:%x\n", (unsigned int)vm_info->initthd, (unsigned int)vm_info->inittid);

		printc("\tCopying pgtbl, captbl, component capabilities\n");
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vmct);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vmpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vmutpt);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vmcc);
		assert(ret == 0);

		printc("\tCreating and copying required initial capabilities\n");
		/*
		 * TODO: Multi-core support to create INITIAL Capabilities per core
		 */
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITTHD_BASE, vk_cinfo, vm_info->initthd);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE, vk_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_EXITTHD_BASE, vk_cinfo, vm_info->exitthd);
		assert(ret == 0);
		
		vm_info->inittcap = cos_tcap_alloc(vk_cinfo, TCAP_PRIO_MAX);
		assert(vm_info->inittcap);

		vm_info->initrcv = cos_arcv_alloc(vk_cinfo, vm_info->initthd, vm_info->inittcap, vk_cinfo->comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(vm_info->initrcv);

		ret = cos_tcap_transfer(vm_info->initrcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE, vk_cinfo, vm_info->inittcap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(vm_cinfo, BOOT_CAPTBL_SELF_INITRCV_BASE, vk_cinfo, vm_info->initrcv);
		assert(ret == 0);

		/*
		 * Create send end-point in VKernel to each VM's INITRCV end-point
		 */
		vk_info.vminitasnd[id] = cos_asnd_alloc(vk_cinfo, vm_info->initrcv, vk_cinfo->captbl_cap);
		assert(vk_info.vminitasnd[id]);

		printc("\tCreating test capabilities in vm\n");
		vm_info->testthd = cos_thd_alloc(vk_cinfo, vm_cinfo->comp_cap, vm_test_fn, (void *)id);
		assert(vm_info->testthd);

		vm_info->testtcap = cos_tcap_alloc(vk_cinfo, TCAP_PRIO_MAX);
		assert(vm_info->testtcap);

		vm_info->testarcv = cos_arcv_alloc(vk_cinfo, vm_info->testthd, vm_info->testtcap, vk_cinfo->comp_cap, vk_info.testarcv);
		assert(vm_info->testarcv);

		ret = cos_tcap_transfer(vm_info->testarcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);
		assert(ret == 0);

		vm_info->testasnd = cos_asnd_alloc(vk_cinfo, vk_info.testarcv, vk_cinfo->captbl_cap);
		assert(vm_info->testasnd);

		vk_info.testasnd[id] = cos_asnd_alloc(vk_cinfo, vm_info->testarcv, vk_cinfo->captbl_cap);
		assert(vk_info.testasnd[id]);

		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_TESTVKTHD_BASE, vk_cinfo, vk_info.testthd);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_TESTTHD_BASE, vk_cinfo, vm_info->testthd);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_TESTTCAP_BASE, vk_cinfo, vm_info->testtcap);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_TESTRCV_BASE, vk_cinfo, vm_info->testarcv);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_TESTASND_BASE, vk_cinfo, vm_info->testasnd);
		assert(ret == 0);

		/*
		 * Create and copy booter comp virtual memory to each VM
		 */
		vm_range = (vaddr_t)cos_get_heap_ptr() - BOOT_MEM_VM_BASE;
		assert(vm_range > 0);
		printc("\tMapping in Booter component's virtual memory (range:%lu)\n", vm_range);
		for (addr = 0 ; addr < vm_range ; addr += PAGE_SIZE) {
			vaddr_t src_pg = (vaddr_t)cos_page_bump_alloc(vk_cinfo), dst_pg;
			assert(src_pg);
			
			memcpy((void *)src_pg, (void *)(BOOT_MEM_VM_BASE + addr), PAGE_SIZE);
			
			dst_pg = cos_mem_alias(vm_cinfo, vk_cinfo, src_pg);
			assert(dst_pg);
		}

		printc("\tAllocating Untyped memory (size: %lu)\n", (unsigned long)VM_UNTYPED_SIZE);
		cos_meminfo_alloc(vm_cinfo, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE);

		printc("vkernel: VM%d Init END\n", id);
	}

	printc("------------------[ VKernel & VMs init complete ]------------------\n");

	printc("Inter-pgtbl tests (Vkernel <-> DOM0)\n");
	while (test_running) cos_switch(vk_info.testthd, vk_info.testtcap, TCAP_PRIO_MAX, TCAP_TIME_NIL, 0);
	printc("Done.\n");

	//printc("Starting Scheduler\n");
	//scheduler();

	printc("vkernel: END\n");
	cos_thd_switch(vk_info.termthd);

	printc("vkernel: back in initial thread after switching to terminal thread. ERROR.\n");

	return;
}
