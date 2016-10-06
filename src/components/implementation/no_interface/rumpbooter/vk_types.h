#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>

#define COS_VIRT_MACH_COUNT 2
#define COS_VIRT_MACH_MEM_SZ (1<<26) //64MB

#define COS_SHM_VM_SZ (1<<20) //4MB
#define COS_SHM_ALL_SZ (((COS_VIRT_MACH_COUNT - 1) > 0 ? (COS_VIRT_MACH_COUNT - 1) : 1) * COS_SHM_VM_SZ) //shared regions with VM 0

#define VM_TIMESLICE (3400*1000*10) //on 3.4GHz machine, 10ms
#define VM_MIN_TIMESLICE (3400)

#define __SIMPLE_XEN_LIKE_TCAPS__
#undef __INTELLIGENT_TCAPS__

enum vm_prio {
	PRIO_BOOST = TCAP_PRIO_MAX,
	PRIO_OVER  = TCAP_PRIO_MAX + 100,
	PRIO_UNDER = TCAP_PRIO_MAX + 50,
};

enum vm_status {
	VM_RUNNING,
	VM_BLOCKED,
	VM_EXITED,
};

enum vm_credits {
#ifdef __INTELLIGENT_TCAPS__
	DOM0_CREDITS = 5,
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
	DOM0_CREDITS = 0, // 0 to not have credit based execution.. 
#endif
	VM1_CREDITS = 1,
	VM2_CREDITS = 4,
};

enum {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
#ifdef __INTELLIGENT_TCAPS__
	VM_CAPTBL_SELF_TIMEASND_BASE   = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_TIMEASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOTCAP_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTCAP_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
#endif
	VM_CAPTBL_SELF_IOASND_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_LAST_CAP             = round_up_to_pow2(VM_CAPTBL_SELF_IOASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum {
	VM0_CAPTBL_SELF_IOTHD_SET_BASE      = VM_CAPTBL_SELF_IOTHD_BASE, 
#ifdef __INTELLIGENT_TCAPS__
	VM0_CAPTBL_SELF_IOTCAP_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTCAP_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
#elif defined __SIMPLE_XEN_LIKE_TCAPS__
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
#endif
	VM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(VM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_FREE                     = round_up_to_pow2(VM0_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

#endif /* VK_TYPES_H */
