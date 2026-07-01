#include "syscall.h"
#include "serial.h"
#include "task.h"
#include "idt.h"

u64 syscall_handler(u64 n, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    switch (n) {
    case SYSCALL_EXIT:
        sys_exit();
        return 0;
    case SYSCALL_GETPID:
        return sys_getpid();
    case SYSCALL_SEND: {
        u64 target_pid = arg1;
        u64 type = arg2;
        const u8 *data = (const u8*)arg3;
        u64 len = arg4;
        return sys_send(target_pid, type, data, len);
    }
    case SYSCALL_RECV: {
        ipc_msg_t *msg = (ipc_msg_t*)arg1;
        return sys_recv(msg);
    }
    case SYSCALL_INB:
        return inb((u16)arg1);
    case SYSCALL_OUTB:
        outb((u16)arg1, (u8)arg2);
        return 0;
    case SYSCALL_WAIT:
        return sys_wait(arg1);
    case SYSCALL_WAIT_ANY:
        return sys_wait_any();
    case SYSCALL_GETSTATE:
        return sys_getstate(arg1);
    case SYSCALL_IRQ_REGISTER:
        return sys_irq_register(arg1);
    case SYSCALL_MMAP:
        return sys_mmap(arg1, arg2, arg3);
    case SYSCALL_MUNMAP:
        return sys_munmap(arg1, arg2);
    case SYSCALL_KILL:
        return sys_kill(arg1);
    case SYSCALL_NANOSLEEP:
        return sys_nanosleep(arg1);
    case SYSCALL_GETTICKS:
        return sys_getticks();
    case SYSCALL_DEBUG_PUTC:
        outb(0xE9, (u8)arg1);
        return 0;
    case SYSCALL_SHUTDOWN:
        sys_shutdown();
        return 0;
    case SYSCALL_MPROTECT:
        return sys_mprotect(arg1, arg2, arg3);
    case SYSCALL_BRK:
        return sys_brk(arg1);
    case SYSCALL_IRQ_ACK:
        outb(0x20, 0x20);
        if (arg1 >= 8) outb(0xA0, 0x20);
        return 0;
    case SYSCALL_CLONE:
        return sys_clone(arg1, arg2, arg3, arg4);
    case SYSCALL_FUTEX:
        return sys_futex((u32*)arg1, (int)arg2, (u32)arg3);
    case SYSCALL_MMAP_PHYS:
        return sys_mmap_phys(arg1, arg2, arg3, arg4);
    case SYSCALL_INW: {
        u16 v;
        __asm__ volatile("inw %1, %0" : "=a"(v) : "d"((u16)arg1));
        return v;
    }
    case SYSCALL_OUTW:
        __asm__ volatile("outw %0, %1" : : "a"((u16)arg2), "d"((u16)arg1));
        return 0;
    case SYSCALL_SPAWN_AT:
        return sys_spawn_at((void*)arg1, arg2);
    case SYSCALL_YIELD:
        return 0;
    default:
        puts("[kernel] Unknown syscall: ");
        puthex(n);
        puts("\r\n");
        return 0;
    }
}
