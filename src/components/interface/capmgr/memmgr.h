#ifndef MEMMGR_H
#define MEMMGR_H

#include <cos_types.h>
#include <cos_component.h>

vaddr_t memmgr_heap_page_alloc(void);
vaddr_t memmgr_heap_page_allocn(unsigned int num_pages);

vaddr_t memmgr_pa2va_map(paddr_t pa, unsigned int len);

cbuf_t memmgr_shared_page_alloc(vaddr_t *pgaddr);
cbuf_t memmgr_shared_page_allocn(u32_t num_pages, vaddr_t *pgaddr);
u32_t  memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr);

#endif /* MEMMGR_H */
