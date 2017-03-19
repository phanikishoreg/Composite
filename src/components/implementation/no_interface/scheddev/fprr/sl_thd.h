/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#ifndef SL_THD_H
#define SL_THD_H

#include <cos_defkernel_api.h>

typedef enum {
	SL_THD_FREE = 0,
	SL_THD_BLOCKED,
	SL_THD_WOKEN, 		/* if a race causes a wakeup before the inevitable block */
	SL_THD_RUNNABLE,
	SL_THD_DYING,
} sl_thd_state;

typedef enum {
	SL_THD_SIMPLE = 0,
	SL_THD_AEP,
	SL_THD_AEP_TCAP,        /* AEP with it's own tcap - interrupt handlers! */
	SL_THD_CHILD_SCHED,
	SL_THD_CHILD_NOSCHED,   /* Child component invokes parent for scheduling! */
} sl_thd_type;

struct sl_thd {
	sl_thd_state         state;
	sl_thd_type          type;
	thdid_t              thdid;
	struct cos_aep_info *aep;
	asndcap_t            sndcap; /* TODO: unused for now. */
	tcap_prio_t          prio;
	struct sl_thd       *dependency;
};

#ifndef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN() do { while (1) ; } while (0)
#endif

static inline struct cos_aep_info *
sl_thd_aep(struct sl_thd *t)
{ return t->aep; }

static inline thdcap_t
sl_thd_thd(struct sl_thd *t)
{ return sl_thd_aep(t)->thd; }

#endif /* SL_THD_H */
