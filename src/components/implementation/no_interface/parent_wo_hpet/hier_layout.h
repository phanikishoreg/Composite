#ifndef HIER_LAYOUT_H
#define HIER_LAYOUT_H

enum child_hier_op {
	SCHED_STARTTIME,
	HPET_SNDCAP,
	HPET_RCVCAP,
	TASK_STARTTIME,
	CHILD_BOOTUP_DONE,
};

#define SHARED_MEM_START 0x80000000
#define SHARED_MEM_SIZE  (1<<22)

#define HPET_PERIOD_USEC (40*1000)

enum child_hier_layout {

	CHILD_HIER_SINV = BOOT_CAPTBL_FREE,
	CHILD_HIER_FREE = round_up_to_pow2(CHILD_HIER_SINV + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
//	CHILD_HPET_THD = BOOT_CAPTBL_FREE,
//	CHILD_HPET_TCAP = round_up_to_pow2(CHILD_HPET_THD + CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
//	CHILD_HPET_RCV = round_up_to_pow2(CHILD_HPET_TCAP + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
//	CHILD_HPET_FREE = round_up_to_pow2(CHILD_HPET_RCV + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
};

#endif /* HIER_LAYOUT_H */
