#include "kernel.h"
#include "chal_cpu.h"
#include "isr.h"

#define APIC_DEFAULT_PHYS 0xfee00000
#define APIC_HDR_LEN_OFF 0x04
#define APIC_CNTRLR_ADDR_OFF 0x24
#define APIC_CNTRLR_FLAGS_OFF 0x28
#define APIC_CNTR_ARR_OFF 0x2C

/* See 5.2.12 in the ACPI 5.0 Spec */
enum
{
	APIC_CNTL_LAPIC  = 0,
	APIC_CNTL_IOAPIC = 1,
};

struct int_cntl_head {
	u8_t type;
	u8_t len;
} __attribute__((packed));

struct lapic_cntl {
	/* type == APIC_CNTL_LAPIC */
	struct int_cntl_head header;
	u8_t                 proc_id;
	u8_t                 apic_id;
	u32_t                flags; /* 0 = dead processor */
} __attribute__((packed));

struct ioapic_cntl {
	/* type == APIC_CNTL_IOAPIC */
	struct int_cntl_head header;
	u8_t                 ioapic_id;
	u8_t                 reserved;
	u32_t                ioapic_phys_addr;
	u32_t                glb_int_num_off; /* I/O APIC's interrupt base number offset  */
} __attribute__((packed));

int ncpus = 1;
int cpus[NUM_CPU];

#define LAPIC_SVI_REG 0x0F0
#define LAPIC_EOI_REG 0x0B0
#define LAPIC_TIMER_LVT_REG 0x320
#define LAPIC_DIV_CONF_REG 0x3e0
#define LAPIC_INIT_COUNT_REG 0x380
#define LAPIC_CURR_COUNT_REG 0x390

#define LAPIC_PERIODIC_MODE (0x01 << 17)
#define LAPIC_ONESHOT_MODE (0x00 << 17)
#define LAPIC_TSCDEADLINE_MODE (0x02 << 17)
#define LAPIC_INT_MASK (1 << 16)

#define LAPIC_TIMER_CALIB_VAL 0xffffffff

#define IA32_MSR_TSC_DEADLINE 0x000006e0

#define LAPIC_TIMER_MIN (1 << 12)
#define LAPIC_COUNTER_MIN (1 << 3)

#define LAPIC_ONESHOT_THRESH (1 << 12)
#define LAPIC_TSCDEADLINE_THRESH 0

extern int timer_process(struct pt_regs *regs);

enum lapic_timer_type
{
	LAPIC_ONESHOT = 0,
	LAPIC_PERIODIC,
	LAPIC_TSC_DEADLINE,
};

enum lapic_timer_div_by_config
{
	LAPIC_DIV_BY_2 = 0,
	LAPIC_DIV_BY_4,
	LAPIC_DIV_BY_8,
	LAPIC_DIV_BY_16,
	LAPIC_DIV_BY_32 = 8,
	LAPIC_DIV_BY_64,
	LAPIC_DIV_BY_128,
	LAPIC_DIV_BY_1,
};

static volatile void *lapic             = (void *)APIC_DEFAULT_PHYS;
static unsigned int   lapic_timer_mode  = LAPIC_TSC_DEADLINE;
static unsigned int   lapic_is_disabled = 1;

static unsigned int lapic_cycs_thresh        = 0;
static u32_t        lapic_cpu_to_timer_ratio = 0;
u32_t               lapic_timer_calib_init   = 0;

static void
lapic_write_reg(u32_t off, u32_t val)
{
	*(u32_t *)(lapic + off) = val;
}

static void
lapic_ack(void)
{
	if (lapic) lapic_write_reg(LAPIC_EOI_REG, 0);
}

static u32_t
lapic_read_reg(u32_t off)
{
	return *(u32_t *)(lapic + off);
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

static inline u32_t
lapic_cycles_to_timer(u32_t cycles)
{
	assert(lapic_cpu_to_timer_ratio);

	/* convert from (relative) CPU cycles to APIC counter */
	cycles = (cycles / lapic_cpu_to_timer_ratio);
	if (cycles == 0) cycles= (LAPIC_TIMER_MIN / lapic_cpu_to_timer_ratio);

	return cycles;
}

static int
lapic_apicid(void)
{
	u32_t a, b, c, d;

	chal_cpuid(1, &a, &b, &c, &d);

	return (int)(b >> 24); /* Vol 2, 3-205: high byte is apicid */
}

void
lapic_intsrc_iter(unsigned char *madt)
{
	struct int_cntl_head *h   = (struct int_cntl_head *)(madt + APIC_CNTR_ARR_OFF);
	u32_t                 len = *(u32_t *)(madt + APIC_HDR_LEN_OFF);
	struct int_cntl_head *end = (struct int_cntl_head *)(madt + len);
	int                   us = lapic_apicid(), off = 1;

	cpus[0] = us;
	printk("\tMADT length %d (base struct %d)\n", len, APIC_CNTR_ARR_OFF);
	assert(h <= end);
	for (; h < end; h = (struct int_cntl_head *)((char *)h + h->len)) {
		/* termination condition */
		assert(h->len >= sizeof(struct int_cntl_head));
		switch (h->type) {
		case APIC_CNTL_LAPIC: {
			struct lapic_cntl *l = (struct lapic_cntl *)h;

			assert(l->header.len == sizeof(struct lapic_cntl));
			printk("\tLAPIC found: coreid %d, apicid %d\n", l->proc_id, l->apic_id);

			if (l->proc_id != us && NUM_CPU > 1) {
				cpus[off++] = l->proc_id;
				ncpus++;
			}

			break;
		}
		case APIC_CNTL_IOAPIC: {
			struct ioapic_cntl *io = (struct ioapic_cntl *)h;

			assert(io->header.len == sizeof(struct ioapic_cntl));
			printk("\tI/O APIC found: ioapicid %d, addr %x, int offset %d\n", io->ioapic_id,
			       io->ioapic_phys_addr, io->glb_int_num_off);
			break;
		}
		default:
			/* See 5.2.12 in the ACPI 5.0 Spec */
			printk("\tInterrupt controller type %d: ignoring\n", h->type);
			break;
		}
	}
	printk("\tAPICs processed, %d cores\n", ncpus);
}

u32_t
lapic_find_localaddr(void *l)
{
	u32_t          i;
	unsigned char  sum       = 0;
	unsigned char *lapicaddr = l;
	u32_t          length    = *(u32_t *)(lapicaddr + APIC_HDR_LEN_OFF);
	u32_t          addr, apic_flags;

	printk("Initializing LAPIC @ %p\n", lapicaddr);

	for (i = 0; i < length; i++) {
		sum += lapicaddr[i];
	}

	if (sum != 0) {
		printk("\tInvalid checksum (%d)\n", sum);
		return 0;
	}

	addr       = *(u32_t *)(lapicaddr + APIC_CNTRLR_ADDR_OFF);
	apic_flags = *(u32_t *)(lapicaddr + APIC_CNTRLR_FLAGS_OFF);
	assert(apic_flags == 1); /* we're assuming the PIC exists */
	lapic_intsrc_iter(lapicaddr);

	printk("\tChecksum is OK\n");
	lapic = (void *)(addr);
	printk("\tlapic: %p\n", lapic);

	return addr;
}

void
lapic_disable_timer(int timer_type)
{
	if (lapic_is_disabled) return;

	if (timer_type == LAPIC_ONESHOT) {
		lapic_write_reg(LAPIC_INIT_COUNT_REG, 0);
	} else if (timer_type == LAPIC_TSC_DEADLINE) {
		writemsr(IA32_MSR_TSC_DEADLINE, 0, 0);
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
	if (deadline < now || (deadline - now) < LAPIC_TIMER_MIN) deadline = now + LAPIC_TIMER_MIN;

	if (timer_type == LAPIC_ONESHOT) {
		u32_t counter;

		counter = lapic_cycles_to_timer((u32_t)(deadline - now));
		if (counter == 0) counter= LAPIC_COUNTER_MIN;

		lapic_write_reg(LAPIC_INIT_COUNT_REG, counter);
	} else if (timer_type == LAPIC_TSC_DEADLINE) {
		writemsr(IA32_MSR_TSC_DEADLINE, (u32_t)((deadline << 32) >> 32), (u32_t)(deadline >> 32));
	} else {
		printk("Mode (%d) not supported\n", timer_type);
		assert(0);
	}

	lapic_is_disabled = 0;
}

void
lapic_set_page(u32_t page)
{
	lapic = (void *)(page * (1 << 22) | ((u32_t)lapic & ((1 << 22) - 1)));

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

u32_t
lapic_get_ccr(void)
{
	return lapic_read_reg(LAPIC_CURR_COUNT_REG);
}

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
{
	lapic_set_timer(lapic_timer_mode, cycles);
}

void
chal_timer_disable(void)
{
	lapic_disable_timer(lapic_timer_mode);
}

unsigned int
chal_cyc_thresh(void)
{
	return lapic_cycs_thresh;
}

void
lapic_timer_init(void)
{
	u32_t low, high;
	u32_t a, b, c, d;

	chal_cpuid(1, &a, &b, &c, &d);
	if (c & (1 << 21)) printk("LAPIC:  processor supports x2APIC, IGNORED.\n");

	if (!lapic_tscdeadline_supported()) {
		printk("LAPIC: TSC-Deadline Mode not supported! Configuring Oneshot Mode!\n");

		/* Set the mode and vector */
		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_ONESHOT_MODE);
		lapic_timer_mode = LAPIC_ONESHOT;

		/* Set the timer and mask it, so timer interrupt is not fired - for timer calibration through HPET */
		lapic_write_reg(LAPIC_INIT_COUNT_REG, LAPIC_TIMER_CALIB_VAL);
		lapic_write_reg(LAPIC_TIMER_LVT_REG, lapic_read_reg(LAPIC_TIMER_LVT_REG) | LAPIC_INT_MASK);
		lapic_timer_calib_init = 1;
		lapic_cycs_thresh      = LAPIC_ONESHOT_THRESH;
	} else {
		printk("LAPIC: Configuring TSC-Deadline Mode!\n");

		/* Set the mode and vector */
		lapic_write_reg(LAPIC_TIMER_LVT_REG, HW_LAPIC_TIMER | LAPIC_TSCDEADLINE_MODE);
		lapic_timer_mode  = LAPIC_TSC_DEADLINE;
		lapic_cycs_thresh = LAPIC_TSCDEADLINE_THRESH;
	}

	/* Set the divisor */
	lapic_write_reg(LAPIC_DIV_CONF_REG, LAPIC_DIV_BY_1);
}

void
smp_init(void)
{
	int apicid = lapic_apicid();
}
