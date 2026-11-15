// Host-only bounded allocator for LVGL's CUSTOM allocator API. Like Zephyr's
// 8192-byte display pool, this owns a fixed arena and can fail. It does not call
// libc malloc. Its 16-byte alignment/header is conservative on a 64-bit host.
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <lvgl.h>
#include "bounded_heap.h"

struct block {
    uint32_t size_and_used;
    uint32_t requested;
    uint64_t reserved;
};
static union {
    max_align_t alignment;
    uint8_t bytes[REEL_TEST_HEAP_BYTES];
} arena;
static size_t used, peak, failures, largest_failure;

static struct block *at(size_t offset) { return (struct block *)(arena.bytes + offset); }
static size_t block_size(struct block *block) { return block->size_and_used & ~1u; }

void lv_mem_init(void) {
    memset(&arena, 0, sizeof(arena));
    at(0)->size_and_used = REEL_TEST_HEAP_BYTES;
    used = peak = failures = largest_failure = 0;
}

void lv_mem_deinit(void) { }
lv_mem_pool_t lv_mem_add_pool(void *memory, size_t bytes) {
    (void)memory; (void)bytes;
    return NULL; // The test must not silently expand its display heap.
}
void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }

void *lv_malloc_core(size_t size) {
    if (size <= REEL_TEST_HEAP_BYTES - sizeof(struct block)) {
        size_t needed = (size + sizeof(struct block) + 15) & ~(size_t)15;
        for (size_t offset = 0; offset < REEL_TEST_HEAP_BYTES;) {
            struct block *block = at(offset);
            size_t available = block_size(block);
            assert(available >= sizeof(struct block));
            if (!(block->size_and_used & 1) && available >= needed) {
                size_t remainder = available - needed;
                if (remainder >= 2 * sizeof(struct block)) {
                    at(offset + needed)->size_and_used = (uint32_t)remainder;
                } else {
                    needed = available;
                }
                block->size_and_used = (uint32_t)needed | 1;
                block->requested = (uint32_t)size;
                used += needed;
                if (used > peak) peak = used;
                return (uint8_t *)block + sizeof(*block);
            }
            offset += available;
        }
    }
    failures++;
    if (size > largest_failure) largest_failure = size;
    return NULL;
}

void lv_free_core(void *pointer) {
    if (!pointer) return;
    struct block *block = (struct block *)((uint8_t *)pointer - sizeof(struct block));
    assert((uint8_t *)block >= arena.bytes && (uint8_t *)block < arena.bytes + sizeof(arena));
    assert(block->size_and_used & 1);
    used -= block_size(block);
    block->size_and_used &= ~1u;
    block->requested = 0;
    for (size_t offset = 0; offset < REEL_TEST_HEAP_BYTES;) {
        struct block *current = at(offset);
        size_t next = offset + block_size(current);
        if (!(current->size_and_used & 1) && next < REEL_TEST_HEAP_BYTES &&
            !(at(next)->size_and_used & 1)) {
            current->size_and_used += (uint32_t)block_size(at(next));
        } else {
            offset = next;
        }
    }
}

void *lv_realloc_core(void *pointer, size_t size) {
    if (!pointer) return lv_malloc_core(size);
    if (!size) { lv_free_core(pointer); return NULL; }
    struct block *old = (struct block *)((uint8_t *)pointer - sizeof(struct block));
    assert(old->size_and_used & 1);
    if (block_size(old) - sizeof(*old) >= size) {
        old->requested = (uint32_t)size;
        return pointer;
    }
    void *replacement = lv_malloc_core(size);
    if (!replacement) return NULL;
    memcpy(replacement, pointer, old->requested);
    lv_free_core(pointer);
    return replacement;
}

void lv_mem_monitor_core(lv_mem_monitor_t *monitor) {
    memset(monitor, 0, sizeof(*monitor));
    monitor->total_size = REEL_TEST_HEAP_BYTES;
    monitor->max_used = peak;
    for (size_t offset = 0; offset < REEL_TEST_HEAP_BYTES;) {
        struct block *block = at(offset);
        size_t available = block_size(block);
        if (block->size_and_used & 1) {
            monitor->used_cnt++;
        } else {
            size_t free = available - sizeof(*block);
            monitor->free_cnt++;
            monitor->free_size += free;
            if (free > monitor->free_biggest_size) monitor->free_biggest_size = free;
        }
        offset += available;
    }
    monitor->used_pct = (uint8_t)(used * 100 / REEL_TEST_HEAP_BYTES);
    monitor->frag_pct = monitor->free_size ?
        (uint8_t)(100 - monitor->free_biggest_size * 100 / monitor->free_size) : 0;
}

lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
size_t reel_test_heap_peak(void) { return peak; }
size_t reel_test_heap_failures(void) { return failures; }
size_t reel_test_heap_largest_failure(void) { return largest_failure; }
