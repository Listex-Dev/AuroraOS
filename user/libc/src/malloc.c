#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "syscall.h"

struct chunk {
    size_t size; /* total size, lowest bit = 1 if allocated */
    struct chunk *next;
    struct chunk *prev;
};

#define CHUNK_HEADER_SIZE ((sizeof(struct chunk) + 15) & ~15)
#define CHUNK_IN_USE      ((size_t)1)
#define CHUNK_SIZE_MASK   (~CHUNK_IN_USE)

static struct chunk *free_list = NULL;

void *sbrk(intptr_t increment)
{
    long cur_brk = __syscall1(SYS_brk, 0);
    if (increment == 0) {
        return (void *)cur_brk;
    }
    long new_brk = __syscall1(SYS_brk, cur_brk + increment);
    if (new_brk == cur_brk && increment > 0) {
        return (void *)-1;
    }
    return (void *)cur_brk;
}

int brk(void *addr)
{
    long ret = __syscall1(SYS_brk, (long)addr);
    return (ret == (long)addr) ? 0 : -1;
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    /* 16-byte alignment */
    size_t needed = (size + CHUNK_HEADER_SIZE + 15) & ~15;
    if (needed < 32) needed = 32;

    /* Search free list for best/first fit */
    struct chunk *curr = free_list;
    while (curr) {
        size_t csize = curr->size & CHUNK_SIZE_MASK;
        if (csize >= needed) {
            /* If remainder is large enough, split chunk */
            if (csize >= needed + 32) {
                struct chunk *split = (struct chunk *)((char *)curr + needed);
                split->size = (csize - needed);
                split->next = curr->next;
                split->prev = curr->prev;
                if (curr->prev) curr->prev->next = split;
                else free_list = split;
                if (curr->next) curr->next->prev = split;

                curr->size = needed | CHUNK_IN_USE;
            } else {
                /* Remove curr from free list */
                if (curr->prev) curr->prev->next = curr->next;
                else free_list = curr->next;
                if (curr->next) curr->next->prev = curr->prev;

                curr->size = csize | CHUNK_IN_USE;
            }
            return (void *)((char *)curr + CHUNK_HEADER_SIZE);
        }
        curr = curr->next;
    }

    /* Allocate from kernel via sbrk */
    size_t alloc_size = (needed < 65536) ? 65536 : needed;
    void *mem = sbrk(alloc_size);
    if (mem == (void *)-1) {
        return NULL;
    }

    struct chunk *new_chunk = (struct chunk *)mem;
    if (alloc_size >= needed + 32) {
        struct chunk *rem = (struct chunk *)((char *)new_chunk + needed);
        rem->size = (alloc_size - needed);
        rem->next = free_list;
        rem->prev = NULL;
        if (free_list) free_list->prev = rem;
        free_list = rem;

        new_chunk->size = needed | CHUNK_IN_USE;
    } else {
        new_chunk->size = alloc_size | CHUNK_IN_USE;
    }

    return (void *)((char *)new_chunk + CHUNK_HEADER_SIZE);
}

void free(void *ptr)
{
    if (!ptr) return;

    struct chunk *c = (struct chunk *)((char *)ptr - CHUNK_HEADER_SIZE);
    c->size &= CHUNK_SIZE_MASK; /* mark as free */

    /* Add to free list */
    c->next = free_list;
    c->prev = NULL;
    if (free_list) free_list->prev = c;
    free_list = c;
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size)
{
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    struct chunk *c = (struct chunk *)((char *)ptr - CHUNK_HEADER_SIZE);
    size_t old_size = (c->size & CHUNK_SIZE_MASK) - CHUNK_HEADER_SIZE;
    if (old_size >= size) {
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size);
        free(ptr);
    }
    return new_ptr;
}
