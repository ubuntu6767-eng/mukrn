#ifndef SYSCALL_H
#define SYSCALL_H

#include "io.h"

#define SYSCALL_EXIT      0
#define SYSCALL_GETPID    1
#define SYSCALL_SEND      2
#define SYSCALL_RECV      3
#define SYSCALL_INB       4
#define SYSCALL_OUTB      5
#define SYSCALL_SPAWN     6
#define SYSCALL_WAIT      7
#define SYSCALL_WAIT_ANY  8
#define SYSCALL_GETSTATE  9
#define SYSCALL_IRQ_REGISTER 10
#define SYSCALL_MMAP      11
#define SYSCALL_MUNMAP    12
#define SYSCALL_KILL      13
#define SYSCALL_NANOSLEEP 14
#define SYSCALL_GETTICKS  15
#define SYSCALL_DEBUG_PUTC 16
#define SYSCALL_SHUTDOWN  17
#define SYSCALL_MPROTECT  18
#define SYSCALL_BRK       19
#define SYSCALL_IRQ_ACK   20

u64 syscall_handler(u64 n, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

#endif
