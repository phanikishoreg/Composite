#include <memmgr.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

vaddr_t
memmgr_heap_page_alloc(void)
{
	return (vaddr_t)cos_page_bump_alloc(cos_compinfo_get(cos_defcompinfo_curr_get()));
}

vaddr_t
memmgr_heap_page_allocn(unsigned int num_pages)
{
	return (vaddr_t)cos_page_bump_allocn(cos_compinfo_get(cos_defcompinfo_curr_get()), num_pages * PAGE_SIZE);
}

/* This cannot be done with component local allocations. id would have to be local to this component and not global but what is the point of having shared mem within a component!! */
int
memmgr_shared_page_alloc(vaddr_t *pgaddr)
{
	assert(0);

	return -1;
}

int
memmgr_shared_page_allocn(int num_pages, vaddr_t *pgaddr)
{
	assert(0);

	return -1;
}

int
memmgr_shared_page_map(int id, vaddr_t *pgaddr)
{
	assert(0);

	return 0;
}

