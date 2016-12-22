#include "io.h"
#include "kernel.h"
#include "chal_cpu.h"
#include "isr.h"

#define APIC_DEFAULT_PHYS      0xfee00000
#define APIC_HDR_LEN_OFF       0x04
#define APIC_CNTRLR_ADDR_OFF   0x24

#define LAPIC_SVI_REG          0x0F0
#define LAPIC_EOI_REG          0x0B0
#define LAPIC_TIMER_LVT_REG    0x320
#define LAPIC_DIV_CONF_REG     0x3e0
#define LAPIC_INIT_COUNT_REG   0x380
#define LAPIC_CURR_COUNT_REG   0x390

#define LAPIC_ONESHOT_MODE     (0x00 << 17)
#define LAPIC_PERIODIC_MODE    (0x01 << 17)
#define LAPIC_TSCDEADLINE_MODE (0x02 << 17)
#define LAPIC_INT_MASK         (1<<16)

#define LAPIC_TIMER_CALIB_VAL  0xffffffff

#define IA32_MSR_TSC_DEADLINE  0x000006e0

#define LAPIC_TIMER_MIN        (1<<12)
#define LAPIC_COUNTER_MIN      (1<<3)

#define LAPIC_RETRY_MAX        9

extern int timer_process(struct pt_regs *regs);

enum lapic_timer_type {
	LAPIC_ONESHOT = 0,
	LAPIC_PERIODIC,
	LAPIC_TSC_DEADLINE,
};

enum lapic_timer_div_by_config {
	LAPIC_DIV_BY_2    = 0,
	LAPIC_DIV_BY_4    = 1,
	LAPIC_DIV_BY_8    = 2,
	LAPIC_DIV_BY_16   = 3,
	LAPIC_DIV_BY_32   = 8,
	LAPIC_DIV_BY_64   = 9,
	LAPIC_DIV_BY_128  = 10,
	LAPIC_DIV_BY_1    = 11,
};

static volatile void *lapic = (void *)APIC_DEFAULT_PHYS;
static unsigned int lapic_timer_mode = LAPIC_TSC_DEADLINE;
static unsigned int lapic_is_disabled = 1;

static cycles_t timer_write = 0, timer_fired = 0, timer_req = 0;

static u32_t lapic_cpu_to_timer_ratio = 0;
u32_t lapic_timer_calib_init = 0;

static void
lapic_write_reg(u32_t off, u32_t val)
{ *(u32_t *)(lapic + off) = val; }

static void
lapic_ack(void)
{ if (lapic) lapic_write_reg(LAPIC_EOI_REG, 0); }

static u32_t
lapic_read_reg(u32_t off)
{ return *(u32_t *)(lapic + off); }

static int
lapic_tscdeadline_supported(void)
{
	u32_t a, b, c, d;

	chal_cpuid(6, &a, &b, &c, &d);
	if (a & (1 << 1)) printk("LAPIC Timer runs at Constant Rate!!\n");	

	chal_cpuid(1, &a, &b, &c, &d);
	if (c & (1 << 24)) return 1;

	return 0;
}

static inline u32_t
lapic_cycles_to_timer(u32_t cycles)
{
	assert(lapic_cpu_to_timer_ratio);

	/* convert from (relative) CPU cycles to APIC counter */
	cycles = (cycles / lapic_cpu_to_timer_ratio);
	if (cycles == 0) cycles = (LAPIC_TIMER_MIN / lapic_cpu_to_timer_ratio);

	return cycles;
}

u32_t
lapic_find_localaddr(void *l)
{
	u32_t i;
	unsigned char sum = 0;
	unsigned char *lapicaddr = l;
	u32_t length = *(u32_t *)(lapicaddr + APIC_HDR_LEN_OFF);

	printk("Initializing LAPIC @ %p\n", lapicaddr);

	for (i = 0 ; i < length ; i ++) {
		sum += lapicaddr[i];
	}

	if (sum == 0) {
		u32_t addr = *(u32_t *)(lapicaddr + APIC_CNTRLR_ADDR_OFF);

		printk("\tChecksum is OK\n");
		lapic = (void *)(addr);
		printk("\tlapic: %p\n", lapic); 

		return addr;
	}

	printk("\tInvalid checksum (%d)\n", sum);
	return 0;
}

void
lapic_disable_timer(int timer_type)
{
	if (lapic_is_disabled) return;

//	printk("TIMER-DISABLE\n");
	if (timer_type == LAPIC_ONESHOT) {
		lapic_write_reg(LAPIC_INIT_COUNT_REG, 0);
	} else if (timer_type == LAPIC_TSC_DEADLINE) {
		u32_t low = 0, high = 0;
		writemsr(IA32_MSR_TSC_DEADLINE, low, high);

//		readmsr(IA32_MSR_TSC_DEADLINE, &low, &high);
//		if (low || high) printk("not disabled???\n");
	} else {
		printk("Mode (%d) not supported\n", timer_type);
		assert(0);
	}

	lapic_is_disabled = 1;
}

void
lapic_set_timer(int timer_type, cycles_t deadline)
{
	u64_t now;

	rdtscll(now);
	if (deadline < now || (deadline - now) < LAPIC_TIMER_MIN) {
//		printk("+");
		deadline = now + LAPIC_TIMER_MIN;
	}
//	if (deadline < LAPIC_TIMER_MIN) deadline = LAPIC_TIMER_MIN;
//	deadline += now;
//	timer_write = now;
//	timer_req = deadline;

	if (timer_type == LAPIC_ONESHOT) {
		u32_t counter;

		counter = lapic_cycles_to_timer((u32_t)(deadline - now));
		if (counter == 0) counter = LAPIC_COUNTER_MIN; 

	//	printk("ONESHOT\n");
		lapic_write_reg(LAPIC_INIT_COUNT_REG, counter);

	//	printk("%lu:%lu:%lu\n", counter, lapic_read_reg(LAPIC_INIT_COUNT_REG), lapic_read_reg(LAPIC_CURR_COUNT_REG));
	} else if (timer_type == LAPIC_TSC_DEADLINE) {
//		u32_t high = 0, low = 0;
		u32_t iters = 0;
//			printk("TSC-DEADLINE = ");
//		do {
			writemsr(IA32_MSR_TSC_DEADLINE, (u32_t) ((deadline << 32) >> 32), (u32_t)(deadline >> 32));

//			readmsr(IA32_MSR_TSC_DEADLINE, &low, &high);
//			if (!low && !high)
//				printk("%llu:%llu\n", deadline, ((u64_t)high << 32) | ((u64_t)low));
//			iters ++;
//			assert(iters < LAPIC_RETRY_MAX);
//		} while (!low && !high);
//	cos_mem_fence();
//		printk("+++now:%llu, deadline:%llu, diff:%llu, %llu\n", now, deadline, deadline - now, ((u64_t)high << 32) | ((u64_t)low));
	} else {
		printk("Mode (%d) not supported\n", timer_type);
		assert(0);
	}

	lapic_is_disabled = 0;
}

void
lapic_set_page(u32_t page)
{
	lapic = (void *)(page * (1 << 22) | ((u32_t)lapic & ((1<<22)-1)));

	printk("\tSet LAPIC @ %p\n", lapic);
}

int
lapic_timer_handler(struct pt_regs *regs)
{
//	static int counter = 0;
	int preempt = 1; 
	cycles_t now;

//	rdtscll(timer_fired);
//	rdtscll(now);
	lapic_ack();
//	printk("now:%llu...", now);
//	printk("-----------------------\n%u=>Timer set@%llu, Timer request:%llu:%llu, Timer fired:%llu:%llu\n---------------------\n", counter, timer_write, timer_req, (timer_req - timer_write), timer_fired, (timer_fired - timer_write));

	preempt = timer_process(regs);
//	counter ++;
	return preempt;
}

u32_t
lapic_get_ccr(void)
{ return lapic_read_reg(LAPIC_CURR_COUNT_REG); }

void
lapic_timer_calibration(u32_t ratio)
{
	assert(ratio && lapic_timer_calib_init);

	lapic_timer_calib_init   = 0;
	lapic_cpu_to_timer_ratio = ratio;

	/* reset INIT counter, and unmask timer */
	lapic_write_reg(LAPIC_INIT_COUNT_REG, 0);
	lapic_write_reg(LAPIC_TIMER_LVT_REG, lapic_read_reg(LAPIC_TIMER_LVT_REG) & ~LAPIC_INT_MASK);
	lapic_is_disabled = 1;

	printk("LAPIC: Timer calibrated - CPU Cycles to APIC Timer Ratio is %u\n", lapic_cpu_to_timer_ratio); 
}

void
chal_timer_set(cycles_t cycles)
{ lapic_set_timer(lapic_timer_mode, cycles); }

void
chal_timer_disable(void)
{ lapic_disable_timer(lapic_timer_mode); }

void
lapic_timer_init(void)
{
	u32_t low, high;

//	if (!lapic_tscdeadline_supported()) {
//		printk("LAPIC: TSC-Deadline Mode not supported! Configuring Oneshot Mode!\n");
//
//		/* Set the mode and vector */
//		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_ONESHOT_MODE);
//		lapic_timer_mode = LAPIC_ONESHOT;
//
//		/* Set the timer and mask it, so timer interrupt is not fired - for timer calibration through HPET */
//		lapic_write_reg(LAPIC_INIT_COUNT_REG, LAPIC_TIMER_CALIB_VAL);
//		lapic_write_reg(LAPIC_TIMER_LVT_REG, lapic_read_reg(LAPIC_TIMER_LVT_REG) | LAPIC_INT_MASK);
//		lapic_timer_calib_init = 1;
//	} else {
		printk("LAPIC: Configuring TSC-Deadline Mode!\n");
	
		/* Set the mode and vector */
		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_TSCDEADLINE_MODE);
		lapic_timer_mode = LAPIC_TSC_DEADLINE;
//	}

	/* Set the divisor */
	lapic_write_reg(LAPIC_DIV_CONF_REG, LAPIC_DIV_BY_1);
}
