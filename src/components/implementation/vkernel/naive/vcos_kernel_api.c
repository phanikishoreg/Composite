/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_kernel_api.h>
#include "vcos_kernel_api.h"
#include <cos_thd_init.h>

extern struct cos_compinfo vkern_info;
extern struct cos_compinfo vmbooter_info; /* FIXME: because only limited args can be passed into SINV */

thdcap_t
vcos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{
	/*
	 * can't just call cos_thd_alloc in child because child does not have memory
	 * So we allocate in the vKernel and then move over to the virtualized
	 * component
	 */ 
	thdcap_t sthd = cos_thd_alloc(&vkern_info, comp, fn, data);
	assert(sthd);
	thdcap_t dthd = cos_cap_copy(&vkern_info, sthd, CAP_THD, ci);
	assert(dthd);
	/* FIXME: Doesn't work without copying this cap back to vkern! */
	cos_cap_init(ci, dthd, &vkern_info);

	printc("sthd: %d, dthd: %d\n", sthd, dthd);
	return dthd;
}

/* TODO: Will not support these for Grid computing project */
thdcap_t
vcos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp)
{ return cos_initthd_alloc(ci, comp); }

captblcap_t
vcos_captbl_alloc(struct cos_compinfo *ci)
{ return vcos_captbl_alloc(ci); }

pgtblcap_t
vcos_pgtbl_alloc(struct cos_compinfo *ci)
{ return cos_pgtbl_alloc(ci); }

compcap_t
vcos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry)
{ return cos_comp_alloc(ci, ctc, ptc, entry); }

int
vcos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry,
		   struct cos_compinfo *ci_resources)
{ return cos_compinfo_alloc(ci, heap_ptr, entry, ci_resources); }
/* -------------------------------------------------------*/

sinvcap_t
vcos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry)
{ return cos_sinv_alloc(srcci, dstcomp, entry); }

arcvcap_t
vcos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t arcvcap)
{ return cos_arcv_alloc(ci, thdcap, tcapcap, compcap, arcvcap); }

asndcap_t
vcos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap)
{ return cos_asnd_alloc(ci, arcvcap, ctcap); }

hwcap_t
vcos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap)
{ return cos_hw_alloc(ci, bitmap); }

void *
vcos_page_bump_alloc(struct cos_compinfo *ci)
{ return cos_page_bump_alloc(ci); }

