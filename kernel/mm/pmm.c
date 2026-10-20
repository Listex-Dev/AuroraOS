#include <aurora/mm/pmm.h>
#include <aurora/bootinfo.h>
#include <aurora/printk.h>
#include <aurora/string.h>
#include <aurora/sync/spinlock.h>

extern char kernel_end_phys[];

#define KERNEL_DIRECT_MAP_BASE 0xFFFF800000000000ULL

static uint8_t *bitmap;
static size_t total_memory_pages = 0;
static size_t free_page_count = 0;
static size_t used_page_count = 0;
static size_t last_search_idx = 0;
static spinlock_t pmm_lock = SPINLOCK_INIT;

static inline void *phys_to_virt(uint64_t phys)
{
    return (void *)(KERNEL_DIRECT_MAP_BASE + phys);
}

static inline void bitmap_set(size_t page_idx)
{
    bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static inline void bitmap_clear(size_t page_idx)
{
    bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static inline bool bitmap_test(size_t page_idx)
{
    return (bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

void pmm_init(void)
{
    /* Determine the highest physical address from boot memory map */
    uint64_t max_phys_addr = 0;

    for (uint32_t i = 0; i < g_boot_info.mmap_count; i++) {
        if (g_boot_info.mmap[i].type == BOOT_MEM_AVAILABLE) {
            uint64_t end = g_boot_info.mmap[i].base + g_boot_info.mmap[i].length;
            if (end > max_phys_addr) {
                max_phys_addr = end;
            }
        }
    }

    if (max_phys_addr == 0) {
        max_phys_addr = g_boot_info.total_memory_bytes;
    }

    total_memory_pages = max_phys_addr / PAGE_SIZE;
    size_t bitmap_size_bytes = (total_memory_pages + 7) / 8;
    bitmap_size_bytes = PAGE_ALIGN(bitmap_size_bytes);

    /* Place bitmap immediately after the kernel image in physical memory */
    uint64_t bitmap_phys_start = PAGE_ALIGN((uint64_t)kernel_end_phys);
    uint64_t bitmap_phys_end = bitmap_phys_start + bitmap_size_bytes;

    bitmap = (uint8_t *)phys_to_virt(bitmap_phys_start);

    /* Initially mark all pages as used/reserved */
    memset(bitmap, 0xFF, bitmap_size_bytes);
    used_page_count = total_memory_pages;
    free_page_count = 0;

    /* Mark available memory ranges as free (0) */
    for (uint32_t i = 0; i < g_boot_info.mmap_count; i++) {
        if (g_boot_info.mmap[i].type == BOOT_MEM_AVAILABLE) {
            uint64_t start = PAGE_ALIGN(g_boot_info.mmap[i].base);
            uint64_t end = PAGE_ALIGN_DOWN(g_boot_info.mmap[i].base + g_boot_info.mmap[i].length);

            size_t start_page = start / PAGE_SIZE;
            size_t end_page = end / PAGE_SIZE;

            if (end_page > total_memory_pages) {
                end_page = total_memory_pages;
            }

            for (size_t p = start_page; p < end_page; p++) {
                if (bitmap_test(p)) {
                    bitmap_clear(p);
                    used_page_count--;
                    free_page_count++;
                }
            }
        }
    }

    /* 1. Reserve first 1 MiB (0 - 0x100000) for BIOS/IVT/EBDA/hardware */
    size_t bios_pages = 0x100000 / PAGE_SIZE;
    for (size_t p = 0; p < bios_pages && p < total_memory_pages; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            free_page_count--;
            used_page_count++;
        }
    }

    /* 2. Reserve kernel image and bitmap physical area */
    size_t kernel_start_page = 0x100000 / PAGE_SIZE;
    size_t kernel_end_page = bitmap_phys_end / PAGE_SIZE;
    for (size_t p = kernel_start_page; p < kernel_end_page && p < total_memory_pages; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            free_page_count--;
            used_page_count++;
        }
    }

    /* 3. Reserve multiboot info structure location */
    if (g_boot_info.mbi_addr != 0) {
        size_t mbi_page = g_boot_info.mbi_addr / PAGE_SIZE;
        if (mbi_page < total_memory_pages && !bitmap_test(mbi_page)) {
            bitmap_set(mbi_page);
            free_page_count--;
            used_page_count++;
        }
    }

    printk("<6>[  0.000000] PMM: Bitmap allocator active (%llu KB bitmap at %p)\n",
           (unsigned long long)(bitmap_size_bytes / 1024), (void *)bitmap_phys_start);
    printk("<6>[  0.000000] PMM: Total: %llu MB, Free: %llu MB, Used: %llu MB\n",
           (unsigned long long)((total_memory_pages * PAGE_SIZE) / (1024 * 1024)),
           (unsigned long long)((free_page_count * PAGE_SIZE) / (1024 * 1024)),
           (unsigned long long)((used_page_count * PAGE_SIZE) / (1024 * 1024)));
}

static uintptr_t pmm_alloc_page_unlocked(void)
{
    for (size_t i = 0; i < total_memory_pages; i++) {
        size_t idx = (last_search_idx + i) % total_memory_pages;
        if (!bitmap_test(idx)) {
            bitmap_set(idx);
            free_page_count--;
            used_page_count++;
            last_search_idx = (idx + 1) % total_memory_pages;
            return (uintptr_t)(idx * PAGE_SIZE);
        }
    }
    return 0; /* Out of physical memory */
}

uintptr_t pmm_alloc_page(void)
{
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    uintptr_t ret = pmm_alloc_page_unlocked();
    spin_unlock_irqrestore(&pmm_lock, flags);
    return ret;
}

uintptr_t pmm_alloc_pages(size_t count)
{
    if (count == 0) {
        return 0;
    }

    uint64_t flags = spin_lock_irqsave(&pmm_lock);

    if (count == 1) {
        uintptr_t ret = pmm_alloc_page_unlocked();
        spin_unlock_irqrestore(&pmm_lock, flags);
        return ret;
    }

    size_t consecutive = 0;
    size_t start_idx = 0;

    for (size_t i = 0; i < total_memory_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) {
                start_idx = i;
            }
            consecutive++;
            if (consecutive == count) {
                for (size_t j = 0; j < count; j++) {
                    bitmap_set(start_idx + j);
                }
                free_page_count -= count;
                used_page_count += count;
                spin_unlock_irqrestore(&pmm_lock, flags);
                return (uintptr_t)(start_idx * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }

    spin_unlock_irqrestore(&pmm_lock, flags);
    return 0; /* Not enough contiguous physical memory */
}

static void pmm_free_page_unlocked(uintptr_t phys_addr)
{
    if (phys_addr == 0 || phys_addr % PAGE_SIZE != 0) {
        return;
    }
    size_t page_idx = phys_addr / PAGE_SIZE;
    if (page_idx >= total_memory_pages) {
        return;
    }

    if (bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        free_page_count++;
        used_page_count--;
    }
}

void pmm_free_page(uintptr_t phys_addr)
{
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    pmm_free_page_unlocked(phys_addr);
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_free_pages(uintptr_t phys_addr, size_t count)
{
    uint64_t flags = spin_lock_irqsave(&pmm_lock);
    for (size_t i = 0; i < count; i++) {
        pmm_free_page_unlocked(phys_addr + (i * PAGE_SIZE));
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
}

void pmm_get_stats(struct pmm_stats *stats)
{
    if (stats) {
        uint64_t flags = spin_lock_irqsave(&pmm_lock);
        stats->total_pages = total_memory_pages;
        stats->free_pages = free_page_count;
        stats->used_pages = used_page_count;
        stats->reserved_pages = total_memory_pages - free_page_count;
        spin_unlock_irqrestore(&pmm_lock, flags);
    }
}
