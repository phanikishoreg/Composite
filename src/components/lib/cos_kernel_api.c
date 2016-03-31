/*
 * Copyright 2015, Qi Wang and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_kernel_api.h>

/* HACKHACKHACKHACKHACKHACK */
#include <stdarg.h>
#include <stdio.h>
#ifdef NIL
static int __attribute__((format(printf,1,2)))
printd(char *fmt, ...)
{
	char s[128];
	va_list arg_ptr;
	int ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_print(s, ret);

	return ret;
}
#else
#define printd(...)
#endif

void
cos_meminfo_init(struct cos_meminfo *mi, vaddr_t umem_ptr, unsigned long umem_sz,
		 vaddr_t untyped_ptr, unsigned long untyped_sz)
{
	mi->umem_ptr      = umem_ptr;
	mi->untyped_ptr   = untyped_ptr;
	mi->umem_range    = umem_ptr + umem_sz;
	mi->untyped_range = untyped_ptr + untyped_sz;
}

static inline struct cos_compinfo *
__compinfo_metacap(struct cos_compinfo *ci)
{ return ci->memsrc; }

void
cos_compinfo_init(struct cos_compinfo *ci, captblcap_t pgtbl_cap, pgtblcap_t captbl_cap,
		  compcap_t comp_cap, vaddr_t heap_ptr, capid_t cap_frontier,
		  struct cos_compinfo *ci_resources)
{
	assert(ci && ci_resources);
	assert(cap_frontier % CAPMAX_ENTRY_SZ == 0);

	ci->memsrc = ci_resources;
	assert(ci_resources->memsrc == ci_resources); /* prevent infinite data-structs */

	ci->pgtbl_cap    = pgtbl_cap;
	ci->captbl_cap   = captbl_cap;
	ci->comp_cap     = comp_cap;
	ci->vas_frontier = heap_ptr;
	ci->cap_frontier = cap_frontier;
	/*
	 * The first allocation should trigger PTE allocation, unless
	 * it is in the middle of a PGD, in which case we assume one
	 * is already allocated.
	 */
	ci->vasrange_frontier = round_up_to_pgd_page(heap_ptr);
	assert(ci->vasrange_frontier == round_up_to_pgd_page(ci->vasrange_frontier));
	/*
	 * captbls are initialized populated with a single
	 * second-level node.
	 */
	if (cap_frontier < CAPTBL_EXPAND_SZ) {
		ci->caprange_frontier = round_up_to_pow2(cap_frontier, CAPTBL_EXPAND_SZ);
	} else {
		ci->caprange_frontier = round_up_to_pow2(cap_frontier + CAPTBL_EXPAND_SZ, CAPTBL_EXPAND_SZ);
	}
	ci->cap16_frontier = ci->cap32_frontier = ci->cap64_frontier = cap_frontier;
}

/**************** [Memory Capability Allocation Functions] ***************/

static vaddr_t
__mem_bump_alloc(struct cos_compinfo *__ci, int km, vaddr_t *mem_ptr, vaddr_t *mem_range)
{
	vaddr_t ret = 0;
	struct cos_compinfo *ci;

	printd("__mem_bump_alloc\n");

	assert(__ci);
	ci = __compinfo_metacap(__ci);
	assert(ci && ci == __compinfo_metacap(__ci));
	if (*mem_ptr >= *mem_range) return 0;

	/*
	 * TODO: We need separate lists for user/kernel memory if we have a
	 * single pool that can handle either.
	 */
	if (*mem_ptr % RETYPE_MEM_SIZE == 0) {
		/* are we dealing with a kernel memory allocation? */
		syscall_op_t op = km ? CAPTBL_OP_MEM_RETYPE2KERN : CAPTBL_OP_MEM_RETYPE2USER;

		ret = call_cap_op(ci->pgtbl_cap, op, *mem_ptr, 0, 0, 0);
		if (ret) return 0;
	}

	ret = *mem_ptr;
	*mem_ptr += PAGE_SIZE;

	return ret;
}

static vaddr_t
__kmem_bump_alloc(struct cos_compinfo *ci)
{
	printd("__kmem_bump_alloc\n");
	return __mem_bump_alloc(ci, 1, &ci->mi.untyped_ptr, &ci->mi.untyped_range);
}

/* this should back-up to using untyped memory... */
static vaddr_t
__umem_bump_alloc(struct cos_compinfo *ci)
{
	printd("__umem_bump_alloc\n");
	return __mem_bump_alloc(ci, 0, &ci->mi.umem_ptr, &ci->mi.umem_range);
}

/**************** [Capability Allocation Functions] ****************/

static capid_t __capid_bump_alloc(struct cos_compinfo *ci, cap_t cap);

static int
__capid_captbl_check_expand(struct cos_compinfo *ci)
{
	/* the compinfo that tracks/allocates resources */
	struct cos_compinfo *meta = __compinfo_metacap(ci);
	/* do we manage our own resources, or does a separate meta? */
	int self_resources = (meta == ci);
	capid_t frontier;

	capid_t captblcap;
	capid_t captblid_add;
	vaddr_t kmem;

	/* ensure that we have bounded structure, and bounded recursion */
	assert(__compinfo_metacap(meta) == meta);

	printd("__capid_captbl_check_expand\n");
	/*
	 * Do we need to expand the capability table?
	 *
	 * This is testing the following: If we are past the first
	 * CAPTBL_EXPAND_SZ (second level in the captbl), and we are a
	 * multiple of page allocation which is _two_ second-level
	 * captbl entries.
	 *
	 * Note also that we need space in the capability table for
	 * the capability to the next node in the page-table, and
	 * perhaps for the memory capability, thus the "off by one"
	 * logic in here.
	 *
	 * Assumptions: 1. When a captbl is allocated, the first
	 * CAPTBL_EXPAND_SZ capabilities are automatically available
	 * (creation implies a second-level captbl node).  2. when we
	 * expand the captbl, we do it in CAPTBL_EXPAND_SZ times 2
	 * increments. 3. IFF the ci is used for its own memory
	 * allocation and capability table tracking, the last
	 * cache-line in each captbl internal node is reserved for the
	 * capability for the next internal node.  This will waste the
	 * rest of the entry (internal fragmentation WRT the captbl
	 * capability).  Oh well.
	 */

	if (self_resources) frontier = ci->caprange_frontier - CAPMAX_ENTRY_SZ;
	else                frontier = ci->caprange_frontier;
	assert(ci->cap_frontier <= frontier);

	/* Common case: */
	if (likely(ci->cap_frontier != frontier)) return 0;

	kmem = __kmem_bump_alloc(ci);
	assert(kmem); /* FIXME: should have a failure semantics for capids */

	if (self_resources) {
		captblcap = frontier;
	} else {
		/* Recursive call: can recur maximum 2 times. */
		captblcap = __capid_bump_alloc(meta, CAP_CAPTBL);
		assert(captblcap);
	}
	captblid_add = ci->caprange_frontier;
	assert(captblid_add % CAPTBL_EXPAND_SZ == 0);

	printd("__capid_captbl_check_expand->pre-captblactivate (%d)\n", CAPTBL_OP_CAPTBLACTIVATE);
	/* captbl internal node allocated with the resource provider's captbls */
	if (call_cap_op(meta->captbl_cap, CAPTBL_OP_CAPTBLACTIVATE, captblcap, meta->pgtbl_cap, kmem, 1)) {
		assert(0); /* race condition? */
		return -1;
	}
	printd("__capid_captbl_check_expand->post-captblactivate\n");
	/*
	 * Assumption:
	 * meta->captbl_cap refers to _our_ captbl, thus
	 * captblcap's use in the following.
	 */

	/* Construct captbl */
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_CONS, captblcap, captblid_add, 0, 0)) {
		assert(0); /* race? */
		return -1;
	}

	/* Success!  Advance the frontiers. */
	ci->cap_frontier      = ci->caprange_frontier;
	ci->caprange_frontier = ci->caprange_frontier + (CAPTBL_EXPAND_SZ * 2);

	return 0;
}

static capid_t
__capid_bump_alloc_generic(struct cos_compinfo *ci, capid_t *capsz_frontier, cap_sz_t sz)
{
	capid_t ret;

	printd("__capid_bump_alloc_generic\n");

	/*
	 * Do we need a new cache-line in the capability table for
	 * this size of capability?
	 */
	if (*capsz_frontier % CAPMAX_ENTRY_SZ == 0) {
		*capsz_frontier = ci->cap_frontier;
		ci->cap_frontier += CAPMAX_ENTRY_SZ;
		if (__capid_captbl_check_expand(ci)) return 0;
	}

	ret = *capsz_frontier;
	*capsz_frontier += sz;

	return ret;
}

/* allocate a new capid in the booter. */
static capid_t
__capid_bump_alloc(struct cos_compinfo *ci, cap_t cap)
{
	capid_t ret;
	unsigned long sz = captbl_idsize(cap);
	capid_t *frontier;

	printd("__capid_bump_alloc\n");

	switch(sz) {
	case CAP16B_IDSZ:
		frontier = &ci->cap16_frontier;
		break;
	case CAP32B_IDSZ:
		frontier = &ci->cap32_frontier;
		break;
	case CAP64B_IDSZ:
		frontier = &ci->cap64_frontier;
		break;
	default:
		return -1;
	}
	return __capid_bump_alloc_generic(ci, frontier, sz);
}

/**************** [User Virtual Memory Allocation Functions] ****************/

static vaddr_t
__page_bump_valloc(struct cos_compinfo *ci)
{
	vaddr_t heap_vaddr;
	struct cos_compinfo *meta = __compinfo_metacap(ci);

	printd("__page_bump_alloc\n");

	assert(meta == __compinfo_metacap(meta)); /* prevent unbounded structures */
	heap_vaddr = ci->vas_frontier;

	/* Do we need to allocate a PTE? */
	if (heap_vaddr == ci->vasrange_frontier) {
		capid_t pte_cap;
		vaddr_t ptemem_cap;

		pte_cap    = __capid_bump_alloc(meta, CAP_PGTBL);
		ptemem_cap = __kmem_bump_alloc(meta);
		/* TODO: handle the case of running out of memory */
		if (pte_cap == 0 || ptemem_cap == 0) return 0;

		/* PTE */
		if (call_cap_op(meta->captbl_cap, CAPTBL_OP_PGTBLACTIVATE,
				pte_cap, meta->pgtbl_cap, ptemem_cap, 1)) {
			assert(0); /* race? */
			return 0;
		}

		/* Construct pgtbl */
		if (call_cap_op(ci->pgtbl_cap, CAPTBL_OP_CONS, pte_cap, heap_vaddr, 0, 0)) {
			assert(0); /* race? */
			return 0;
		}

		ci->vasrange_frontier += PGD_RANGE;
		assert(ci->vasrange_frontier == round_up_to_pgd_page(ci->vasrange_frontier));
	}

	/* FIXME: make atomic WRT concurrent allocations */
	ci->vas_frontier += PAGE_SIZE;

	return heap_vaddr;
}

static vaddr_t
__page_bump_alloc(struct cos_compinfo *ci)
{
	struct cos_compinfo *meta = __compinfo_metacap(ci);
	vaddr_t heap_vaddr, umem;

	/* Allocate the virtual address to map into */
	heap_vaddr = __page_bump_valloc(ci);
	if (unlikely(!heap_vaddr)) return 0;

	/*
	 * Allocate the memory to map into that virtual address.
	 *
	 * FIXME: if this fails, we should also back out the page_bump_valloc
	 */
	umem = __umem_bump_alloc(ci);
	if (!umem) return 0;

	/* Actually map in the memory. FIXME: cleanup! */
	if (call_cap_op(meta->pgtbl_cap, CAPTBL_OP_MEMACTIVATE, umem,
			ci->pgtbl_cap, heap_vaddr, 0)) {
		assert(0);
		return 0;
	}

	return heap_vaddr;
}

/**************** [Liveness Allocation] ****************/

/*
 * TODO: This won't be generic until we have per-component liveness
 * namespaces.  This will _only work in the low-level booter_.
 */
CACHE_ALIGNED static u32_t livenessid_frontier = BOOT_LIVENESS_ID_BASE;

static u32_t
livenessid_bump_alloc(void)
{ return livenessid_frontier++; }

/**************** [Kernel Object Allocation] ****************/

static int
__alloc_mem_cap(struct cos_compinfo *ci, cap_t ct, vaddr_t *kmem, capid_t *cap)
{
	printd("__alloc_mem_cap\n");

	*kmem = __kmem_bump_alloc(ci);
	if (!*kmem)   return -1;
	*cap  = __capid_bump_alloc(ci, ct);
	if (!*cap) return -1;
	return 0;
}

static thdcap_t
__cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, int init_data)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_thd_alloc\n");

	assert(ci && comp > 0);

	if (__alloc_mem_cap(ci, CAP_THD, &kmem, &cap)) return 0;
	assert((size_t)init_data < sizeof(u16_t)*8);
	/* TODO: Add cap size checking */
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_THDACTIVATE, (init_data << 16) | cap, ci->pgtbl_cap, kmem, comp)) BUG();

	return cap;
}

#include <cos_thd_init.h>

thdcap_t
cos_thd_alloc(struct cos_compinfo *ci, compcap_t comp, cos_thd_fn_t fn, void *data)
{
	int idx = cos_thd_init_alloc(fn, data);
	thdcap_t ret;

	if (idx < 1) return 0;
	ret = __cos_thd_alloc(ci, comp, idx);
	if (!ret) cos_thd_init_free(idx);

	return ret;
}

thdcap_t
cos_initthd_alloc(struct cos_compinfo *ci, compcap_t comp)
{ return __cos_thd_alloc(ci, comp, 1); }

captblcap_t
cos_captbl_alloc(struct cos_compinfo *ci)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_captbl_alloc\n");

	assert(ci);

	if (__alloc_mem_cap(ci, CAP_CAPTBL, &kmem, &cap)) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_CAPTBLACTIVATE, cap, ci->pgtbl_cap, kmem, 0)) BUG();

	return cap;
}

pgtblcap_t
cos_pgtbl_alloc(struct cos_compinfo *ci)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_pgtbl_alloc\n");

	assert(ci);

	if (__alloc_mem_cap(ci, CAP_PGTBL, &kmem, &cap)) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_PGTBLACTIVATE, cap, ci->pgtbl_cap, kmem, 0))  BUG();

	return cap;
}

compcap_t
cos_comp_alloc(struct cos_compinfo *ci, captblcap_t ctc, pgtblcap_t ptc, vaddr_t entry)
{
	capid_t cap;
	u32_t   lid = livenessid_bump_alloc();

	printd("cos_comp_alloc\n");

	assert(ci && ctc && ptc && lid);

	cap = __capid_bump_alloc(ci, CAP_COMP);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_COMPACTIVATE, cap, (ctc<<16) | ptc, lid, entry)) BUG();

	return cap;
}

/* Allocate an entire new component and initialize ci with it's data */
int
cos_compinfo_alloc(struct cos_compinfo *ci, vaddr_t heap_ptr, vaddr_t entry,
		   struct cos_compinfo *ci_resources)
{
	pgtblcap_t ptc;
	captblcap_t ctc;
	compcap_t compc;

	printd("cos_compinfo_alloc\n");

	ptc = cos_pgtbl_alloc(ci_resources);
	assert(ptc);
	ctc = cos_captbl_alloc(ci_resources);
	assert(ctc);
	compc = cos_comp_alloc(ci_resources, ctc, ptc, entry);
	assert(compc);

	cos_compinfo_init(ci, ptc, ctc, compc, heap_ptr, 0, ci_resources);

	return 0;
}

sinvcap_t
cos_sinv_alloc(struct cos_compinfo *srcci, compcap_t dstcomp, vaddr_t entry)
{
	capid_t cap;

	printd("cos_sinv_alloc\n");

	assert(srcci && dstcomp);

	cap = __capid_bump_alloc(srcci, CAP_COMP);
	if (!cap) return 0;
	if (call_cap_op(srcci->captbl_cap, CAPTBL_OP_SINVACTIVATE, cap, dstcomp, entry, 0)) BUG();

	return cap;
}

/*
 * Arguments:
 * thdcap:  the thread to activate on snds to the rcv endpoint.
 * tcap:    the tcap to use for that execution.
 * compcap: the component the rcv endpoint is visible in.
 * arcvcap: the rcv * endpoint that is the scheduler to be activated
 *          when the thread blocks on this endpoint.
 */
arcvcap_t
cos_arcv_alloc(struct cos_compinfo *ci, thdcap_t thdcap, tcap_t tcapcap, compcap_t compcap, arcvcap_t arcvcap)
{
	capid_t cap;

	assert(ci && thdcap && compcap);

	cap = __capid_bump_alloc(ci, CAP_ARCV);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_ARCVACTIVATE, cap, thdcap | (tcapcap << 16), compcap, arcvcap)) BUG();

	return cap;
}

asndcap_t
cos_asnd_alloc(struct cos_compinfo *ci, arcvcap_t arcvcap, captblcap_t ctcap)
{
	capid_t cap;

	assert(ci && arcvcap && ctcap);

	cap = __capid_bump_alloc(ci, CAP_ASND);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_ASNDACTIVATE, cap, ctcap, arcvcap, 0))  BUG();

	return cap;
}

/*
 * TODO: bitmap must be a subset of existing one.
 *       but there is no such check now, violates access control policy.
 */
hwcap_t
cos_hw_alloc(struct cos_compinfo *ci, u32_t bitmap)
{
	capid_t cap;

	assert(ci);

	cap = __capid_bump_alloc(ci, CAP_HW);
	if (!cap) return 0;
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_HW_ACTIVATE, cap, bitmap, 0, 0))  BUG();

	return cap;
}

void *
cos_page_bump_alloc(struct cos_compinfo *ci)
{ return (void*)__page_bump_alloc(ci); }

/**************** [Kernel Object Operations] ****************/

int
cos_thd_switch(thdcap_t c)
{ return call_cap_op(c, 0, 0, 0, 0, 0); }

int
cos_asnd(asndcap_t snd)
{ return call_cap_op(snd, 0, 0, 0, 0, 0); }

int
cos_rcv(arcvcap_t rcv, thdid_t *thdid, int *receiving, cycles_t *cycles)
{
	unsigned long thd_state = 0;
	unsigned long cyc = 0;
	int ret;

	ret = call_cap_retvals_asm(rcv, 0, 0, 0, 0, 0, &thd_state, &cyc);

	*receiving = (int)(thd_state >> (sizeof(thd_state)*8-1));
	*thdid = (thdid_t)(thd_state & ((1 << (sizeof(thdid_t)*8))-1));
	*cycles = cyc;

	return ret;
}

int
cos_mem_alias(pgtblcap_t ptdst, vaddr_t dst, pgtblcap_t ptsrc, vaddr_t src)
{
	assert(0);
	return 0;
}

int
cos_mem_remove(pgtblcap_t pt, vaddr_t addr)
{
	assert(0);
	return 0;
}

int
cos_mem_move(pgtblcap_t ptdst, vaddr_t dst, pgtblcap_t ptsrc, vaddr_t src)
{
	assert(0);
	return 0;
}

/* TODO: generalize to modify all state */
int
cos_thd_mod(struct cos_compinfo *ci, thdcap_t tc, void *tlsaddr)
{ return call_cap_op(ci->captbl_cap, CAPTBL_OP_THDTLSSET, tc, (int)tlsaddr, 0, 0); }

/* FIXME: problems when we got to 64 bit systems with the return value */
int
cos_introspect(struct cos_compinfo *ci, capid_t cap, unsigned long op)
{ return call_cap_op(ci->captbl_cap, CAPTBL_OP_INTROSPECT, cap, (int)op, 0, 0); }

/***************** [Kernel Tcap Operations] *****************/

static tcap_t
__cos_tcap_alloc(struct cos_compinfo *ci, tcap_t src, tcap_split_flags_t flags)
{
	vaddr_t kmem;
	capid_t cap;

	printd("cos_tcap_alloc\n");
	assert (ci);

	if (__alloc_mem_cap(ci, CAP_TCAP, &kmem, &cap)) return 0;

	/* TODO: Add cap size checking */
	if (call_cap_op(ci->captbl_cap, CAPTBL_OP_TCAP_ACTIVATE, (flags << 16) | cap, ci->pgtbl_cap, kmem, src)) BUG();

	return cap;
}

tcap_t
cos_tcap_split(struct cos_compinfo *ci, tcap_t src, tcap_res_t res, tcap_prio_t prio, tcap_split_flags_t flags)
{
	int prio_higher = (u32_t)(prio >> 32);
	int prio_lower  = (u32_t)((prio << 32) >> 32);
	tcap_t ret;

	ret = __cos_tcap_alloc(ci, src, flags);
	if (res != 0 && ret > 0 &&
	    call_cap_op(src, CAPTBL_OP_TCAP_TRANSFER, ret, res, prio_higher, prio_lower)) return 0;

	return ret;
}

int
cos_tcap_transfer(tcap_t src, tcap_t dst, tcap_res_t res, tcap_prio_t prio)
{
	int prio_higher = (u32_t)(prio >> 32);
	int prio_lower  = (u32_t)((prio << 32) >> 32);

	return call_cap_op(src, CAPTBL_OP_TCAP_TRANSFER, dst, res, prio_higher, prio_lower);
}

int
cos_tcap_delegate(tcap_t src, arcvcap_t dst, tcap_res_t res, tcap_prio_t prio, tcap_deleg_flags_t flags)
{
	u32_t dispatch  = (flags & TCAP_DELEG_DISPATCH) == TCAP_DELEG_DISPATCH;
	/* top bit is if we are dispatching or not */
	int prio_higher = (u32_t)(prio >> 32) | (dispatch << ((sizeof(dispatch)*8)-1));
	int prio_lower  = (u32_t)((prio << 32) >> 32);
	int ret = -EINVAL;

	return call_cap_op(src, CAPTBL_OP_TCAP_DELEGATE, dst, res, prio_higher, prio_lower);
}

int
cos_tcap_merge(tcap_t dst, tcap_t rm)
{ return call_cap_op(dst, CAPTBL_OP_TCAP_MERGE, rm, 0, 0, 0); }

int
cos_hw_attach(hwcap_t hwc, hwid_t hwid, arcvcap_t arcv)
{ return call_cap_op(hwc, CAPTBL_OP_HW_ATTACH, hwid, arcv, 0, 0); }

int
cos_hw_detach(hwcap_t hwc, hwid_t hwid)
{ return call_cap_op(hwc, CAPTBL_OP_HW_DETACH, hwid, 0, 0, 0); }

int
cos_hw_cycles_per_usec(hwcap_t hwc)
{ return call_cap_op(hwc, CAPTBL_OP_HW_CYC_USEC, 0, 0, 0, 0); }

void *
cos_hw_map(struct cos_compinfo *ci, hwcap_t hwc, paddr_t pa)
{
	vaddr_t va = __page_bump_valloc(ci);

	if (unlikely(!va)) return NULL;

	return (void*)call_cap_op(hwc, CAPTBL_OP_HW_MAP, ci->pgtbl_cap, va, pa, 0);
}
