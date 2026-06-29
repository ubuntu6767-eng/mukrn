#ifndef TASK_H
#define TASK_H

#include "io.h"

#define MAX_TASKS 64
#define USER_CODE_ADDR 0x400000
#define USER_STACK_TOP 0x500000

#define IPC_QUEUE_SIZE 16
#define IPC_MAX_DATA 64

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[IPC_MAX_DATA];
    u64 length;
} __attribute__((packed)) ipc_msg_t;

typedef enum {
    TASK_EXITED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} task_state_t;

typedef struct {
    u64 rsp;
    u64 pid;
    task_state_t state;
    u64 stack_phys;
    u64 user_stack_phys;
    u64 pml4_phys;
    u64 load_addr;
    u64 code_pages;
    int wait_idx;
    ipc_msg_t ipc_queue[IPC_QUEUE_SIZE];
    u32 ipc_head;
    u32 ipc_tail;
    u32 ipc_count;
} task_t;

#define EMBED_COUNT 6
typedef struct {
    unsigned char *start;
    unsigned char *end;
    u64 load_addr;
    u64 fixed_pid;
} embed_prog_t;

extern embed_prog_t embedded[];

extern task_t tasks[MAX_TASKS];
extern int current_task;
extern int num_tasks;

void task_init(void);
void create_process(void *binary, u64 size, u64 load_addr);
int task_create(void *binary, u64 size, u64 load_addr, u64 want_pid);
void scheduler_start(void);
void ipc_init_task(task_t *t);
u64 sys_getpid(void);
int sys_send(u64 target_pid, u64 type, const u8 *data, u64 len);
int sys_recv(ipc_msg_t *msg);
int sys_spawn(u64 idx);
void sys_exit(void);
int sys_wait(u64 pid);
int sys_wait_any(void);
int sys_getstate(u64 pid);
int sys_mmap(u64 virt, u64 size, u64 flags);
int sys_munmap(u64 virt, u64 size);

#endif
