#include <aurora/mm/vmm.h>
#include <aurora/mm/pmm.h>
#include <aurora/arch/x86_64/cr.h>
#include <aurora/printk.h>
#include <aurora/string.h>

static uint64_t *kernel_pml4 = NULL;

void vmm_invlpg(uint64_t virt)
{
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t *vmm_get_kernel_pml4(void)
{
    if (!kernel_pml4) {
        uint64_t cr3 = read_cr3() & ~0xFFFULL;
        kernel_pml4 = (uint64_t *)vmm_phys_to_virt(cr3);
    }
    return kernel_pml4;
}

int vmm_map_page(uint64_t *pml4, uint64_t virt, uint64_t phys, uint64_t flags)
{
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    /* 1. Level 4 -> Level 3 (PDPT) */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uintptr_t pdpt_phys = pmm_alloc_page();
        if (!pdpt_phys) {
            return -1;
        }
        memset(vmm_phys_to_virt(pdpt_phys), 0, PAGE_SIZE);
        pml4[pml4_idx] = pdpt_phys | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else if (flags & PTE_USER) {
        pml4[pml4_idx] |= PTE_USER;
    }

    uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    /* 2. Level 3 -> Level 2 (PD) */
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uintptr_t pd_phys = pmm_alloc_page();
        if (!pd_phys) {
            return -1;
        }
        memset(vmm_phys_to_virt(pd_phys), 0, PAGE_SIZE);
        pdpt[pdpt_idx] = pd_phys | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else if (flags & PTE_USER) {
        pdpt[pdpt_idx] |= PTE_USER;
    }

    uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    /* 3. Level 2 -> Level 1 (PT) */
    if (!(pd[pd_idx] & PTE_PRESENT) || (pd[pd_idx] & PTE_HUGE)) {
        uintptr_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            return -1;
        }
        memset(vmm_phys_to_virt(pt_phys), 0, PAGE_SIZE);
        pd[pd_idx] = pt_phys | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    } else if (flags & PTE_USER) {
        pd[pd_idx] |= PTE_USER;
    }

    uint64_t *pt = (uint64_t *)vmm_phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);

    /* 4. Set PT entry */
    pt[pt_idx] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;

    vmm_invlpg(virt);
    return 0;
}

int vmm_unmap_page(uint64_t *pml4, uint64_t virt)
{
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        return -1;
    }
    uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return -1;
    }
    uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return -1;
    }
    uint64_t *pt = (uint64_t *)vmm_phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);

    pt[pt_idx] = 0;
    vmm_invlpg(virt);
    return 0;
}

uint64_t vmm_virt_to_phys(uint64_t *pml4, uint64_t virt)
{
    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        return 0;
    }
    uint64_t *pdpt = (uint64_t *)vmm_phys_to_virt(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return 0;
    }
    uint64_t *pd = (uint64_t *)vmm_phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return 0;
    }

    if (pd[pd_idx] & PTE_HUGE) {
        return (pd[pd_idx] & ~0x1FFFFFULL) | (virt & 0x1FFFFFULL);
    }

    uint64_t *pt = (uint64_t *)vmm_phys_to_virt(pd[pd_idx] & PTE_ADDR_MASK);
    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return 0;
    }

    return (pt[pt_idx] & PTE_ADDR_MASK) | (virt & 0xFFFULL);
}

void vmm_init(void)
{
    kernel_pml4 = vmm_get_kernel_pml4();

    /* Unmap the early boot identity map (PML4[0]) to reclaim clean user space (0..512GB) */
    kernel_pml4[0] = 0;
    vmm_invlpg(0);

    printk("<6>[  0.000000] VMM: 4-Level Paging initialized (PML4 at %p)\n", (void *)read_cr3());
    printk("<6>[  0.000000] VMM: Direct physical memory map active at 0x%p\n", (void *)KERNEL_DIRECT_MAP_BASE);
}
