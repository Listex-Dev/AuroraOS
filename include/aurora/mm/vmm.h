#ifndef _AURORA_MM_VMM_H
#define _AURORA_MM_VMM_H

#include <aurora/types.h>

#define KERNEL_DIRECT_MAP_BASE 0xFFFF800000000000ULL
#define KERNEL_HEAP_START      0xFFFF810000000000ULL
#define KERNEL_HEAP_INITIAL    (16 * 1024 * 1024ULL) /* 16 MB initial heap pool */

/* Page Table Entry Flags */
#define PTE_PRESENT       (1ULL << 0)
#define PTE_WRITABLE      (1ULL << 1)
#define PTE_USER          (1ULL << 2)
#define PTE_WRITETHROUGH  (1ULL << 3)
#define PTE_NOCACHE       (1ULL << 4)
#define PTE_ACCESSED      (1ULL << 5)
#define PTE_DIRTY         (1ULL << 6)
#define PTE_HUGE          (1ULL << 7)
#define PTE_GLOBAL        (1ULL << 8)
#define PTE_NX            (1ULL << 63)

#define PTE_ADDR_MASK     0x000FFFFFFFFFF000ULL

static inline void *vmm_phys_to_virt(uint64_t phys)
{
    return (void *)(KERNEL_DIRECT_MAP_BASE + phys);
}

static inline uint64_t vmm_virt_to_phys_direct(void *virt)
{
    return (uint64_t)virt - KERNEL_DIRECT_MAP_BASE;
}

void vmm_init(void);

int vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(uint64_t *pml4, uint64_t virt);
uint64_t vmm_virt_to_phys(uint64_t *pml4, uint64_t virt);

void vmm_invlpg(uint64_t virt);
uint64_t *vmm_get_kernel_pml4(void);
uint64_t *vmm_clone_user_space(uint64_t *src_pml4);
void vmm_destroy_user_space(uint64_t *pml4);

#endif /* _AURORA_MM_VMM_H */
