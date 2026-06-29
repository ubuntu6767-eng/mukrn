#include "syscall.h"
#include "serial.h"
#include "keyboard.h"
#include "task.h"

u64 syscall_handler(u64 n, u64 arg1, u64 arg2, u64 arg3)
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
            char c = kb_read();
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
    default:
        puts("[kernel] Unknown syscall: ");
        puthex(n);
        puts("\r\n");
        return 0;
    }
}
