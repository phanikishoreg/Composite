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
#include "inv.h"

#define HW_LINES_TOTAL 256
#define HW_LINES_EXTERNAL_MIN 32
#define HW_LINES_EXTERNAL_MAX 63

#ifdef DEV_RCV
struct cap_asnd hw_asnd_caps[HW_LINES_TOTAL];
#else
struct thread* hw_thd[HW_LINES_TOTAL];
#endif

struct cap_hw {
	struct cap_header h;
	u32_t hw_bitmap;
} __attribute__((packed));

#ifndef DEV_RCV
static void
hw_thdcap_init(void)
{
	printk("HW:Working with Threads..\n"); 
	memset(hw_thd, 0, sizeof(struct thread *) * HW_LINES_TOTAL); 
}
#else
static void
hw_asndcap_init(void)
{ 
	printk("HW:Working with Async Snd..\n"); 
	memset(&hw_asnd_caps, 0, sizeof(struct cap_asnd) * HW_LINES_TOTAL); 
}
#endif
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

#ifdef DEV_RCV
static int
hw_attach_rcvcap(struct cap_hw *hwc, hwid_t hwid, struct cap_arcv * rcvc, capid_t rcv_cap)
{
	if (hwid < HW_LINES_EXTERNAL_MIN || hwid > HW_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_LINES_EXTERNAL_MIN)))) return -EINVAL;
	if (hw_asnd_caps[hwid - 1].h.type == CAP_ASND) return -EEXIST;

	return asnd_copy(&hw_asnd_caps[hwid - 1], rcvc, rcv_cap, 0, 0);
}

static int
hw_detach_rcvcap(struct cap_hw *hwc, hwid_t hwid)
{
	if (hwid < HW_LINES_EXTERNAL_MIN || hwid > HW_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_LINES_EXTERNAL_MIN)))) return -EINVAL;

	memset(&hw_asnd_caps[hwid - 1], 0, sizeof(struct cap_asnd));

	return 0;
}
#else
static int
hw_attach_thd(struct cap_hw *hwc, hwid_t hwid, struct thread *thd)
{
	if (hwid < HW_LINES_EXTERNAL_MIN || hwid > HW_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_LINES_EXTERNAL_MIN)))) return -EINVAL;
	if (hw_thd[hwid - 1]) return -EEXIST;

	hw_thd[hwid - 1] = thd;

	return 0;
}

static int
hw_detach_thd(struct cap_hw *hwc, hwid_t hwid)
{
	if (hwid < HW_LINES_EXTERNAL_MIN || hwid > HW_LINES_EXTERNAL_MAX) return -EINVAL;
	if (!(hwc->hw_bitmap & (1 << (hwid - HW_LINES_EXTERNAL_MIN)))) return -EINVAL;

	hw_thd[hwid - 1] = NULL;

	return 0;
}
#endif
#endif /* HW_H */
