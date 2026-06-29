#include "task.h"
#include "pmm.h"
#include "paging.h"
#include "serial.h"

task_t tasks[MAX_TASKS];
int current_task = -1;
int num_tasks = 0;
static u32 next_pid = 1;

void task_init(void)
{
    puts("[kernel] Task system init\r\n");
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i].state = TASK_EXITED;
}

void create_process(void *binary, u64 size)
{
    if (num_tasks >= MAX_TASKS) return;

    u64 code_pages = (size + 4095) / 4096;
    if (code_pages > 16) {
        puts("[kernel] Process too large\r\n");
        return;
    }

    void *kstack = pmm_alloc_page();
    void *ustack = pmm_alloc_page();
    if (!kstack || !ustack) {
        puts("[kernel] No memory for process\r\n");
        return;
    }

    for (u64 i = 0; i < code_pages; i++) {
        void *cp = pmm_alloc_page();
        if (!cp) {
            puts("[kernel] No code page\r\n");
            return;
        }
        u64 virt = USER_CODE_ADDR + i * 4096;
        map_page(virt, (u64)cp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    map_page(USER_STACK_TOP - 4096, (u64)ustack,
             PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    u8 *src = (u8*)binary;
    for (u64 i = 0; i < size; i++)
        ((u8*)USER_CODE_ADDR)[i] = src[i];

    u64 *sp = (u64*)((u64)kstack + 4096);
    *--sp = 0x23;
    *--sp = USER_STACK_TOP;
    *--sp = 0x202;
    *--sp = 0x2B;
    *--sp = USER_CODE_ADDR;
    *--sp = 0;
    *--sp = 0;
    for (int i = 0; i < 15; i++) *--sp = 0;

    task_t *t = &tasks[num_tasks];
    t->rsp = (u64)sp;
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->stack_phys = (u64)kstack;
    t->user_stack_phys = (u64)ustack;
    num_tasks++;

    puts("[kernel] Process #");
    puthex(t->pid);
    puts(" created\r\n");
}

void scheduler_start(void)
{
    if (num_tasks == 0) return;
    current_task = 0;
    tasks[0].state = TASK_RUNNING;
    puts("[kernel] Scheduler started\r\n");

    u64 rsp = tasks[0].rsp;
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov rsp, %0\n"
        "pop r15\n pop r14\n pop r13\n pop r12\n"
        "pop r11\n pop r10\n pop r9\n  pop r8\n"
        "pop rbp\n pop rdi\n pop rsi\n pop rdx\n"
        "pop rcx\n pop rbx\n pop rax\n"
        "add rsp, 16\n"
        "iretq\n"
        ".att_syntax noprefix\n"
        : : "r"(rsp) : "memory"
    );
}
