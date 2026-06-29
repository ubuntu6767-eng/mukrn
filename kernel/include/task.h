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
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_EXITED
} task_state_t;

typedef struct {
    u64 rsp;
    u64 pid;
    task_state_t state;
    u64 stack_phys;
    u64 user_stack_phys;
    u64 pml4_phys;
    ipc_msg_t ipc_queue[IPC_QUEUE_SIZE];
    u32 ipc_head;
    u32 ipc_tail;
    u32 ipc_count;
} task_t;

extern task_t tasks[MAX_TASKS];
extern int current_task;
extern int num_tasks;

void task_init(void);
void create_process(void *binary, u64 size, u64 load_addr);
void scheduler_start(void);
void ipc_init_task(task_t *t);
u64 sys_getpid(void);
int sys_send(u64 target_pid, u64 type, const u8 *data, u64 len);
int sys_recv(ipc_msg_t *msg);

#endif
