#include "gdt.h"
#include "pmm.h"
#include "serial.h"

#define TSS_PHYS 0x8F00

typedef struct {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed)) tss_t;

typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdtr_t;

void gdt_setup_tss(void)
{
    void *stack = pmm_alloc_page();
    if (!stack) {
        puts("[kernel] ERROR: TSS stack alloc failed\r\n");
        return;
    }

    tss_t *tss = (tss_t*)TSS_PHYS;
    for (int i = 0; i < (int)sizeof(tss_t) / 4; i++)
        ((u32*)tss)[i] = 0;
    tss->rsp0 = (u64)stack + 4096;
    tss->iomap_base = sizeof(tss_t);

    gdtr_t gdtr;
    __asm__ volatile("sgdt %0" : "=m"(gdtr));

    u64 *desc = (u64*)(gdtr.base + 0x30);
    u64 tss_addr = TSS_PHYS;
    u64 low = 0;
    low |= 0x67;
    low |= (tss_addr & 0xFFFF) << 16;
    low |= ((tss_addr >> 16) & 0xFF) << 32;
    low |= 0x89ULL << 40;
    low |= ((tss_addr >> 24) & 0xFF) << 56;
    desc[0] = low;
    desc[1] = (tss_addr >> 32) << 32;

    __asm__ volatile("lgdt %0" : : "m"(gdtr));
    __asm__ volatile(".intel_syntax noprefix; mov ax, 0x30; ltr ax; .att_syntax noprefix");
    puts("[kernel] TSS set up\r\n");
}
