#ifndef _AURORA_MM_HEAP_H
#define _AURORA_MM_HEAP_H

#include <aurora/types.h>

/* Allocation flags matching Section 4.3 */
#define GFP_KERNEL 0x01
#define GFP_ATOMIC 0x02
#define GFP_ZERO   0x04

void kmalloc_init(void);

void *kmalloc(size_t size, uint32_t flags);
void *kzalloc(size_t size, uint32_t flags);
void kfree(void *ptr);
void *krealloc(void *ptr, size_t new_size, uint32_t flags);
void *kmalloc_aligned(size_t size, size_t alignment, uint32_t flags);

struct heap_stats {
    size_t total_allocated_bytes;
    size_t active_allocations;
    size_t slab_allocations;
    size_t large_allocations;
};

void heap_get_stats(struct heap_stats *stats);

#endif /* _AURORA_MM_HEAP_H */
