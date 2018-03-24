#include <sched.h>

int
sched_thd_wakeup(thdid_t t)
{
	sl_thd_wakeup(t);

	return 0;
}

int
sched_thd_block(thdid_t dep_t)
{
	sl_thd_block(dep_t);

	return 0;
}

cycles_t
sched_thd_block_timeout(thdid_t dep_t, cycles_t abs_timeout)
{
	return sl_thd_block_timeout(dep_t, abs_timeout);
}

thdid_t
sched_thd_create(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t = sl_thd_alloc(fn, data);

	if (!t) return 0;

	return sl_thd_thdid(t);
}

thdid_t
sched_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_aepkey_t key)
{
	struct sl_thd *t = sl_thd_aep_alloc(fn, data, owntc, key);

	if (!t) return 0;

	aep->thd  = sl_thd_thdcap(t);
	aep->tc   = sl_thd_tcap(t);
	aep->rcv  = sl_thd_rcvcap(t);
	aep->tid  = sl_thd_thdid(t);
	aep->fn   = fn;
	aep->data = data;

	return aep->tid;
}

int
sched_thd_param_set(thdid_t tid, sched_param_t p)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!t) return -1;

	sl_thd_param_set(t, p);

	return 0;
}

int
sched_thd_delete(thdid_t tid)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!t) return -1;

	sl_thd_free(t);

	return 0;
}

int
sched_thd_exit(void)
{
	sl_thd_exit();
	assert(0); /* should not reach here */

	return 0;
}
