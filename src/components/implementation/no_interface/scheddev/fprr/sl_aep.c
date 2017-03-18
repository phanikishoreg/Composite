#include <consts.h>
#include <cos_defkernel_api.h>
#include <sl_aep.h>

static struct cos_aep_info __sl_aeps[MAX_NUM_THREADS];
static int __sl_aep_alloc_idx;

struct cos_aep_info *
sl_aep_alloc(void)
{
	struct cos_aep_info *aep = &__sl_aeps[__sl_aep_alloc_idx];

	__sl_aep_alloc_idx ++;
	assert(__sl_aep_alloc_idx < MAX_NUM_THREADS);
	aep->tc = aep->thd = aep->rcv = 0;
	aep->fn = aep->data = NULL;
	return aep;
}
	
void
sl_aep_free(struct cos_aep_info *aep)
{ /* TODO: reusing slots */ }

void
sl_aep_init(void)
{ __sl_aep_alloc_idx = 0; }
