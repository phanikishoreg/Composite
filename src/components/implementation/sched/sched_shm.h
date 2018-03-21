#ifndef SCHED_SHM_H
#define SCHED_SHM_H

#include <sched_info.h>

#define SCHED_MAX_SHM_PAGES 1

struct sched_shm_info {
	cbuf_t  id;
	vaddr_t addr;
	u32_t   npages;
};

extern struct sched_shm_info sched_shinfo_cur;

extern cbuf_t memmgr_shared_page_allocn(u32_t num_pages, vaddr_t *pgaddr);
extern u32_t  memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr);

static inline struct sched_shm_info *
sched_shminfo_cur_get(void)
{
	return &sched_shinfo_cur;
}

static inline void
sched_shminfo_init(struct sched_shm_info *shi, cbuf_t id)
{
	if (!id) {
		shi->id = memmgr_shared_page_allocn(SCHED_MAX_SHM_PAGES, &(shi->addr));
		shi->npages = SCHED_MAX_SHM_PAGES;
	} else {
		shi->id = id;
		shi->npages = memmgr_shared_page_map(id, &(shi->addr));
	}

	assert(shi->npages == SCHED_MAX_SHM_PAGES);
}

static inline cbuf_t
sched_shminfo_id(struct sched_shm_info *shi)
{
	return shi->id;
}

static inline vaddr_t
sched_shminfo_addr(struct sched_shm_info *shi)
{
	return shi->addr;
}

static inline vaddr_t
sched_shminfo_npages(struct sched_shm_info *shi)
{
	return shi->npages;
}

#endif /* SCHED_SHM_H */
