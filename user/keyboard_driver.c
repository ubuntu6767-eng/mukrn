typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;

static void putc(char c);
static void puts(const char *s);
static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static int irq_register(u64 irq);
static int getpid(void);

static char map[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 0, 0, 0, 0,
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void _start(void)
{
    irq_register(1);

    for (;;) {
        ipc_msg_t msg;
        while (recv(&msg) != 0)
            __asm__ volatile("pause");

        if (msg.sender_pid == 0 && msg.type == 0) {
            u8 sc = msg.data[0];
            if (!(sc & 0x80) && sc < sizeof(map)) {
                char c = map[sc];
                if (c) {
                    u8 buf[1] = { (u8)c };
                    send(4, 0, buf, 1);
                }
            }
        }
    }
}

static void putc(char c) {
    __asm__ volatile("int $0x80" : : "a"(0), "D"(c));
}

static void puts(const char *s) {
    __asm__ volatile("int $0x80" : : "a"(1), "D"(s));
}

static int recv(ipc_msg_t *msg) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(6), "D"(msg));
    return r;
}

static int send(u64 pid, u64 type, const u8 *data, u64 len) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(5), "D"(pid), "S"(type), "d"((long)data), "c"(len));
    return r;
}

static int irq_register(u64 irq) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(13), "D"(irq));
    return r;
}

static int getpid(void) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(4));
    return r;
}
