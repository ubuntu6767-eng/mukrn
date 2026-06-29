#include "idt.h"
#include "serial.h"
#include "task.h"
#include "syscall.h"
#include "paging.h"

static idt_entry_t idt[IDT_SIZE] = {0};
static idtr_t idtr;
volatile u64 ticks = 0;
extern int num_tasks;
extern int current_task;
extern task_t tasks[];
static int irq_handlers[16];

static void idt_set_entry(int n, void *handler, u8 flags) {
    u64 addr = (u64)handler;
    idt[n].offset_0 = addr & 0xFFFF;
    idt[n].selector = 0x18;
    idt[n].ist = 0;
    idt[n].flags = flags;
    idt[n].offset_1 = (addr >> 16) & 0xFFFF;
    idt[n].offset_2 = (addr >> 32) & 0xFFFFFFFF;
    idt[n].reserved = 0;
}

static int schedule_next(void) {
    if (num_tasks <= 1) return current_task;
    for (int i = 1; i < num_tasks; i++) {
        int n = (current_task + i) % num_tasks;
        if (tasks[n].state == TASK_READY) {
            tasks[n].state = TASK_RUNNING;
            return n;
        }
    }
    return current_task;
}

static void save_frame(registers_t *r) {
    registers_t *dst = (registers_t*)tasks[current_task].stack_phys;
    for (int i = 0; i < sizeof(registers_t) / sizeof(u64); i++)
        ((u64*)dst)[i] = ((u64*)r)[i];
    tasks[current_task].rsp = (u64)dst;
}

static registers_t *schedule(registers_t *r) {
    if (current_task >= 0) {
        save_frame(r);
        tasks[current_task].state = TASK_READY;
    }
    int next = schedule_next();
    tasks[next].state = TASK_RUNNING;
    current_task = next;
    if (tasks[next].pml4_phys)
        paging_switch(tasks[next].pml4_phys);
    return (registers_t*)tasks[current_task].rsp;
}

registers_t *isr_handler(registers_t *r) {
    u64 n = r->int_no;

    if (n <= 31) {
        puts("\r\n[PANIC] Exception ");
        puthex(n);
        puts(" err: ");
        puthex(r->err_code);
        puts(" RIP: ");
        puthex(r->rip);
        puts(" RSP: ");
        puthex(r->rsp);
        puts(" RBP: ");
        puthex(r->rbp);
        if (n == 14) {
            u64 cr2;
            __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
            puts(" CR2: ");
            puthex(cr2);
            puts(" CR3: ");
            u64 cr3;
            __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
            puthex(cr3);
            u64 *pde = (u64*)(cr3);
            u64 pml4_i = (cr2 >> 39) & 0x1FF;
            u64 pdpt_i = (cr2 >> 30) & 0x1FF;
            u64 pdt_i  = (cr2 >> 21) & 0x1FF;
            u64 pt_i   = (cr2 >> 12) & 0x1FF;
            puts(" PML4["); puthex(pml4_i); puts("]");
            puthex(pde[pml4_i]);
            if (pde[pml4_i] & 1) {
                u64 *pdpt = (u64*)(pde[pml4_i] & ~0xFFF);
                puts(" PDPT["); puthex(pdpt_i); puts("]");
                puthex(pdpt[pdpt_i]);
                if (pdpt[pdpt_i] & 1) {
                    u64 *pdt = (u64*)(pdpt[pdpt_i] & ~0xFFF);
                    puts(" PDT["); puthex(pdt_i); puts("]");
                    puthex(pdt[pdt_i]);
                    if ((pdt[pdt_i] & 1) && !(pdt[pdt_i] & PAGE_PS)) {
                        u64 *pt = (u64*)(pdt[pdt_i] & ~0xFFF);
                        puts(" PT["); puthex(pt_i); puts("]");
                        puthex(pt[pt_i]);
                    }
                }
            }
        }
        puts("\r\n");
        __asm__ volatile("cli; hlt");
        for (;;);
    }

    if (n >= 32 && n <= 47) {
        if (n >= 40) outb(PIC2_COMMAND, PIC_EOI);
        outb(PIC1_COMMAND, PIC_EOI);

        if (n == 32) {
            ticks++;
            if (num_tasks > 1 && (r->cs & 3))
                return schedule(r);
        }

        if (n == 33) {
            int idx = irq_handlers[1];
            if (idx >= 0 && idx < MAX_TASKS && tasks[idx].state != TASK_EXITED) {
                u8 sc = inb(0x60);
                task_t *t = &tasks[idx];
                if (t->ipc_count < IPC_QUEUE_SIZE) {
                    ipc_msg_t *msg = &t->ipc_queue[t->ipc_head];
                    msg->sender_pid = 0;
                    msg->type = 0;
                    msg->length = 1;
                    msg->data[0] = sc;
                    t->ipc_head = (t->ipc_head + 1) % IPC_QUEUE_SIZE;
                    t->ipc_count++;
                    if (t->state == TASK_BLOCKED)
                        t->state = TASK_READY;
                }
            }
        }

        return r;
    }

    if (n == 0x80) {
        r->rax = syscall_handler(r->rax, r->rdi, r->rsi, r->rdx, r->rcx);
        if (num_tasks > 1) {
            save_frame(r);
            if (tasks[current_task].state != TASK_BLOCKED && tasks[current_task].state != TASK_EXITED)
                tasks[current_task].state = TASK_READY;
            int next = schedule_next();
            tasks[next].state = TASK_RUNNING;
            current_task = next;
            if (tasks[next].pml4_phys)
                paging_switch(tasks[next].pml4_phys);
            return (registers_t*)tasks[current_task].rsp;
        }
        return r;
    }

    return r;
}

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;

    for (int i = 0; i < 16; i++)
        irq_handlers[i] = -1;

    for (int i = 0; i < 48; i++)
        idt_set_entry(i, 0, 0x8E);

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
    idt_set_entry(0x80, isr128, 0xEF);

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

int sys_irq_register(u64 irq)
{
    if (irq >= 16) return -1;
    if (current_task < 0) return -1;
    irq_handlers[irq] = current_task;
    return 0;
}
