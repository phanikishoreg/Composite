#ifndef SL_MOD_POLICY_H
#define SL_MOD_POLICY_H

#include <sl_thd.h>
#include <ps_list.h>

struct sl_thd_policy {
	struct sl_thd  thd;
	tcap_prio_t    priority;
	microsec_t     period_usec, budget_usec;
	cycles_t       period, deadline, last_period;
	tcap_res_t     budget, expended;

	int            prio_idx;

#ifdef SL_DEBUG_DEADLINES
	u32_t          missed, made;
#endif
};

#ifdef SL_DEBUG_DEADLINES
unsigned long long dl_missed, dl_made;
#endif

extern cycles_t task_start_time;

static inline cycles_t
sl_mod_get_task_starttime(void)
{ return task_start_time; }

static inline struct sl_thd *
sl_mod_thd_get(struct sl_thd_policy *tp)
{ return &tp->thd; }

static inline struct sl_thd_policy *
sl_mod_thd_policy_get(struct sl_thd *t)
{ return ps_container(t, struct sl_thd_policy, thd); }

#endif /* SL_MOD_POLICY_H */
