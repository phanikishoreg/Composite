#ifndef BOOT_DEPS_H
#define BOOT_DEPS_H

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0;} } while(0)

#include <cos_alloc.h>
#include <cobj_format.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include "comp_cap_info.h"

#define CHILD_UNTYPED_SIZE (1<<26)

/* Assembly function for sinv from new component */
extern void *__inv_test_entry(int a, int b, int c);

struct cobj_header *hs[MAX_NUM_SPDS+1];

struct comp_cap_info new_comp_cap_info[MAX_NUM_SPDS+1];

struct cos_defcompinfo *boot_dci;
struct cos_compinfo *boot_ci;

static vaddr_t
boot_deps_map_sect(int spdid, vaddr_t dest_daddr)
{
	struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;
	vaddr_t addr = (vaddr_t) cos_page_bump_alloc(boot_ci);
	assert(addr);
			
	if (cos_mem_alias_at(child_ci, dest_daddr, boot_ci, addr)) BUG();

	child_ci->vas_frontier      = dest_daddr + PAGE_SIZE;
        child_ci->vasrange_frontier = round_up_to_pgd_page(child_ci->vas_frontier);
        assert(child_ci->vasrange_frontier == round_up_to_pgd_page(child_ci->vasrange_frontier));

	return addr;
}

static void
boot_comp_pgtbl_expand(int n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	int i;	
	int tot = 0;	
	/* Expand Page table, could do this faster */
	for (i = 0 ; i < (int)h->nsect ; i++) {
		tot += cobj_sect_size(h, i);
	}
	
	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0 ; i < n_pte ; i++) {
		if (!cos_pgtbl_intern_alloc(boot_ci, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(int spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	*ct = cos_captbl_alloc(boot_ci);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_ci);
	assert(*pt);

	new_comp_cap_info[spdid].utpt = cos_pgtbl_alloc(boot_ci);
	assert(new_comp_cap_info[spdid].utpt);

	new_comp_cap_info[spdid].compinfo = cos_compinfo_get(&new_comp_cap_info[spdid].defci);

	cos_meminfo_init(&(new_comp_cap_info[spdid].compinfo->mi), BOOT_MEM_KM_BASE, CHILD_UNTYPED_SIZE, new_comp_cap_info[spdid].utpt);
	cos_compinfo_init(new_comp_cap_info[spdid].compinfo, *pt, *ct, 0, 
				  (vaddr_t)vaddr, BOOT_CAPTBL_FREE, boot_ci);
}

static void
boot_newcomp_init_copy(int spdid, int is_sched)
{
	struct cos_compinfo *ci             = boot_ci;
	struct cos_compinfo *child_ci       = new_comp_cap_info[spdid].compinfo;
	struct cos_defcompinfo *child_defci = &new_comp_cap_info[spdid].defci;
	int ret;

	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_CT, ci, child_ci->captbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_PT, ci, child_ci->pgtbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_UNTYPED_PT, ci, new_comp_cap_info[spdid].utpt);
	assert(ret == 0);
	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_COMP, ci, child_ci->comp_cap);
	assert(ret == 0);

	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_INITTHD_BASE, ci, child_defci->sched_aep.thd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_INITHW_BASE, ci, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	if (is_sched) {
		ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_INITTCAP_BASE, ci, child_defci->sched_aep.tc);
		assert(ret == 0);
		ret = cos_cap_cpy_at(child_ci, BOOT_CAPTBL_SELF_INITRCV_BASE, ci, child_defci->sched_aep.rcv);
		assert(ret == 0);
	} else {
		/* TODO: make sinv caps here or how? */
	}
}

static void
boot_newcomp_meminfo_alloc(int spdid, vaddr_t start, unsigned long size)
{
	struct cos_compinfo *child_ci = new_comp_cap_info[spdid].compinfo;

	cos_meminfo_alloc(child_ci, start, size);
}

static void
boot_newcomp_create(int spdid)
{
	compcap_t   cc;
	captblcap_t ct = new_comp_cap_info[spdid].compinfo->captbl_cap;
	pgtblcap_t  pt = new_comp_cap_info[spdid].compinfo->pgtbl_cap;
	int is_sched = 1;

	cc = cos_comp_alloc(boot_ci, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);	
	new_comp_cap_info[spdid].compinfo->comp_cap = cc;

	cos_defcompinfo_child_init(&new_comp_cap_info[spdid].defci, is_sched);

	boot_newcomp_init_copy(spdid, is_sched);
	boot_newcomp_meminfo_alloc(spdid, BOOT_MEM_KM_BASE, CHILD_UNTYPED_SIZE);	
}

static void
boot_bootcomp_init(void)
{
	boot_dci = cos_defcompinfo_curr_get();
	boot_ci  = cos_compinfo_get(boot_dci);

	cos_meminfo_init(&(boot_ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
}

static void
boot_done(void)
{
	printc("llboot - done creating system\n");
}

#endif /* BOOT_DEPS_H */
