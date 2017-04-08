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
__sl_timeout_mod_wakeup(cycles_t now)
{
	if (!heap_size(hs)) return;

	int idx = 1;
	do {
		struct sl_thd *tp, *th;

		//printc("$ %d %d, ", heap_size(hs), idx);
		tp = heap_peek_at(hs, idx);
		assert(tp);
		idx ++;

		if (idx >= heap_size(hs)) break;
		if (tp->wakeup_cycs > now) continue;
		sl_print("T:%u ", tp->thdid);

		idx --;
		//printc("# %d %d, ", heap_size(hs), idx);
		th = heap_remove(hs, idx);
		assert(th && th == tp);
		th->wakeup_idx = -1;
		assert(th->type != SL_THD_AEP && th->type != SL_THD_AEP_TCAP);
		if (th->type == SL_THD_SIMPLE || th->type == SL_THD_CHILD_NOSCHED) sl_thd_wakeup_cs(th);
		else                                                               sl_mod_wakeup(sl_mod_thd_policy_get(th));
	} while (heap_size(hs));
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

	/* AEPs are woken up not by timeout module but by some other thread/interrupt */
	if (t->type == SL_THD_AEP || t->type == SL_THD_AEP_TCAP) return;

	rdtscll(now);
	if (implicit) {	
		t->wakeup_cycs += t->period; /* implicit wakeups! update to next period */
		while (t->wakeup_cycs < now) {
			t->wakeup_cycs += t->period;
		}
		sl_print("$%u:%llu:%llu$", t->thdid, t->wakeup_cycs, now);
	} else {
		t->wakeup_cycs  = wkup_cycs;
	}

	heap_add(hs, t);
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

	/* wakeup any blocked threads! */
	__sl_timeout_mod_wakeup(now);
}

static int
__compare_min(void *a, void *b)
{
	struct sl_thd *ta = (struct sl_thd *)a;
	struct sl_thd *tb = (struct sl_thd *)b;

	if ((ta->prio <= tb->prio) && (ta->wakeup_cycs <= tb->wakeup_cycs)) return 1;
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
