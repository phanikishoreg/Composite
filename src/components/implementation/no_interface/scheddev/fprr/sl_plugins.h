#ifndef SL_PLUGINS_H
#define SL_PLUGINS_H

#include <res_spec.h>
#include <sl_mod_policy.h>

/*
 * The build system has to add in the appropriate backends for the
 * system.  We rely on the linker to hook up all of these function
 * call instead of function pointers so that we can statically analyze
 * stack consumption and execution paths (e.g. for WCET) which are
 * prohibited by function pointers.  Additionally, significant work
 * (by Daniel Lohmann's group) has shown that a statically configured
 * system is more naturally fault resilient.  A minor benefit is the
 * performance of not using function pointers, but that is secondary.
 */
struct sl_thd_policy *sl_thd_alloc_backend(thdid_t tid);
void sl_thd_free_backend(struct sl_thd_policy *t);

void sl_thd_index_add_backend(struct sl_thd_policy *);
void sl_thd_index_rem_backend(struct sl_thd_policy *);
struct sl_thd_policy *sl_thd_lookup_backend(thdid_t);
void sl_thd_init_backend(void);

/*
 * Each scheduler policy must implement the following API.  See above
 * for why this is not a function-pointer-based API.
 *
 * Scheduler modules (policies) should define the following
 */
struct sl_thd_policy;
static inline struct sl_thd *sl_mod_thd_get(struct sl_thd_policy *tp);
static inline struct sl_thd_policy *sl_mod_thd_policy_get(struct sl_thd *t);

void sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles);
struct sl_thd_policy *sl_mod_schedule(void);

void sl_mod_deadlines(void);
void sl_mod_block(struct sl_thd_policy *t);
void sl_mod_wakeup(struct sl_thd_policy *t);
void sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *tp);

void sl_mod_thd_create(struct sl_thd_policy *t);
void sl_mod_thd_delete(struct sl_thd_policy *t);
void sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int val);
void sl_mod_init(void);
void sl_mod_init_sync(cycles_t start);

/* API for handling timer management */
void sl_timeout_mod_expended(cycles_t now, cycles_t oldtimeout);
void sl_timeout_mod_init(void);

/* API for waking up threads using timer management module */
void sl_timeout_mod_block(struct sl_thd *t, int implicit, cycles_t wkup_cycs);
struct sl_thd *sl_timeout_mod_block_peek(void);
void sl_timeout_mod_remove(struct sl_thd *t);
void sl_timeout_mod_wakeup_expired(cycles_t now);

#endif	/* SL_PLUGINS_H */
