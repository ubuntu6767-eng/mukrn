#include "task.h"
#include "pmm.h"
#include "paging.h"
#include "serial.h"
#include "idt.h"

extern unsigned char _binary_keyboard_driver_bin_start[];
extern unsigned char _binary_keyboard_driver_bin_end[];
extern unsigned char _binary_command_bin_start[];
extern unsigned char _binary_command_bin_end[];
extern unsigned char _binary_shell_bin_start[];
extern unsigned char _binary_shell_bin_end[];

embed_prog_t embedded[EMBED_COUNT] = {
    { _binary_keyboard_driver_bin_start, _binary_keyboard_driver_bin_end, 0x400000 },
    { _binary_command_bin_start,         _binary_command_bin_end,         0x500000 },
    { _binary_shell_bin_start,           _binary_shell_bin_end,           0x600000 },
};

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

void ipc_init_task(task_t *t)
{
    t->ipc_head = 0;
    t->ipc_tail = 0;
    t->ipc_count = 0;
}

void create_process(void *binary, u64 size, u64 load_addr)
{
    if (num_tasks >= MAX_TASKS) return;

    u64 code_pages = (size + 4095) / 4096;
    if (code_pages > 16) {
        puts("[kernel] Process too large\r\n");
        return;
    }

    void *kstack = pmm_alloc_page();
    if (!kstack) {
        puts("[kernel] No memory for process\r\n");
        return;
    }

    u64 new_pml4 = paging_clone_kernel();
    if (!new_pml4) return;

    u64 old_root = paging_root;
    paging_switch(new_pml4);

    for (u64 i = 0; i < code_pages; i++) {
        void *cp = pmm_alloc_page();
        if (!cp) return;
        u64 virt = load_addr + i * 4096;
        map_page(virt, (u64)cp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    for (int i = 0; i < 4; i++) {
        void *sp = pmm_alloc_page();
        if (!sp) return;
        u64 v = load_addr + 0x100000 - (i + 1) * 4096;
        map_page(v, (u64)sp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    u8 *src = (u8*)binary;
    for (u64 i = 0; i < size; i++)
        ((u8*)load_addr)[i] = src[i];

    paging_switch(old_root);

    u64 user_stack_top = load_addr + 0x100000;
    u64 *sp = (u64*)((u64)kstack + sizeof(registers_t));
    *--sp = 0x23;
    *--sp = user_stack_top;
    *--sp = 0x202;
    *--sp = 0x2B;
    *--sp = load_addr;
    *--sp = 0;
    *--sp = 0;
    for (int i = 0; i < 15; i++) *--sp = 0;

    task_t *t = &tasks[num_tasks];
    t->rsp = (u64)kstack;
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->stack_phys = (u64)kstack;
    t->user_stack_phys = 0;
    t->pml4_phys = new_pml4;
    t->load_addr = load_addr;
    t->code_pages = code_pages;
    t->wait_idx = -1;
    ipc_init_task(t);
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
    u64 pml4 = tasks[0].pml4_phys;
    if (!pml4) pml4 = 0x1000;
    __asm__ volatile(
        ".intel_syntax noprefix\n"
        "mov cr3, %1\n"
        "mov rsp, %0\n"
        "pop r15\n pop r14\n pop r13\n pop r12\n"
        "pop r11\n pop r10\n pop r9\n  pop r8\n"
        "pop rbp\n pop rdi\n pop rsi\n pop rdx\n"
        "pop rcx\n pop rbx\n pop rax\n"
        "add rsp, 16\n"
        "iretq\n"
        ".att_syntax noprefix\n"
        : : "r"(rsp), "r"(pml4) : "memory"
    );
}

u64 sys_getpid(void)
{
    if (current_task < 0) return 0;
    return tasks[current_task].pid;
}

int sys_send(u64 target_pid, u64 type, const u8 *data, u64 len)
{
    int target = -1;
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].pid == target_pid && tasks[i].state != TASK_EXITED) {
            target = i;
            break;
        }
    }
    if (target < 0) return -1;

    task_t *t = &tasks[target];
    if (t->ipc_count >= IPC_QUEUE_SIZE) return -1;

    ipc_msg_t *msg = &t->ipc_queue[t->ipc_head];
    msg->sender_pid = current_task < 0 ? 0 : tasks[current_task].pid;
    msg->type = type;
    msg->length = len > IPC_MAX_DATA ? IPC_MAX_DATA : len;
    for (u64 i = 0; i < msg->length; i++)
        msg->data[i] = data[i];

    t->ipc_head = (t->ipc_head + 1) % IPC_QUEUE_SIZE;
    t->ipc_count++;

    if (t->state == TASK_BLOCKED)
        t->state = TASK_READY;

    return 0;
}

int sys_recv(ipc_msg_t *msg)
{
    if (current_task < 0) return -1;

    task_t *t = &tasks[current_task];

    if (t->ipc_count == 0)
        return -1;

    ipc_msg_t *src = &t->ipc_queue[t->ipc_tail];
    msg->sender_pid = src->sender_pid;
    msg->type = src->type;
    msg->length = src->length;
    for (u64 i = 0; i < src->length; i++)
        msg->data[i] = src->data[i];

    t->ipc_tail = (t->ipc_tail + 1) % IPC_QUEUE_SIZE;
    t->ipc_count--;

    return 0;
}

int sys_spawn(u64 idx)
{
    if (idx >= EMBED_COUNT) return -1;
    embed_prog_t *p = &embedded[idx];
    u64 size = (u64)p->end - (u64)p->start;
    if (size == 0) return -1;
    int prev = num_tasks;
    create_process((void*)p->start, size, p->load_addr);
    if (num_tasks == prev) return -1;
    return tasks[prev].pid;
}

void sys_exit(void)
{
    if (current_task < 0) return;
    task_t *t = &tasks[current_task];

    if (t->wait_idx >= 0)
        tasks[t->wait_idx].state = TASK_READY;

    t->state = TASK_EXITED;

    puts("[kernel] Process #");
    puthex(t->pid);
    puts(" exited\r\n");
}

int sys_wait(u64 pid)
{
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].pid == pid) {
            if (tasks[i].state == TASK_EXITED) return 0;
            tasks[i].wait_idx = current_task;
            tasks[current_task].state = TASK_BLOCKED;
            return 0;
        }
    }
    return -1;
}
