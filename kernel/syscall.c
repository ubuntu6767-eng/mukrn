#include "syscall.h"
#include "serial.h"
#include "task.h"

u64 syscall_handler(u64 n, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    switch (n) {
    case SYSCALL_PUTC:
        putc((char)arg1);
        return 0;
    case SYSCALL_PUTS:
        puts((const char*)arg1);
        return 0;
    case SYSCALL_READ: {
        char *buf = (char*)arg1;
        u64 max = arg2;
        for (u64 i = 0; i < max; i++) {
            while (!(inb(0x3FD) & 1)) __asm__ volatile("pause");
            char c = inb(0x3F8);
            buf[i] = c;
            if (c == '\r' || c == '\n') {
                putc('\r');
                putc('\n');
                return i + 1;
            }
            putc(c);
        }
        return max;
    }
    case SYSCALL_EXIT:
        puts("[kernel] Process exited\r\n");
        for (;;) __asm__ volatile("cli; hlt");
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
    default:
        puts("[kernel] Unknown syscall: ");
        puthex(n);
        puts("\r\n");
        return 0;
    }
}
