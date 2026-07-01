#include "io.h"
#include "serial.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "task.h"
#include "syscall.h"

extern unsigned char _binary_init_elf_start[];
extern unsigned char _binary_init_elf_end[];

void __attribute__((section(".entry"))) kmain(void)
{
    puts("[kernel] x86-64 long mode\r\n");

    idt_init();
    puts("[kernel] IDT loaded\r\n");

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    pic_remap();
    puts("[kernel] PIC remapped\r\n");

    pit_init(500);
    puts("[kernel] PIT 500 Hz\r\n");

    outb(PIC1_DATA, 0xFC);

    __asm__ volatile("sti");
    puts("[kernel] Interrupts enabled\r\n");

    pmm_init();
    paging_init();
    task_init();
    gdt_setup_tss();

    u64 init_size = (u64)_binary_init_elf_end - (u64)_binary_init_elf_start;
    task_create((void*)_binary_init_elf_start, init_size, 0x400000, 1);
    puts("[kernel] Starting scheduler...\r\n");
    scheduler_start();

    for (;;) __asm__ volatile("hlt");
}
