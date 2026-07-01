typedef unsigned long long u64;
typedef unsigned char u8;

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} __attribute__((packed)) ipc_msg_t;

static u64 syscall(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

void _start(void)
{
    ipc_msg_t msg;
    for (;;) {
        int r = syscall(3, (u64)&msg, 0, 0, 0);
        if (r == 0 && msg.sender_pid == 1) {
            u8 reply[64];
            reply[0] = 2;
            syscall(2, 1, 2, (u64)reply, 1);
            break;
        }
    }

    syscall(0, 0, 0, 0, 0);
}
