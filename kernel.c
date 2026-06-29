#include "io.h"
#include "serial.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"

extern volatile u64 ticks;

void __attribute__((section(".entry"))) kmain(void)
{
    puts("\x1b[2J\x1b[H");

    puts("[kernel] Entry at 0x100000\r\n");
    puts("[kernel] x86-64 long mode\r\n");

    idt_init();
    puts("[kernel] IDT loaded\r\n");

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    puts("[kernel] IRQs masked\r\n");

    pic_remap();
    puts("[kernel] PIC remapped\r\n");

    pit_init(100);
    puts("[kernel] PIT 100 Hz\r\n");

    outb(PIC1_DATA, 0xFC);
    puts("[kernel] IRQ0+IRQ1 unmasked\r\n");

    __asm__ volatile("sti");
    puts("[kernel] Interrupts enabled\r\n\r\n");

    pmm_init();

    paging_init();

    void *phys = pmm_alloc_page();
    if (phys) {
        puts("[kernel] Allocated page at 0x");
        puthex((u64)phys);
        puts("\r\n");

        u64 virt = 0x40000000;
        map_page(virt, (u64)phys, PAGE_PRESENT | PAGE_WRITABLE);
        puts("[kernel] Mapped 0x40000000 -> 0x");
        puthex((u64)phys);
        puts("\r\n");

        char *p = (char*)virt;
        p[0] = 'H'; p[1] = 'e'; p[2] = 'l'; p[3] = 'l';
        p[4] = 'o'; p[5] = '!'; p[6] = 0;
        puts("[kernel] Wrote string via virt: ");
        puts(p);
        puts("\r\n");

        unmap_page(virt);
        puts("[kernel] Unmapped 0x40000000\r\n");

        pmm_free_page(phys);
        puts("[kernel] Freed page\r\n");
    }

    puts("\r\n[kernel] Paging demo done. Timer ticks:\r\n");

    u64 last = 0;
    while (1) {
        __asm__ volatile("pause");
        if (ticks != last) {
            last = ticks;
            putc('.');
            if (last % 100 == 0) {
                puts("\r\n[kernel] tick: ");
                puthex(ticks);
                puts(" ");
            }
        }
    }
}
