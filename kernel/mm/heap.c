#include <aurora/mm/heap.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/printk.h>
#include <aurora/panic.h>
#include <aurora/string.h>

#define ALLOC_MAGIC 0xABCD1234
#define FREE_MAGIC  0xDEADBEEF
#define SLAB_LARGE  0xFFFFFFFF

#define NUM_SLABS 9

static const size_t slab_sizes[NUM_SLABS] = {
    16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

struct alloc_header {
    uint32_t magic;
    uint32_t slab_index;
    size_t   size;
};

struct slab_free_node {
    struct slab_free_node *next;
};

static struct slab_free_node *slab_heads[NUM_SLABS];

static struct heap_stats g_heap_stats;

void kmalloc_init(void)
{
    memset(slab_heads, 0, sizeof(slab_heads));
    memset(&g_heap_stats, 0, sizeof(g_heap_stats));

    printk("<6>[  0.000000] Kernel Heap: Slab allocator initialized (9 classes: 16B - 4096B)\n");
}

static int get_slab_index(size_t size)
{
    for (int i = 0; i < NUM_SLABS; i++) {
        if (size <= slab_sizes[i]) {
            return i;
        }
    }
    return -1;
}

void *kmalloc(size_t size, uint32_t flags)
{
    if (size == 0) {
        return NULL;
    }

    int slab_idx = get_slab_index(size);

    if (slab_idx != -1) {
        /* Allocate from Slab Cache */
        if (!slab_heads[slab_idx]) {
            /* Need more pages for this slab */
            size_t block_size = sizeof(struct alloc_header) + slab_sizes[slab_idx];
            size_t pages_per_slab = (block_size + PAGE_SIZE - 1) / PAGE_SIZE;
            if (pages_per_slab == 0) {
                pages_per_slab = 1;
            }
            size_t total_slab_bytes = pages_per_slab * PAGE_SIZE;
            size_t blocks_per_slab = total_slab_bytes / block_size;
            if (blocks_per_slab == 0) {
                blocks_per_slab = 1;
            }

            uintptr_t phys = (pages_per_slab == 1) ? pmm_alloc_page() : pmm_alloc_pages(pages_per_slab);
            if (!phys) {
                return NULL; /* OOM */
            }

            uint8_t *page = (uint8_t *)vmm_phys_to_virt(phys);

            for (size_t b = 0; b < blocks_per_slab; b++) {
                uint8_t *block = page + (b * block_size);
                struct slab_free_node *node = (struct slab_free_node *)block;
                node->next = slab_heads[slab_idx];
                slab_heads[slab_idx] = node;
            }
        }

        struct slab_free_node *node = slab_heads[slab_idx];
        slab_heads[slab_idx] = node->next;

        struct alloc_header *header = (struct alloc_header *)node;
        header->magic = ALLOC_MAGIC;
        header->slab_index = (uint32_t)slab_idx;
        header->size = size;

        g_heap_stats.total_allocated_bytes += slab_sizes[slab_idx];
        g_heap_stats.active_allocations++;
        g_heap_stats.slab_allocations++;

        void *user_ptr = (void *)(header + 1);

        if (flags & GFP_ZERO) {
            memset(user_ptr, 0, size);
        }

        return user_ptr;
    } else {
        /* Large allocation: allocate whole pages directly from PMM */
        size_t total_needed = sizeof(struct alloc_header) + size;
        size_t pages = (PAGE_ALIGN(total_needed)) / PAGE_SIZE;

        uintptr_t phys = pmm_alloc_pages(pages);
        if (!phys) {
            return NULL; /* OOM */
        }

        struct alloc_header *header = (struct alloc_header *)vmm_phys_to_virt(phys);
        header->magic = ALLOC_MAGIC;
        header->slab_index = SLAB_LARGE;
        header->size = size;

        g_heap_stats.total_allocated_bytes += (pages * PAGE_SIZE);
        g_heap_stats.active_allocations++;
        g_heap_stats.large_allocations++;

        void *user_ptr = (void *)(header + 1);

        if (flags & GFP_ZERO) {
            memset(user_ptr, 0, size);
        }

        return user_ptr;
    }
}

void *kzalloc(size_t size, uint32_t flags)
{
    return kmalloc(size, flags | GFP_ZERO);
}

void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }

    struct alloc_header *header = ((struct alloc_header *)ptr) - 1;

    if (header->magic != ALLOC_MAGIC) {
        printk("<0>[HEAP CORRUPTION] Invalid magic 0x%x at %p in kfree\n", header->magic, ptr);
        panic("Heap corruption or double free detected!");
    }

    header->magic = FREE_MAGIC;
    g_heap_stats.active_allocations--;

    if (header->slab_index < NUM_SLABS) {
        /* Return block to slab free list */
        int slab_idx = (int)header->slab_index;
        g_heap_stats.total_allocated_bytes -= slab_sizes[slab_idx];

        struct slab_free_node *node = (struct slab_free_node *)header;
        node->next = slab_heads[slab_idx];
        slab_heads[slab_idx] = node;
    } else if (header->slab_index == SLAB_LARGE) {
        /* Return pages to PMM */
        size_t total_needed = sizeof(struct alloc_header) + header->size;
        size_t pages = (PAGE_ALIGN(total_needed)) / PAGE_SIZE;
        g_heap_stats.total_allocated_bytes -= (pages * PAGE_SIZE);

        uintptr_t phys = vmm_virt_to_phys_direct(header);
        pmm_free_pages(phys, pages);
    }
}

void *krealloc(void *ptr, size_t new_size, uint32_t flags)
{
    if (!ptr) {
        return kmalloc(new_size, flags);
    }
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct alloc_header *header = ((struct alloc_header *)ptr) - 1;
    if (header->magic != ALLOC_MAGIC) {
        panic("Invalid pointer passed to krealloc");
    }

    if (header->size >= new_size) {
        return ptr; /* Already large enough */
    }

    void *new_ptr = kmalloc(new_size, flags);
    if (!new_ptr) {
        return NULL;
    }

    memcpy(new_ptr, ptr, header->size);
    kfree(ptr);
    return new_ptr;
}

void *kmalloc_aligned(size_t size, size_t alignment, uint32_t flags)
{
    if (alignment <= sizeof(void *)) {
        return kmalloc(size, flags);
    }

    /* Allocate extra space to fit alignment and offset storage */
    size_t total = size + alignment + sizeof(void *);
    void *raw = kmalloc(total, flags);
    if (!raw) {
        return NULL;
    }

    uintptr_t raw_addr = (uintptr_t)raw + sizeof(void *);
    uintptr_t aligned_addr = (raw_addr + alignment - 1) & ~(alignment - 1);

    /* Store original raw pointer right before aligned address */
    void **store = (void **)aligned_addr - 1;
    *store = raw;

    return (void *)aligned_addr;
}

void heap_get_stats(struct heap_stats *stats)
{
    if (stats) {
        *stats = g_heap_stats;
    }
}
