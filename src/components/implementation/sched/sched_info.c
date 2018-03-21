#include <cos_kernel_api.h>
#include <ps.h>
#include <sched_info.h>
#include <hypercall.h>
#include <sl.h>

#define SCHED_MAX_CHILD_COMPS 8
static struct sched_childinfo childinfo[SCHED_MAX_CHILD_COMPS];
static unsigned int sched_num_child = 0;
static unsigned int sched_num_childsched = 0;

struct sched_shm_info sched_shinfo_cur;

unsigned int self_init = 0, num_child_init = 0;

/* implementation specific initialization per child */
extern void sched_child_init(struct sched_childinfo *schedci);

struct sched_childinfo *
sched_childinfo_find(spdid_t id)
{
	unsigned int i;

	for (i = 0; i < sched_num_child; i ++) {
		if (childinfo[i].id == id) return &(childinfo[i]);
	}

	return NULL;
}

struct sched_childinfo *
sched_childinfo_alloc(spdid_t id, compcap_t compcap, comp_flag_t flags)
{
	int idx = 0;
	struct sched_childinfo *sci = NULL;
	struct cos_defcompinfo *dci = NULL;

	assert(sched_num_child < SCHED_MAX_CHILD_COMPS - 1);
	idx = ps_faa((unsigned long *)&sched_num_child, 1);
	sci = &childinfo[idx];
	dci = sched_child_defci_get(sci);

	if (flags & COMP_FLAG_SCHED) ps_faa((unsigned long *)&sched_num_childsched, 1);

	if (compcap) {
		struct cos_compinfo *ci = cos_compinfo_get(dci);

		ci->comp_cap = compcap;
	} else {
		cos_defcompinfo_childid_init(dci, id);
	}
	sci->id = id;
	sci->flags = flags;

	return sci;
}

unsigned int
sched_num_child_get(void)
{
	return sched_num_child;
}

unsigned int
sched_num_childsched_get(void)
{
	return sched_num_childsched;
}

static void
sched_childinfo_init_intern(int is_raw)
{
	int remaining = 0;
	spdid_t child;
	comp_flag_t childflags;

	memset(childinfo, 0, sizeof(struct sched_childinfo) * SCHED_MAX_CHILD_COMPS);
	memset(&sched_shinfo_cur, 0, sizeof(struct sched_shm_info));

	while ((remaining = hypercall_comp_child_next(cos_spd_id(), &child, &childflags)) >= 0) {
		struct cos_defcompinfo *child_dci = NULL;
		struct sched_childinfo *schedinfo = NULL;
		struct sl_thd          *initthd   = NULL;
		compcap_t               compcap   = 0;

		PRINTLOG(PRINT_DEBUG, "Initializing child component %u, is_sched=%d\n", child, childflags & COMP_FLAG_SCHED);
		if (is_raw) compcap = hypercall_comp_compcap_get(child);

		schedinfo = sched_childinfo_alloc(child, compcap, childflags);
		assert(schedinfo);
		child_dci = sched_child_defci_get(schedinfo);

		initthd = sl_thd_initaep_alloc(child_dci, NULL, childflags & COMP_FLAG_SCHED, childflags & COMP_FLAG_SCHED ? 1 : 0, 0);
		assert(initthd);
		sched_child_initthd_set(schedinfo, initthd);

		sched_child_init(schedinfo);
		if (!remaining) break;
	}

	assert(sched_num_child_get()); /* at least 1 child component */
	if (!is_raw) sched_shminfo_init(sched_shminfo_cur_get(), 0);
}

void
sched_childinfo_init(void)
{
	sched_childinfo_init_intern(0);
}

void
sched_childinfo_init_raw(void)
{
	sched_childinfo_init_intern(1);
}
