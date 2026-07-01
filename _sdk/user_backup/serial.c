typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;

static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static int getpid(void);
static u8 inb(u16 port);
static void outb(u16 port, u8 val);

#define SHELL_PID 4

void _start(void)
{
    for (;;) {
        ipc_msg_t msg;
        while (recv(&msg) == 0) {
            if (msg.type == 0) {
                while (!(inb(0x3FD) & 0x20));
                outb(0x3F8, msg.data[0]);
            } else if (msg.type == 1) {
                char *s = (char*)msg.data;
                while (*s) {
                    while (!(inb(0x3FD) & 0x20));
                    outb(0x3F8, *s++);
                }
            }
        }
        if (inb(0x3FD) & 1) {
            char c = inb(0x3F8);
            u8 buf[1] = { (u8)c };
            send(SHELL_PID, 0, buf, 1);
        }
        __asm__ volatile("pause");
    }
}

static int recv(ipc_msg_t *msg) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(3), "D"(msg));
    return r;
}

static int send(u64 pid, u64 type, const u8 *data, u64 len) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(2), "D"(pid), "S"(type), "d"((long)data), "c"(len));
    return r;
}

static int getpid(void) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(1));
    return r;
}

static u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(4), "D"(port));
    return r;
}

static void outb(u16 port, u8 val) {
    __asm__ volatile("int $0x80" : : "a"(5), "D"(port), "S"(val));
}
