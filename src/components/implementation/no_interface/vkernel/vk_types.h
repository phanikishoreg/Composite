#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define VM_COUNT        2	/* virtual machine count */
#define VM_UNTYPED_SIZE (1<<26) /* untyped memory per vm = 64MB */

#define VM_BUDGET_FIXED 400000
#define VM_PRIO_FIXED   TCAP_PRIO_MAX

struct vms_info {
	struct cos_compinfo cinfo;
	thdcap_t initthd, exitthd;
	thdid_t inittid;
	tcap_t inittcap;
	arcvcap_t initrcv;

	thdcap_t testthd;
	tcap_t testtcap;
	arcvcap_t testarcv;
	asndcap_t testasnd;
};

struct vkernel_info {
	struct cos_compinfo cinfo;

	thdcap_t termthd;
	asndcap_t vminitasnd[VM_COUNT];

	thdcap_t testthd;
	tcap_t testtcap;
	arcvcap_t testarcv;
	asndcap_t testasnd[VM_COUNT]; /* just vkernel and dom0? */
};

enum vm_captbl_layout {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
	VM_CAPTBL_LAST_CAP             = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + NUM_CPU_COS*CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_TESTVKTHD_BASE       = VM_CAPTBL_LAST_CAP,
	VM_CAPTBL_TESTTHD_BASE         = VM_CAPTBL_TESTVKTHD_BASE + CAP16B_IDSZ,
	VM_CAPTBL_TESTTCAP_BASE        = VM_CAPTBL_TESTTHD_BASE + CAP16B_IDSZ,
	VM_CAPTBL_TESTRCV_BASE         = round_up_to_pow2(VM_CAPTBL_TESTTCAP_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_TESTASND_BASE        = VM_CAPTBL_TESTRCV_BASE + CAP64B_IDSZ,
	VM_CAPTBL_TEST_LAST_CAP        = VM_CAPTBL_TESTASND_BASE + CAP64B_IDSZ,
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_TEST_LAST_CAP, CAPMAX_ENTRY_SZ),
};

#endif /* VK_TYPES_H */
