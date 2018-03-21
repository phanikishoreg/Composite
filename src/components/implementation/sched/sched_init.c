#include <schedinit.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sched_info.h>

extern unsigned int self_init, num_child_init;
extern thdcap_t capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid);

int
schedinit_child(cbuf_t id)
{
	spdid_t c = cos_inv_token();
	thdid_t thdid  = 0;
	struct cos_defcompinfo *dci;
	struct sched_childinfo *ci;

	if (!c) return -1;
	ci  = sched_childinfo_find(c);
	/* is a child sched? */
	if (!ci || !(ci->flags & COMP_FLAG_SCHED)) return -1;
	dci = sched_child_defci_get(ci);
	if (!dci) return -1;

	/* thd retrieve */
	do {
		struct sl_thd *t = NULL;
		struct cos_aep_info aep;

		memset(&aep, 0, sizeof(struct cos_aep_info));
		aep.thd = capmgr_thd_retrieve_next(c, &thdid);
		if (!thdid) break;
		t = sl_thd_lkup(thdid);
		/* already in? only init thd, coz it's created by this sched! */
		if (unlikely(t)) continue;

		aep.tid = thdid;
		aep.tc  = sl_thd_tcap(sl__globals()->sched_thd);
		t = sl_thd_init_ext(&aep, sched_child_initthd_get(ci));
		if (!t) return -1;
	} while (thdid);

	sched_shminfo_init(sched_child_shminfo_get(ci), id);
	num_child_init++;

	return 0;
}
