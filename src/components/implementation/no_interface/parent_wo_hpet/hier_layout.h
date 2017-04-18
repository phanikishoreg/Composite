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
};

#define N_PARENT_THDS0 3 /* includes hpet in parent */
#define N_PARENT_THDS (N_PARENT_THDS0 - 1) /* other than child sched thd.*/
#define N_CHILD_THDS 2

#define HPET_IN_CHILD 0
#define HPET_IN_PARENT (N_PARENT_THDS0 - 1)

#define MS_TO_US(m) (m*1000)

#undef HPET_ALIGNED
#ifdef HPET_ALIGNED
#define CHILD_PERIOD_T 20000 //usecs
#define CHILD_WCET_C   8990 //usecs
#define CHILD_PRIO     2

static microsec_t parent_thds_T[N_PARENT_THDS0] = { 10, 30};
static microsec_t parent_thds_C[N_PARENT_THDS0] = { 1, 9};
static microsec_t parent_thds_W[N_PARENT_THDS0] = { 1000, 9000};
static microsec_t parent_thds_Prio[N_PARENT_THDS0] = { 1, 3};

static microsec_t child_thds_T[N_CHILD_THDS] = { HPET_PERIOD_USEC/1000, 60};
static microsec_t child_thds_C[N_CHILD_THDS] = { 6, 12};
static microsec_t child_thds_W[N_CHILD_THDS] = { 5990, 12000};

#else

#define CHILD_PERIOD_T 20000 //usecs
#define CHILD_WCET_C   9990 //usecs
#define CHILD_PRIO     2

static microsec_t parent_thds_T[N_PARENT_THDS0] = { 18, 36};
static microsec_t parent_thds_C[N_PARENT_THDS0] = { 2, 12};
static microsec_t parent_thds_W[N_PARENT_THDS0] = { 2000, 12000};
static microsec_t parent_thds_Prio[N_PARENT_THDS0] = { 1, 3};

static microsec_t child_thds_T[N_CHILD_THDS] = { HPET_PERIOD_USEC/1000, 60};
static microsec_t child_thds_C[N_CHILD_THDS] = { 4, 18};
static microsec_t child_thds_W[N_CHILD_THDS] = { 4000, 18000};
#endif

#endif /* HIER_LAYOUT_H */
