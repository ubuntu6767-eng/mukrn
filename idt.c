#include "idt.h"
#include "serial.h"

static idt_entry_t idt[IDT_SIZE] = {0};
static idtr_t idtr;

static void idt_set_entry(int n, void *handler, u8 flags) {
    u64 addr = (u64)handler;
    idt[n].offset_0 = addr & 0xFFFF;
    idt[n].selector = CODE64_SEG;
    idt[n].ist = 0;
    idt[n].flags = flags;
    idt[n].offset_1 = (addr >> 16) & 0xFFFF;
    idt[n].offset_2 = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

volatile u64 ticks = 0;

void isr_handler(registers_t *r) {
    u64 n = r->int_no;

    if (n <= 31) {
        if (n == 14) {
            u64 cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            puts("\r\n[PANIC] Page fault at 0x");
            puthex(cr2);
            puts(" (err: ");
            puthex(r->err_code);
            puts(")\r\n");
        } else {
            puts("\r\n[PANIC] Exception ");
            puthex(n);
            puts(" (err: ");
            puthex(r->err_code);
            puts(")\r\n");
        }
        __asm__ volatile("cli; hlt");
        for (;;);
    }

    if (n >= 32 && n <= 47) {
        if (n >= 40) outb(PIC2_COMMAND, PIC_EOI);
        outb(PIC1_COMMAND, PIC_EOI);
        if (n == 32) ticks++;
    }
}

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;

    idt_set_entry(0,  isr0,  0x8E);
    idt_set_entry(1,  isr1,  0x8E);
    idt_set_entry(2,  isr2,  0x8E);
    idt_set_entry(3,  isr3,  0x8E);
    idt_set_entry(4,  isr4,  0x8E);
    idt_set_entry(5,  isr5,  0x8E);
    idt_set_entry(6,  isr6,  0x8E);
    idt_set_entry(7,  isr7,  0x8E);
    idt_set_entry(8,  isr8,  0x8E);
    idt_set_entry(9,  isr9,  0x8E);
    idt_set_entry(10, isr10, 0x8E);
    idt_set_entry(11, isr11, 0x8E);
    idt_set_entry(12, isr12, 0x8E);
    idt_set_entry(13, isr13, 0x8E);
    idt_set_entry(14, isr14, 0x8E);
    idt_set_entry(15, isr15, 0x8E);
    idt_set_entry(16, isr16, 0x8E);
    idt_set_entry(17, isr17, 0x8E);
    idt_set_entry(18, isr18, 0x8E);
    idt_set_entry(19, isr19, 0x8E);
    idt_set_entry(20, isr20, 0x8E);
    idt_set_entry(21, isr21, 0x8E);
    idt_set_entry(22, isr22, 0x8E);
    idt_set_entry(23, isr23, 0x8E);
    idt_set_entry(24, isr24, 0x8E);
    idt_set_entry(25, isr25, 0x8E);
    idt_set_entry(26, isr26, 0x8E);
    idt_set_entry(27, isr27, 0x8E);
    idt_set_entry(28, isr28, 0x8E);
    idt_set_entry(29, isr29, 0x8E);
    idt_set_entry(30, isr30, 0x8E);
    idt_set_entry(31, isr31, 0x8E);
    idt_set_entry(32, isr32, 0x8E);
    idt_set_entry(33, isr33, 0x8E);
    idt_set_entry(34, isr34, 0x8E);
    idt_set_entry(35, isr35, 0x8E);
    idt_set_entry(36, isr36, 0x8E);
    idt_set_entry(37, isr37, 0x8E);
    idt_set_entry(38, isr38, 0x8E);
    idt_set_entry(39, isr39, 0x8E);
    idt_set_entry(40, isr40, 0x8E);
    idt_set_entry(41, isr41, 0x8E);
    idt_set_entry(42, isr42, 0x8E);
    idt_set_entry(43, isr43, 0x8E);
    idt_set_entry(44, isr44, 0x8E);
    idt_set_entry(45, isr45, 0x8E);
    idt_set_entry(46, isr46, 0x8E);
    idt_set_entry(47, isr47, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idtr));
}

void pic_remap(void) {
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

void pit_init(u32 frequency) {
    u32 divisor = 1193182 / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}
