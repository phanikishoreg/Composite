#ifndef HYPERCALL_H
#define HYPERCALL_H

#include <cos_kernel_api.h>
/* TODO: using token from sync inv creation */

/*
 * strong assumption:
 *  - capid_t though is unsigned long, only assuming it occupies 16bits for now
 */
#define HYPERCALL_ERROR 0xFFFF

enum hypercall_cntl {
	HYPERCALL_COMP_INIT_DONE = 0,
	HYPERCALL_COMP_INFO_GET, /* packed! <retval>, <pgtbl, captbl>, <compcap, parent_spdid> */
	HYPERCALL_COMP_INFO_NEXT, /* iterator to get comp_info */
	HYPERCALL_COMP_FRONTIER_GET, /* get current cap frontier & vaddr frontier of spdid comp */

	HYPERCALL_COMP_INITTHD_GET,
	HYPERCALL_COMP_CHILDSPDIDS_GET,
	HYPERCALL_COMP_CHILDSCHEDSPDIDS_GET,
};

/* assumption: spdids are monotonically increasing from 0 and max MAX_NUM_SPD == 64 */
static inline int 
hypercall_comp_childspdids_get(spdid_t c, u64_t *id_bits)
{
	word_t lo = 0, hi = 0;
	int ret;

	*id_bits = 0;
	ret = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_CHILDSPDIDS_GET, c, 0, &lo, &hi);
	*id_bits = ((u64_t)hi << 32) | (u64_t)lo;

	return ret;
}

static inline int 
hypercall_comp_childschedspdids_get(spdid_t c, u64_t *id_bits)
{
	word_t lo = 0, hi = 0;
	int ret;

	*id_bits = 0;
	ret = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_CHILDSCHEDSPDIDS_GET, c, 0, &lo, &hi);
	*id_bits = ((u64_t)hi << 32) | (u64_t)lo;

	return ret;
}

static inline int
hypercall_comp_init_done(void)
{
	/* DEPRICATED it seems, but not in my code base yet!*/
	return cos_sinv(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INIT_DONE, 0, 0);
}

static inline thdcap_t 
hypercall_comp_initthd_get(spdid_t spdid, arcvcap_t *rcv, tcap_t *tc, capid_t *myfr)
{
	word_t ret2;
	thdcap_t t;

	t = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INITTHD_GET, spdid, 0, myfr, &ret2);
	*rcv = (ret2 >> 16);
	*tc  = ((ret2 << 16) >> 16);

	return t;
}

static inline int
hypercall_comp_info_get(capid_t rcurfr, spdid_t spdid, pgtblcap_t *pgc, captblcap_t *capc, compcap_t *cc, spdid_t *psid, capid_t *rsfr)
{
	word_t r1 = 0, r2 = 0, r3 = 0;

	/*
	 * guess, hypercall layer must know it's coming from resmgr and allow only 
	 * that component special privilege to access anything.
	 * for all others, it should return error!
	 */
	r1    = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INFO_GET, spdid, rcurfr, &r2, &r3);
	*rsfr = ((r1 << 16) >> 16);
	if (*rsfr == HYPERCALL_ERROR) return -1;

	*pgc  = (r2 >> 16);
	*capc = ((r2 << 16) >> 16);
	*cc   = (r3 >> 16);
	*psid = ((r3 << 16) >> 16);

	return -1;
}

static inline int
hypercall_comp_info_next(capid_t rcurfr, spdid_t *sid, pgtblcap_t *pgc, captblcap_t *capc, compcap_t *cc, spdid_t *psid, capid_t *rsfr)
{
	word_t r1 = 0, r2 = 0, r3 = 0;

	/*
	 * guess, hypercall layer must know it's coming from resmgr and allow only 
	 * that component special privilege to access anything.
	 * for all others, it should return error!
	 */
	r1    = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_INFO_NEXT, 0, rcurfr, &r2, &r3);
	*rsfr = (int)((r1 << 16) >> 16);

	if (*rsfr == HYPERCALL_ERROR) return -1;

	*sid  = (r1 >> 16);
	*pgc  = (r2 >> 16);
	*capc = ((r2 << 16) >> 16);
	*cc   = (r3 >> 16);
	*psid = ((r3 << 16) >> 16);

	return 0;
}

static inline int
hypercall_comp_frontier_get(spdid_t spdid, vaddr_t *vasfr, capid_t *capfr)
{
	int ret = 0;
	word_t r1 = 0, r2 = 0, r3 = 0;

	/*
	 * guess, hypercaller must know it's coming from resmgr and allow only 
	 * that component special privilege to access anything.
	 * for all others, it should return error!
	 */
	r1  = cos_sinv_3rets(BOOT_CAPTBL_SINV_CAP, 0, HYPERCALL_COMP_FRONTIER_GET, spdid, 0, &r2, &r3);
	if (r1) return -1;

	*capfr = ((r2 << 16) >> 16);
	*vasfr = (vaddr_t)r3;

	return ret;
}

#endif /* HYPERCALL_H */