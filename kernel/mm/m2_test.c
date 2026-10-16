#include <aurora/mm/m2_test.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>

#define STRESS_ITERATIONS 1000000

bool test_memory_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M2 verification test...\n");

    /* --- 1. PMM Allocation & Free Test --- */
    struct pmm_stats stats_before;
    pmm_get_stats(&stats_before);

    uintptr_t page1 = pmm_alloc_page();
    uintptr_t page2 = pmm_alloc_page();
    uintptr_t pages16 = pmm_alloc_pages(16);

    if (!page1 || !page2 || !pages16 || page1 == page2) {
        printk("<0>[TEST PMM] FAILED: Invalid physical page allocation!\n");
        return false;
    }

    pmm_free_page(page1);
    pmm_free_page(page2);
    pmm_free_pages(pages16, 16);

    struct pmm_stats stats_after;
    pmm_get_stats(&stats_after);

    if (stats_before.free_pages != stats_after.free_pages) {
        printk("<0>[TEST PMM] FAILED: Free page count mismatch (leak detected)!\n");
        return false;
    }
    printk("<6>[TEST PMM] OK: Physical page alloc/free verified with zero leaks.\n");

    /* --- 2. VMM Mapping & Translation Test --- */
    uint64_t *pml4 = vmm_get_kernel_pml4();
    uintptr_t test_phys = pmm_alloc_page();
    uint64_t test_virt = 0xFFFF800070000000ULL; /* Test direct virtual address */

    if (vmm_map_page(pml4, test_virt, test_phys, PTE_WRITABLE) != 0) {
        printk("<0>[TEST VMM] FAILED: Could not map test page!\n");
        return false;
    }

    volatile uint64_t *ptr = (volatile uint64_t *)test_virt;
    *ptr = 0xCAFEBABE12345678ULL;

    volatile uint64_t *direct_ptr = (volatile uint64_t *)vmm_phys_to_virt(test_phys);
    if (*direct_ptr != 0xCAFEBABE12345678ULL) {
        printk("<0>[TEST VMM] FAILED: Value mismatch through virtual mapping!\n");
        return false;
    }

    vmm_unmap_page(pml4, test_virt);
    pmm_free_page(test_phys);
    printk("<6>[TEST VMM] OK: 4-level paging and direct map translation verified.\n");

    /* --- 3. Kernel Heap 1 Million Alloc/Free Stress Test (Acceptance Criteria) --- */
    printk("<6>[TEST HEAP] Running 1,000,000 alloc/free stress test...\n");

    struct heap_stats hstats_start;
    heap_get_stats(&hstats_start);

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        /* Cycle through various slab sizes: 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 */
        size_t size = 16 << (i % 9);
        void *p = kmalloc(size, GFP_KERNEL);
        if (!p) {
            printk("<0>[TEST HEAP] FAILED: kmalloc(%llu) returned NULL at iter %d!\n",
                   (unsigned long long)size, i);
            return false;
        }

        /* Write pattern to ensure memory is writable */
        *(volatile uint32_t *)p = (uint32_t)(0x12345670 + (i % 16));

        kfree(p);
    }

    /* Test large allocation */
    void *large = kmalloc(65536, GFP_ZERO); /* 64 KB */
    if (!large) {
        printk("<0>[TEST HEAP] FAILED: Large kmalloc returned NULL!\n");
        return false;
    }
    kfree(large);

    struct heap_stats hstats_end;
    heap_get_stats(&hstats_end);

    if (hstats_end.active_allocations != hstats_start.active_allocations) {
        printk("<0>[TEST HEAP] FAILED: Active allocations remaining: %llu\n",
               (unsigned long long)hstats_end.active_allocations);
        return false;
    }

    printk("<6>[TEST HEAP] OK: 1,000,000 alloc/free iterations completed with 0 leaks!\n");
    printk("<6>[  0.000000] Milestone M2 status: [OK] PMM, VMM, Direct Map, Slab Heap verified.\n");

    return true;
}
