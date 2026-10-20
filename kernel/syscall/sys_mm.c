#include <aurora/syscall.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/errno.h>
#include <aurora/string.h>

#define USER_BRK_MAX 0x0000700000000000ULL

int64_t sys_brk(uintptr_t new_brk)
{
    task_t *curr = sched_current();
    if (!curr) {
        return -ESRCH;
    }

    if (new_brk == 0) {
        return (int64_t)curr->brk_current;
    }

    if (new_brk < curr->brk_start || new_brk >= USER_BRK_MAX) {
        return -EINVAL;
    }

    if (new_brk > curr->brk_current) {
        uint64_t start_page = PAGE_ALIGN(curr->brk_current);
        uint64_t end_page = PAGE_ALIGN(new_brk);

        uint64_t *pml4 = curr->pml4 ? curr->pml4 : vmm_get_kernel_pml4();

        for (uint64_t v = start_page; v < end_page; v += PAGE_SIZE) {
            uintptr_t phys = pmm_alloc_page();
            if (!phys) {
                /* Rollback any partially allocated pages to prevent leak */
                for (uint64_t rollback = start_page; rollback < v; rollback += PAGE_SIZE) {
                    uint64_t p = vmm_virt_to_phys(pml4, rollback);
                    vmm_unmap_page(pml4, rollback);
                    if (p) {
                        pmm_free_page(p & PAGE_MASK);
                    }
                }
                return -ENOMEM;
            }
            memset(vmm_phys_to_virt(phys), 0, PAGE_SIZE);
            vmm_map_page(pml4, v, phys, PTE_USER | PTE_WRITABLE | PTE_NX);
        }

        curr->brk_current = new_brk;
    }

    return (int64_t)curr->brk_current;
}
