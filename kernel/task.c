#include "task.h"
#include "pmm.h"
#include "paging.h"
#include "serial.h"
#include "idt.h"
#include "elf.h"

extern unsigned char _binary_init_elf_start[];
extern unsigned char _binary_init_elf_end[];

embed_prog_t embedded[EMBED_COUNT] = {
    { _binary_init_elf_start,             _binary_init_elf_end,             0x400000, 1 },
};

task_t tasks[MAX_TASKS];
int current_task = -1;
int num_tasks = 0;
static u32 next_pid = 8;

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

static int create_elf(void *binary, u64 size, u64 want_pid);

int task_create(void *binary, u64 size, u64 load_addr, u64 want_pid)
{
    if (size >= 4 && *(u32*)binary == ELF_MAGIC)
        return create_elf(binary, size, want_pid);
    if (num_tasks >= MAX_TASKS) return -1;

    u64 code_pages = (size + 4095) / 4096;
    if (code_pages > 16) {
        puts("[kernel] Process too large\r\n");
        return -1;
    }

    void *kstack = pmm_alloc_page();
    if (!kstack) {
        puts("[kernel] No memory for process\r\n");
        return -1;
    }

    if (want_pid > 0) {
        for (int i = 0; i < num_tasks; i++)
            if (tasks[i].pid == want_pid && tasks[i].state != TASK_EXITED) return -1;
    }

    u64 new_pml4 = paging_clone_kernel();
    if (!new_pml4) return -1;

    u64 old_root = paging_root;
    paging_switch(new_pml4);

    for (u64 i = 0; i < code_pages; i++) {
        void *cp = pmm_alloc_page();
        if (!cp) return -1;
        u64 virt = load_addr + i * 4096;
        map_page(virt, (u64)cp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    for (int i = 0; i < 4; i++) {
        void *sp = pmm_alloc_page();
        if (!sp) return -1;
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
    t->pid = want_pid > 0 ? want_pid : next_pid++;
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

    return t->pid;
}

void create_process(void *binary, u64 size, u64 load_addr)
{
    task_create(binary, size, load_addr, 0);
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

static int create_elf(void *binary, u64 size, u64 want_pid)
{
    if (size < sizeof(elf64_hdr_t)) return -1;
    elf64_hdr_t *hdr = (elf64_hdr_t*)binary;
    if (*(u32*)hdr->ident != ELF_MAGIC) return -1;

    u64 max_vaddr = 0;
    for (u16 i = 0; i < hdr->phnum; i++) {
        elf64_phdr_t *ph = (elf64_phdr_t*)((u8*)binary + hdr->phoff + i * hdr->phentsize);
        if (ph->type != PT_LOAD) continue;
        u64 end = ph->vaddr + ph->memsz;
        if (end > max_vaddr) max_vaddr = end;
    }
    max_vaddr = (max_vaddr + 4095) & ~4095ULL;

    if (num_tasks >= MAX_TASKS) return -1;
    void *kstack = pmm_alloc_page();
    if (!kstack) return -1;
    int reuse = -1;
    if (want_pid > 0) {
        for (int i = 0; i < num_tasks; i++) {
            if (tasks[i].pid == want_pid) {
                if (tasks[i].state != TASK_EXITED) return -1;
                reuse = i;
            }
        }
    }
    u64 new_pml4 = paging_clone_kernel();
    if (!new_pml4) return -1;

    u64 old_root = paging_root;
    __asm__ volatile("cli");
    paging_switch(new_pml4);

    for (u16 i = 0; i < hdr->phnum; i++) {
        elf64_phdr_t *ph = (elf64_phdr_t*)((u8*)binary + hdr->phoff + i * hdr->phentsize);
        if (ph->type != PT_LOAD) continue;
        if (ph->vaddr < 0x1000) {
            puts("[kernel] BAD phdr vaddr="); puthex(ph->vaddr);
            puts(" type="); puthex(ph->type);
            puts(" filesz="); puthex(ph->filesz);
            puts(" offset="); puthex(ph->offset);
            puts("\r\n");
        }
        u64 vstart = ph->vaddr & ~0xFFFULL;
        u64 vend = (ph->vaddr + ph->memsz + 4095) & ~0xFFFULL;
        for (u64 v = vstart; v < vend; v += 4096) {
            void *page = pmm_alloc_page();
            if (!page) { paging_switch(old_root); __asm__ volatile("sti"); return -1; }
            u64 pg = PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
            if (!(ph->flags & 2)) pg &= ~(u64)PAGE_WRITABLE;
            map_page(v, (u64)page, pg);
        }
        u8 *src = (u8*)binary + ph->offset;
        for (u64 j = 0; j < ph->filesz; j++)
            ((u8*)ph->vaddr)[j] = src[j];
    }

    u64 load_addr = 0x400000;
    u64 user_stack_top = max_vaddr + 0x20000;
    for (int i = 0; i < 4; i++) {
        void *sp = pmm_alloc_page();
        if (!sp) { paging_switch(old_root); __asm__ volatile("sti"); return -1; }
        u64 v = user_stack_top - (i + 1) * 4096;
        map_page(v, (u64)sp, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    paging_switch(old_root);
    __asm__ volatile("sti");

    u64 *sp = (u64*)((u64)kstack + sizeof(registers_t));
    *--sp = 0x23;
    *--sp = user_stack_top;
    *--sp = 0x202;
    *--sp = 0x2B;
    *--sp = hdr->entry;
    *--sp = 0;
    *--sp = 0;
    for (int i = 0; i < 15; i++) *--sp = 0;

    int ti = reuse >= 0 ? reuse : num_tasks;
    task_t *t = &tasks[ti];
    t->rsp = (u64)kstack;
    t->pid = want_pid > 0 ? want_pid : next_pid++;
    t->state = TASK_READY;
    t->stack_phys = (u64)kstack;
    t->user_stack_phys = 0;
    t->pml4_phys = new_pml4;
    t->load_addr = load_addr;
    t->code_pages = (max_vaddr - 0x400000 + 4095) / 4096;
    t->wait_idx = -1;
    ipc_init_task(t);
    if (reuse < 0) num_tasks++;

    puts("[kernel] ELF process #");
    puthex(t->pid);
    puts(" created\r\n");
    return t->pid;
}

int sys_spawn(u64 idx)
{
    if (idx >= EMBED_COUNT) return -1;
    embed_prog_t *p = &embedded[idx];
    u64 size = (u64)p->end - (u64)p->start;
    if (size == 0) return -1;
    u8 *binary = (u8*)p->start;
    if (size >= 4 && *(u32*)binary == ELF_MAGIC)
        return create_elf((void*)binary, size, p->fixed_pid);
    return task_create((void*)binary, size, p->load_addr, p->fixed_pid);
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

int sys_wait_any(void)
{
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state == TASK_EXITED && i != current_task)
            return tasks[i].pid;
    }
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].state != TASK_EXITED && i != current_task)
            tasks[i].wait_idx = current_task;
    }
    tasks[current_task].state = TASK_BLOCKED;
    return 0;
}

int sys_getstate(u64 pid)
{
    for (int i = 0; i < num_tasks; i++)
        if (tasks[i].pid == pid) return tasks[i].state;
    return -1;
}

static int map_pages_at(u64 pml4, u64 virt, u64 size, u64 flags)
{
    u64 old = paging_root;
    paging_switch(pml4);
    u64 start = virt & ~0xFFFULL;
    u64 end = (virt + size + 4095) & ~0xFFFULL;
    for (u64 v = start; v < end; v += 4096) {
        void *page = pmm_alloc_page();
        if (!page) { paging_switch(old); return -1; }
        map_page(v, (u64)page, flags);
    }
    paging_switch(old);
    return 0;
}

int sys_mmap(u64 virt, u64 size, u64 flags)
{
    if (current_task < 0) return -1;
    u64 pg_flags = PAGE_PRESENT | PAGE_USER;
    if (flags & 2) pg_flags |= PAGE_WRITABLE;
    if (flags & 4) pg_flags |= 0;
    return map_pages_at(tasks[current_task].pml4_phys, virt, size, pg_flags);
}

int sys_munmap(u64 virt, u64 size)
{
    if (current_task < 0) return -1;
    u64 old = paging_root;
    paging_switch(tasks[current_task].pml4_phys);
    u64 start = virt & ~0xFFFULL;
    u64 end = (virt + size + 4095) & ~0xFFFULL;
    for (u64 v = start; v < end; v += 4096)
        unmap_page(v);
    paging_switch(old);
    return 0;
}

int sys_kill(u64 pid)
{
    for (int i = 0; i < num_tasks; i++) {
        if (tasks[i].pid == pid && tasks[i].state != TASK_EXITED) {
            tasks[i].state = TASK_EXITED;
            if (tasks[i].stack_phys) pmm_free_page((void*)tasks[i].stack_phys);
            for (u64 j = 0; j < tasks[i].code_pages; j++)
                unmap_page(tasks[i].load_addr + j * 4096);
            unmap_page(tasks[i].load_addr + 0x100000 - 4096);
            return 0;
        }
    }
    return -1;
}

int sys_nanosleep(u64 ns)
{
    u64 start = ticks;
    u64 target = ns / 10000;
    while ((ticks - start) < target) {
        tasks[current_task].state = TASK_BLOCKED;
        __asm__ volatile("sti; hlt");
    }
    tasks[current_task].state = TASK_READY;
    return 0;
}

u64 sys_getticks(void)
{
    return ticks;
}

void sys_shutdown(void)
{
    outw(0x604, 0x2000);
    for (;;) __asm__ volatile("hlt");
}

int sys_mprotect(u64 virt, u64 size, u64 flags)
{
    if (current_task < 0) return -1;
    u64 old = paging_root;
    paging_switch(tasks[current_task].pml4_phys);
    u64 start = virt & ~0xFFFULL;
    u64 end = (virt + size + 4095) & ~0xFFFULL;
    u64 page_flags = PAGE_PRESENT | PAGE_USER;
    if (flags & 1) page_flags |= PAGE_WRITABLE;
    for (u64 v = start; v < end; v += 4096)
        map_page_flags(v, page_flags);
    paging_switch(old);
    return 0;
}

#define HEAP_START 0x600000
static u64 heap_brk = HEAP_START;

int sys_brk(u64 addr)
{
    if (current_task < 0) return -1;
    if (addr == 0) return heap_brk;
    u64 old = paging_root;
    paging_switch(tasks[current_task].pml4_phys);
    if (addr > heap_brk) {
        for (u64 v = heap_brk; v < addr; v += 4096) {
            void *pg = pmm_alloc_page();
            if (!pg) { paging_switch(old); return -1; }
            map_page(v, (u64)pg, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
    }
    paging_switch(old);
    u64 prev = heap_brk;
    heap_brk = addr;
    return prev;
}
