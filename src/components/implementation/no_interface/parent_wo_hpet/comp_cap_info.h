#ifndef COMP_CAP_INFO_H
#define COMP_CAP_INFO_H

#include <cos_defkernel_api.h>
#include <sl.h>

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_defcompinfo defci;
	struct cos_compinfo *compinfo;
	pgtblcap_t utpt;
	vaddr_t addr_start;
	vaddr_t vaddr_mapped_in_booter;
	vaddr_t upcall_entry;
	struct sl_thd *t;
};

#endif /* COMP_CAP_INFO_H */
