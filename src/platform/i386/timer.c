#include <thd.h>
#include <inv.h>
#include <hw.h>

#include "isr.h"
#include "io.h"
#include "kernel.h"
#include "chal/cpuid.h"

/*
 * These addressess are specified as offsets from the base HPET
 * pointer, which is a 1024-byte region of memory-mapped
 * registers. The reason we use offsets rather than a struct or
 * bitfields is that ALL accesses, both read and write, must be
 * aligned at 32- or 64-bit boundaries and must read or write an
 * entire 32- or 64-bit value at a time. Packed structs cause GCC to
 * produce code which attempts to operate on the single byte level,
 * which fails.
 */

#define HPET_OFFSET(n) ((unsigned char*)hpet + n)

#define HPET_CAPABILITIES  (0x0)
#define HPET_CONFIGURATION (0x10)
#define HPET_INTERRUPT     (0x20)
#define HPET_COUNTER       (*(u64_t*)(HPET_OFFSET(0xf0)))

#define HPET_T0_CONFIG    (0x100)
#define HPET_Tn_CONFIG(n) HPET_OFFSET(HPET_T0_CONFIG + (0x20 * n))

#define HPET_T0_COMPARATOR    (0x108)
#define HPET_Tn_COMPARATOR(n) HPET_OFFSET(HPET_T0_COMPARATOR + (0x20 * n))

#define HPET_T0_INTERRUPT    (0x110)
#define HPET_Tn_INTERRUPT(n) HPET_OFFSET(HPET_T0_INTERRUPT + (0x20 * n))

#define HPET_ENABLE_CNF (1ll)
#define HPET_LEG_RT_CNF (1ll<<1)

#define HPET_TAB_LENGTH  (0x4)
#define HPET_TAB_ADDRESS (0x2c)

/* Bits in HPET_Tn_CONFIG */
/* 1 << 0 is reserved */
#define TN_INT_TYPE_CNF	(1ll << 1)	/* 0 = edge trigger, 1 = level trigger */
#define TN_INT_ENB_CNF	(1ll << 2)	/* 0 = no interrupt, 1 = interrupt */
#define TN_TYPE_CNF	(1ll << 3)	/* 0 = one-shot, 1 = periodic */
#define TN_PER_INT_CAP	(1ll << 4)	/* read only, 1 = periodic supported */
#define TN_SIZE_CAP	(1ll << 5)	/* 0 = 32-bit, 1 = 64-bit */
#define TN_VAL_SET_CNF	(1ll << 6)	/* set to allow directly setting accumulator */
/* 1 << 7 is reserved */
#define TN_32MODE_CNF	(1ll << 8)	/* 1 = force 32-bit access to 64-bit timer */
/* #define TN_INT_ROUTE_CNF (1<<9:1<<13)*/	/* routing for interrupt */
#define TN_FSB_EN_CNF	(1ll << 14)	/* 1 = deliver interrupts via FSB instead of APIC */
#define TN_FSB_INT_DEL_CAP (1ll << 15)	/* read only, 1 = FSB delivery available */

#define HPET_INT_ENABLE(n) (*hpet_interrupt = (0x1 << n)) /* Clears the INT n for level-triggered mode. */

static volatile u32_t *hpet_capabilities;
static volatile u64_t *hpet_config;
static volatile u64_t *hpet_interrupt;
static void *hpet;
//static volatile cycles_t timeout;

volatile struct hpet_timer {
		u64_t config;
		u64_t compare;
		u64_t interrupt;
		u64_t reserved;
} __attribute__((packed)) *hpet_timers;

/*
 * When determining how many CPU cycles are in a HPET tick, we must
 * execute a number of periodic ticks (TIMER_CALIBRATION_ITER) at a
 * controlled interval, and use the HPET tick granularity to compute
 * how many CPU cycles per HPET tick there are.  Unfortunately, this
 * can be quite low (e.g. HPET tick of 10ns, CPU tick of 2ns) leading
 * to rounding error that is a significant fraction of the conversion
 * factor.
 *
 * Practically, this will lead to the divisor in the conversion being
 * smaller than it should be, thus causing timers to go off _later_
 * than they should.  Thus we use a multiplicative factor
 * (TIMER_ERROR_BOUND_FACTOR) to lessen the rounding error.
 *
 * All of the hardware is documented in the HPET specification @
 * http://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/software-developers-hpet-spec-1-0a.pdf
 */

#define PICO_PER_MICRO           1000000UL
#define FEMPTO_PER_PICO          1000UL
#define TIMER_CALIBRATION_ITER   256
#define TIMER_ERROR_BOUND_FACTOR 256
static int timer_calibration_init = 1;
static unsigned long timer_cycles_per_hpetcyc = TIMER_ERROR_BOUND_FACTOR;
static unsigned long cycles_per_tick;
static unsigned long hpetcyc_per_tick;
#define ULONG_MAX 4294967295UL

static inline u64_t
timer_cpu2hpet_cycles_print(u64_t cycles)
{
	unsigned long cyc;

	printk("%s: %llu ", __func__, cycles);
	/* demote precision to enable word-sized math */
	cyc    = (unsigned long)cycles;
	printk("%lu ", cyc);
	if (unlikely((u64_t)cyc < cycles)) cyc = ULONG_MAX;
	/* convert from CPU cycles to HPET cycles */
	cyc    = (cyc / timer_cycles_per_hpetcyc) * TIMER_ERROR_BOUND_FACTOR;
	/* promote the precision to interact with the hardware correctly */
	cycles = cyc;
	printk("%llu %lu\n", cycles, cyc);
	return cycles;
}

static inline u64_t
timer_cpu2hpet_cycles(u64_t cycles)
{
	unsigned long cyc;

	//printk("%s: %llu ", __func__, cycles);
	/* demote precision to enable word-sized math */
	cyc    = (unsigned long)cycles;
	//printk("%lu ", cyc);
	if (unlikely((u64_t)cyc < cycles)) cyc = ULONG_MAX;
	/* convert from CPU cycles to HPET cycles */
	cyc    = (cyc / timer_cycles_per_hpetcyc) * TIMER_ERROR_BOUND_FACTOR;
	/* promote the precision to interact with the hardware correctly */
	cycles = cyc;
	//printk("%llu %lu\n", cycles, cyc);
	return cycles;
}

static void
timer_disable(timer_type_t timer_type)
{
	/* Disable timer interrupts */
	*hpet_config ^= ~1;

	/* Reset main counter */
	hpet_timers[timer_type].config = 0;
	hpet_timers[timer_type].compare = 0;

	/* Enable timer interrupts */
	*hpet_config |= 1;
}

static void
timer_calibration(void)
{
	static int   cnt = 0;
	static u64_t cycle = 0, tot = 0, prev;

	prev = cycle;
	rdtscll(cycle);
	if (cnt) tot += cycle-prev;
	if (cnt >= TIMER_CALIBRATION_ITER) {
		assert(hpetcyc_per_tick);
		timer_calibration_init   = 0;
		cycles_per_tick          = (unsigned long)(tot / TIMER_CALIBRATION_ITER);
		assert(cycles_per_tick > hpetcyc_per_tick);

		/* Possibly significant rounding error here.  Bound by the factor */
		timer_cycles_per_hpetcyc = (TIMER_ERROR_BOUND_FACTOR * cycles_per_tick) / hpetcyc_per_tick;
		printk("Timer calibrated:\n\tCPU cycles (%d-%lld:%ld)per HPET tick: %ld\n\tHPET ticks in %d us: %ld\n",
		       cnt, tot, cycles_per_tick, timer_cycles_per_hpetcyc/TIMER_ERROR_BOUND_FACTOR, TIMER_DEFAULT_US_INTERARRIVAL, hpetcyc_per_tick);

		timer_disable(TIMER_PERIODIC);
		timer_disable(TIMER_PERIODIC);
		//timer_set(TIMER_ONESHOT, (cycles_t)(~0ULL));
	}
	cnt++;
}

int
chal_cyc_usec(void)
{ return cycles_per_tick / TIMER_DEFAULT_US_INTERARRIVAL; }

int
periodic_handler(struct pt_regs *regs)
{
	int preempt = 1;

	if (unlikely(timer_calibration_init)) timer_calibration();

	ack_irq(HW_PERIODIC);
	preempt = cap_hw_asnd(&hw_asnd_caps[HW_PERIODIC], regs);
	HPET_INT_ENABLE(TIMER_PERIODIC);

	return preempt;
}

extern int timer_process(struct pt_regs *regs);

int
oneshot_handler(struct pt_regs *regs)
{
//	static int count;
	int preempt = 1;
//	cycles_t now;
//	count ++;

//	if (count > 16) printk("%d ", count);
//	rdtscll(now);
//	printk("%llu %llu\n", timeout, now);
	ack_irq(HW_ONESHOT);
	preempt = timer_process(regs);
	HPET_INT_ENABLE(TIMER_ONESHOT);

	return preempt;
}

void
timer_set(timer_type_t timer_type, u64_t cycles)
{
	u64_t outconfig = TN_INT_TYPE_CNF | TN_INT_ENB_CNF;

	cycles = timer_cpu2hpet_cycles(cycles);

	/* Disable timer interrupts */
	*hpet_config ^= ~1;

	/* Reset main counter */
	if (timer_type == TIMER_ONESHOT) {
		//printk("Cycles: %llu\n", cycles);
	//	hpet_timers[TIMER_PERIODIC].config = 0;
		/* FIXME: Don't want to get/set this everytime. */
		/* Set a static value to count up to */
		hpet_timers[timer_type].config     = outconfig;
		cycles += HPET_COUNTER;
//		printk("HPET: %llu Cycles: %llu\n", HPET_COUNTER, cycles);
	} else {
		/* FIXME: Don't want to get/set this everytime. */
	//	hpet_timers[TIMER_ONESHOT].config = 0;
		/* Set a periodic value */
		hpet_timers[timer_type].config    = outconfig | TN_TYPE_CNF | TN_VAL_SET_CNF;
		/* Reset main counter */
		HPET_COUNTER = 0x00;
	}
	hpet_timers[timer_type].compare = cycles;
//	timeout = cycles;

	/* Enable timer interrupts */
	*hpet_config |= 1;
}

/* FIXME:  This is broken. Why does setting the oneshot twice make it work? */
void
chal_timer_set(cycles_t cycles)
{
//	timer_cpu2hpet_cycles_print(cycles);
	timer_set(TIMER_ONESHOT, cycles);
	timer_set(TIMER_ONESHOT, cycles);
}

u64_t
timer_find_hpet(void *timer)
{
	u32_t i;
	unsigned char sum = 0;
	unsigned char *hpetaddr = timer;
	u32_t length = *(u32_t*)(hpetaddr + HPET_TAB_LENGTH);

	printk("Initializing HPET @ %p\n", hpetaddr);

	for (i = 0; i < length; i++) {
		sum += hpetaddr[i];
	}

	if (sum == 0) {
		u64_t addr = *(u64_t*)(hpetaddr + HPET_TAB_ADDRESS);
		printk("\tChecksum is OK\n");
		printk("\tAddr: %016llx\n", addr);
		hpet = (void*)((u32_t)(addr & 0xffffffff));
		printk("\thpet: %p\n", hpet);
		return addr;
	}

	printk("\tInvalid checksum (%d)\n", sum);
	return 0;
}

void
timer_set_hpet_page(u32_t page)
{
	hpet              = (void*)(page * (1 << 22) | ((u32_t)hpet & ((1<<22)-1)));
	hpet_capabilities = (u32_t*)((unsigned char*)hpet + HPET_CAPABILITIES);
	hpet_config       = (u64_t*)((unsigned char*)hpet + HPET_CONFIGURATION);
	hpet_interrupt    = (u64_t*)((unsigned char*)hpet + HPET_INTERRUPT);
	hpet_timers       = (struct hpet_timer*)((unsigned char*)hpet + HPET_T0_CONFIG);

	printk("\tSet HPET @ %p\n", hpet);
}

void
timer_init(void)
{
	unsigned long pico_per_hpetcyc;

	assert(hpet_capabilities);
	pico_per_hpetcyc = hpet_capabilities[1]/FEMPTO_PER_PICO; /* bits 32-63 are # of femptoseconds per HPET clock tick */
	hpetcyc_per_tick = (TIMER_DEFAULT_US_INTERARRIVAL * PICO_PER_MICRO) / pico_per_hpetcyc;

	//hpetcyc_per_tick = (PICO_PER_MICRO) / pico_per_hpetcyc;
	//hpetcyc_per_tick *= TIMER_DEFAULT_US_INTERARRIVAL;

	printk("Enabling timer @ %p with tick granularity %lu:%ld picoseconds(%ld)\n", hpet, hpet_capabilities[1], pico_per_hpetcyc, hpetcyc_per_tick);
	/* Enable legacy interrupt routing */
	*hpet_config |= (1ll);

	/*
	 * Set the timer as specified.  This assumes that the cycle
	 * specification is in hpet cycles (not cpu cycles).
	 */
	timer_set(TIMER_PERIODIC, hpetcyc_per_tick);
}
