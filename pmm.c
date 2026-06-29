#include "pmm.h"
#include "serial.h"

#define E820_BASE 0x4000

static void *free_stack = 0;

void pmm_init(void)
{
    u32 count = *(u32*)E820_BASE;
    e820_entry_t *entries = (e820_entry_t*)(E820_BASE + 4);

    puts("[kernel] E820 entries: ");
    puthex(count);
    puts("\r\n");

    for (u32 i = 0; i < count; i++) {
        e820_entry_t *e = &entries[i];
        if (e->type != 1) continue;

        u64 start = e->base;
        u64 end = e->base + e->len;

        if (start < 0x100000) start = 0x100000;
        if (start >= end) continue;

        start = (start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        end = end & ~(PAGE_SIZE - 1);

        for (u64 addr = start; addr < end; addr += PAGE_SIZE) {
            if (addr >= 0x1000 && addr < 0x4000) continue;
            if (addr >= 0x7C00 && addr < 0x8600) continue;
            if (addr >= 0x100000 && addr < 0x102000) continue;

            *(u64*)addr = (u64)free_stack;
            free_stack = (void*)addr;
        }
    }

    puts("[kernel] PMM ready\r\n");
}

void *pmm_alloc_page(void)
{
    if (!free_stack) return 0;
    void *page = free_stack;
    free_stack = (void*)*(u64*)free_stack;
    u64 *p = (u64*)page;
    for (int i = 0; i < PAGE_SIZE / 8; i++) p[i] = 0;
    return page;
}

void pmm_free_page(void *addr)
{
    *(u64*)addr = (u64)free_stack;
    free_stack = addr;
}
