#ifndef SYSCALL_H
#define SYSCALL_H

#include "io.h"

#define SYSCALL_PUTC    0
#define SYSCALL_PUTS    1
#define SYSCALL_READ    2
#define SYSCALL_EXIT    3
#define SYSCALL_GETPID  4
#define SYSCALL_SEND    5
#define SYSCALL_RECV    6
#define SYSCALL_INB     7
#define SYSCALL_OUTB    8

u64 syscall_handler(u64 n, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

#endif
