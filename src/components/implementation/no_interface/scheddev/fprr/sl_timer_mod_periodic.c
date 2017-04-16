#include <sl.h>
#include <sl_consts.h>
#include <sl_plugins.h>

#include <heap.h>

#define SL_TIMEOUT_MOD_MAX_THDS MAX_NUM_THREADS

struct wakeup_heap {
	struct heap h;
	void       *data[SL_TIMEOUT_MOD_MAX_THDS];
	char        p; /* pad */
} wakeup_heap;

static struct heap *hs = (struct heap *)&wakeup_heap;

static void
__sl_timeout_mod_wakeup_expired(cycles_t now)
{
//	printc("C");
	cycles_t nw;
	if (!heap_size(hs)) return;

	//rdtscll(nw);
	nw = now;
	int idx = 1;
	do {
		struct sl_thd *tp, *th;

		//printc("$ %d %d, ", heap_size(hs), idx);
		tp = heap_peek_at(hs, idx);
		assert(tp);

//		if (tp->wakeup_cycs > nw) goto next;
		if (!sl_cycles_same(tp->wakeup_cycs, nw) && (tp->wakeup_cycs > nw)) goto next;
		/* AEP threads are explicitly woken up by wakeup scheduling events.. */
		if (tp->type == SL_THD_AEP || tp->type == SL_THD_AEP_TCAP) goto next;
		sl_print("T:%u ", tp->thdid);
//		printc("T:%u:%llu ", tp->thdid, nw - tp->wakeup_cycs);

//		printc(" W:%u:%llu:%llu:%llu ", tp->thdid, nw, tp->wakeup_cycs, nw - tp->wakeup_cycs);
		//printc("# %d %d, ", heap_size(hs), idx);
		th = heap_remove(hs, idx);
		assert(th && th == tp);
		th->wakeup_idx = -1;
		assert(th->type != SL_THD_AEP && th->type != SL_THD_AEP_TCAP);
		if (th->type == SL_THD_SIMPLE) {
			sl_thd_wakeup_cs(th);
		} else {
			sl_mod_wakeup(sl_mod_thd_policy_get(th));
		}
next:
		idx ++;
	} while (idx <= heap_size(hs));
}

static struct sl_thd * 
__sl_timeout_mod_block_peek(void)
{ return heap_peek(hs); }

void
sl_timeout_mod_block(struct sl_thd *t, int implicit, cycles_t wkup_cycs)
{
	cycles_t now;
	sl_print("%u", t->thdid);

	assert(t);
	//assert(t->wakeup_idx == -1); /* valid thread and not already in heap */
	if (t->wakeup_idx != -1) return;
	assert(heap_size(hs) < SL_TIMEOUT_MOD_MAX_THDS); /* heap full! */

	if (!(t->period)) assert(!implicit);

	if (implicit) {	
		t->wakeup_cycs += t->period; /* implicit wakeups! update to next period */
	} else {
		t->wakeup_cycs  = wkup_cycs;
	}

	heap_add(hs, t);
}

void
sl_timeout_mod_remove(struct sl_thd *t)
{
	assert(heap_size(hs)); 
	if (t->wakeup_idx <= 0) return;

	assert(t->type == SL_THD_AEP_TCAP || t->type == SL_THD_AEP);
	heap_remove(hs, t->wakeup_idx);
	t->wakeup_idx = -1;
}

void
sl_timeout_mod_wakeup_expired(cycles_t now)
{
	/* wakeup any blocked threads! */
	__sl_timeout_mod_wakeup_expired(now);
}

struct sl_thd *
sl_timeout_mod_block_peek(void)
{ return __sl_timeout_mod_block_peek(); }

void
sl_timeout_mod_expended(microsec_t now, microsec_t oldtimeout)
{
	cycles_t offset;

	assert(now >= oldtimeout);

	/* in virtual environments, or with very small periods, we might miss more than one period */
	offset = (now - oldtimeout) % sl_timeout_period_get();
	sl_timeout_oneshot(now + sl_timeout_period_get() - offset);

//	__sl_timeout_mod_wakeup_expired(now);
}

static int
__compare_min(void *a, void *b)
{
	struct sl_thd *ta = (struct sl_thd *)a;
	struct sl_thd *tb = (struct sl_thd *)b;

	if ((ta->prio <= tb->prio)) return 1; // && (ta->wakeup_cycs <= tb->wakeup_cycs)) return 1;
	return 0;
//	return ((struct sl_thd *)a)->wakeup_cycs <= ((struct sl_thd *)b)->wakeup_cycs;
}

static void
__update_idx(void *e, int pos)
{ ((struct sl_thd *)e)->wakeup_idx = pos; }

void
sl_timeout_mod_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
	heap_init(hs, SL_TIMEOUT_MOD_MAX_THDS, __compare_min, __update_idx); 
}
