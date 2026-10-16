#ifndef _AURORA_MM_PMM_H
#define _AURORA_MM_PMM_H

#include <aurora/types.h>

#define PAGE_SIZE       4096ULL
#define PAGE_SHIFT      12
#define PAGE_MASK       (~(PAGE_SIZE - 1))

#define PAGE_ALIGN(addr)    (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(a)  ((a) & PAGE_MASK)

/* Page flags for future struct page mem_map */
#define PG_RESERVED (1 << 0)
#define PG_SLAB     (1 << 1)
#define PG_ANON     (1 << 2)
#define PG_FILE     (1 << 3)

struct pmm_stats {
    size_t total_pages;
    size_t free_pages;
    size_t used_pages;
    size_t reserved_pages;
};

void pmm_init(void);

uintptr_t pmm_alloc_page(void);
uintptr_t pmm_alloc_pages(size_t count);

void pmm_free_page(uintptr_t phys_addr);
void pmm_free_pages(uintptr_t phys_addr, size_t count);

void pmm_get_stats(struct pmm_stats *stats);

#endif /* _AURORA_MM_PMM_H */
