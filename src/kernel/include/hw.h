/**
 * Copyright 2016 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef HW_H
#define HW_H

#include "component.h"
#include "thd.h"
#include "chal/call_convention.h"

#define IRQ_LINES_TOTAL 256
#define IRQ_LINES_EXTERNAL_MIN 32
#define IRQ_LINES_EXTERNAL_MAX 63

struct thread* hw_thd[IRQ_LINES_TOTAL];
/* why thdcap, save thd pointer instead */
/* capid_t hw_thdcap[IRQ_LINES_TOTAL]; */
/* capid_t hw_rcvcap[IRQ_LINES_TOTAL]; */

struct cap_hw {
	struct cap_header h;
	u32_t hw_bitmap;
} __attribute__((packed));

static void
hw_thdcap_init(void)
{ memset(hw_thd, 0, sizeof(struct thread *) * IRQ_LINES_TOTAL); }

static int
hw_activate(struct captbl *t, capid_t cap, capid_t capin, u32_t irq_lines)
{
	struct cap_hw *hwc;
	int ret;

	hwc = (struct cap_hw *)__cap_capactivate_pre(t, cap, capin, CAP_HW, &ret);
	if (!hwc) return ret;
	hwc->hw_bitmap = irq_lines;
	__cap_capactivate_post(&hwc->h, CAP_HW);

	return 0;
}

static int
hw_deactivate(struct cap_captbl *t, capid_t capin, livenessid_t lid)
{ return cap_capdeactivate(t, capin, CAP_HW, lid); }

/*
static int
hw_attach_rcvcap(struct cap_hw *hwc, u32_t irqline, capid_t rcvcap)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_rcvcap[irqline - 1] = rcvcap;
	return 0;
}

static int
hw_detach_rcvcap(struct cap_hw *hwc, u32_t irqline)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_rcvcap[irqline - 1] = 0;
	return 0;
}
*/
/*static int
hw_attach_thdcap(struct cap_hw *hwc, u32_t irqline, capid_t thdcap)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thdcap[irqline] = thdcap;
	return 0;
}

static int
hw_detach_thdcap(struct cap_hw *hwc, u32_t irqline)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thdcap[irqline] = 0;
	return 0;
}*/
/*
static int
hw_attach_thd(struct cap_hw *hwc, u32_t irqline, struct thread *thd)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thd[irqline - 1] = thd;
	return 0;
}

static int
hw_detach_thd(struct cap_hw *hwc, u32_t irqline)
{
	if (irqline < IRQ_LINES_EXTERNAL_MIN || irqline > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (irqline - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thd[irqline - 1] = NULL;
	return 0;
}*/

static int
hw_attach_thd(struct cap_hw *hwc, hwid_t hwid, struct thread *thd)
{
	if (hwid < IRQ_LINES_EXTERNAL_MIN || hwid > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thd[hwid - 1] = thd;
	return 0;
}

static int
hw_detach_thd(struct cap_hw *hwc, hwid_t hwid)
{
	if (hwid < IRQ_LINES_EXTERNAL_MIN || hwid > IRQ_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - IRQ_LINES_EXTERNAL_MIN)))) return -EINVAL;
	hw_thd[hwid - 1] = NULL;
	return 0;
}
#endif /* HW_H */
