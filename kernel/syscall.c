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
    case SYSCALL_SPAWN:
        return sys_spawn(arg1);
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
    case SYSCALL_READ_SECTOR:
        return sys_read_sector(arg1, arg2, (u8*)arg3);
    case SYSCALL_SPAWN_EXEC:
        return sys_spawn_exec((void*)arg1, arg2, (u64*)arg3);
    case SYSCALL_WRITE_SECTOR:
        return sys_write_sector(arg1, arg2, (u8*)arg3);
    default:
        puts("[kernel] Unknown syscall: ");
        puthex(n);
        puts("\r\n");
        return 0;
    }
}
