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

#define LAPIC_PERIODIC_MODE    (0x01 << 17)
#define LAPIC_ONESHOT_MODE     (0x00 << 17)
#define LAPIC_TSCDEADLINE_MODE (0x02 << 17)
#define LAPIC_MASK (1<<15)

#define IA32_MSR_TSC_DEADLINE	0x000006e0

#define RETRY_ITERS   3

extern int timer_process(struct pt_regs *regs);

enum lapic_timer_type {
	ONESHOT = 0,
	PERIODIC,
	TSC_DEADLINE,
};

enum lapic_timer_div_by_config {
	DIV_BY_2 = 0,
	DIV_BY_4,
	DIV_BY_8,
	DIV_BY_16,
	DIV_BY_32,
	DIV_BY_64,
	DIV_BY_128,
	DIV_BY_1,
};

static volatile void *lapic = (void *)APIC_DEFAULT_PHYS;
static unsigned int lapic_timer_mode = TSC_DEADLINE;

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

static void
lapic_write_reg(u32_t off, u32_t val)
{ *(u32_t *)(lapic + off) = val; }

static void
lapic_ack(void)
{ if (lapic) lapic_write_reg(LAPIC_EOI_REG, 0); }

static u32_t
lapic_read_reg(u32_t off)
{ return *(u32_t *)(lapic + off); }

void
lapic_set_timer(int timer_type, cycles_t deadline)
{
	u64_t now;

	assert (timer_type != PERIODIC);

	rdtscll(now);
//	if (deadline <= now) return;

	if (timer_type == ONESHOT) {

		lapic_write_reg(LAPIC_INIT_COUNT_REG, (u32_t)(deadline - now));

	} else {

		u32_t high, low;
		int retries = RETRY_ITERS;

retry:	
		writemsr(IA32_MSR_TSC_DEADLINE, (u32_t) ((deadline << 32) >> 32), (u32_t)(deadline >> 32));
		readmsr(IA32_MSR_TSC_DEADLINE, &low, &high);
		if (!low && !high) {
			retries --;
			if (retries > 0) {
				goto retry;
			}
			else {
				printk("Something wrong.. Cannot program TSC-DEADLINE\n");
				assert(0);
			}
		}
	}
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
	int preempt = 1; 

	lapic_ack();

	preempt = timer_process(regs);

	return preempt;
}

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

void
chal_timer_set(cycles_t cycles)
{ lapic_set_timer(lapic_timer_mode, cycles); }

void
lapic_timer_init(void)
{
	u32_t low, high;

	if (!lapic_tscdeadline_supported()) {
		printk("LAPIC: TSC-Deadline Mode not supported! Configuring Oneshot Mode!\n");

		/* Set the mode and vector */
		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_ONESHOT_MODE);
		lapic_timer_mode = ONESHOT;
	} else {
		printk("LAPIC: Configuring TSC-Deadline Mode\n");
	
		/* Set the mode and vector */
		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_TSCDEADLINE_MODE);
		lapic_timer_mode = TSC_DEADLINE;
	}

	/* Set the divisor */
	lapic_write_reg(LAPIC_DIV_CONF_REG, DIV_BY_1);
}
