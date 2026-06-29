#ifndef TASK_H
#define TASK_H

#include "io.h"

#define MAX_TASKS 64
#define USER_CODE_ADDR 0x400000
#define USER_STACK_TOP 0x500000

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
} task_t;

void task_init(void);
void create_process(void *binary, u64 size);
void scheduler_start(void);

#endif
