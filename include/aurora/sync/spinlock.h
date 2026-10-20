#ifndef _AURORA_SYNC_SPINLOCK_H
#define _AURORA_SYNC_SPINLOCK_H

#include <aurora/types.h>

typedef struct spinlock {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT ((spinlock_t){ .lock = 0 })

static inline void spinlock_init(spinlock_t *sl)
{
    sl->lock = 0;
}

static inline void spin_lock(spinlock_t *sl)
{
    while (__atomic_test_and_set(&sl->lock, __ATOMIC_ACQUIRE)) {
        while (sl->lock) {
            __asm__ volatile("pause" ::: "memory");
        }
    }
}

static inline bool spin_trylock(spinlock_t *sl)
{
    return !__atomic_test_and_set(&sl->lock, __ATOMIC_ACQUIRE);
}

static inline void spin_unlock(spinlock_t *sl)
{
    __atomic_clear(&sl->lock, __ATOMIC_RELEASE);
}

/* IRQ-safe spinlock operations */
static inline uint64_t spin_lock_irqsave(spinlock_t *sl)
{
    uint64_t rflags;
    __asm__ volatile(
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(rflags)
        :
        : "memory"
    );
    spin_lock(sl);
    return rflags;
}

static inline void spin_unlock_irqrestore(spinlock_t *sl, uint64_t rflags)
{
    spin_unlock(sl);
    if (rflags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

#endif /* _AURORA_SYNC_SPINLOCK_H */
