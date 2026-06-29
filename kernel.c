#include "io.h"
#include "serial.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "task.h"
#include "keyboard.h"
#include "syscall.h"

extern unsigned char _binary_shell_bin_start[];
extern unsigned char _binary_shell_bin_end[];

void __attribute__((section(".entry"))) kmain(void)
{
    puts("\x1b[2J\x1b[H");
    puts("[kernel] x86-64 long mode\r\n");

    idt_init();
    puts("[kernel] IDT loaded\r\n");

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    pic_remap();
    puts("[kernel] PIC remapped\r\n");

    pit_init(100);
    puts("[kernel] PIT 100 Hz\r\n");

    outb(PIC1_DATA, 0xFC);

    __asm__ volatile("sti");
    puts("[kernel] Interrupts enabled\r\n");

    pmm_init();
    paging_init();
    task_init();
    keyboard_init();
    gdt_setup_tss();

    u64 shell_size = (u64)_binary_shell_bin_end - (u64)_binary_shell_bin_start;
    puts("[kernel] Shell size: ");
    puthex(shell_size);
    puts(" bytes\r\n");

    create_process(_binary_shell_bin_start, shell_size);

    puts("[kernel] Starting shell...\r\n");
    scheduler_start();

    for (;;) __asm__ volatile("hlt");
}
