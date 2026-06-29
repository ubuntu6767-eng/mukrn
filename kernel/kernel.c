#include "io.h"
#include "serial.h"
#include "idt.h"
#include "pmm.h"
#include "paging.h"
#include "gdt.h"
#include "task.h"
#include "syscall.h"

extern unsigned char _binary_shell_bin_start[];
extern unsigned char _binary_shell_bin_end[];
extern unsigned char _binary_command_bin_start[];
extern unsigned char _binary_command_bin_end[];
extern unsigned char _binary_keyboard_driver_bin_start[];
extern unsigned char _binary_keyboard_driver_bin_end[];
// also declared in task.c for embed table

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
    gdt_setup_tss();

    u64 kbd_size = (u64)_binary_keyboard_driver_bin_end - (u64)_binary_keyboard_driver_bin_start;
    puts("[kernel] Keyboard driver size: ");
    puthex(kbd_size);
    puts(" bytes\r\n");
    create_process(_binary_keyboard_driver_bin_start, kbd_size, 0x400000);
    puts("[kernel] kbd0.pml4="); puthex(tasks[0].pml4_phys);
    puts(" kbd0.pid="); puthex(tasks[0].pid); putc('\n');

    u64 shell_size = (u64)_binary_shell_bin_end - (u64)_binary_shell_bin_start;
    puts("[kernel] Shell size: ");
    puthex(shell_size);
    puts(" bytes\r\n");
    create_process(_binary_shell_bin_start, shell_size, 0x600000);
    puts("[kernel] sh1.pml4="); puthex(tasks[1].pml4_phys);
    puts(" sh1.pid="); puthex(tasks[1].pid); putc('\n');

    u64 cmd_size = (u64)_binary_command_bin_end - (u64)_binary_command_bin_start;
    puts("[kernel] Command size: ");
    puthex(cmd_size);
    puts(" bytes\r\n");
    create_process(_binary_command_bin_start, cmd_size, 0x500000);
    puts("[kernel] cmd0.pml4="); puthex(tasks[2].pml4_phys);
    puts(" cmd0.pid="); puthex(tasks[2].pid); putc('\n');

    puts("[kernel] Starting scheduler...\r\n");
    scheduler_start();

    for (;;) __asm__ volatile("hlt");
}
