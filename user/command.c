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

void _start(void)
{
    for (;;) {
        ipc_msg_t msg;
        while (recv(&msg) != 0)
            __asm__ volatile("pause");

        if (msg.type != 1) continue;

        u64 shell = msg.sender_pid;
        char c = msg.data[0];

        if (c == 'h') {
            const char *resp = "Commands:\r\n  help\r\n  clear\r\n  exit\r\n";
            send(shell, 2, (const u8*)resp, 36);
        } else if (c == 'c') {
            const char *resp = "\x1b[2J\x1b[H";
            send(shell, 2, (const u8*)resp, 6);
        } else if (c == 'e') {
            const char *resp = "Bye!\r\n";
            send(shell, 3, (const u8*)resp, 6);
            for (;;) __asm__ volatile("cli; hlt");
        } else {
            const char *resp = "Unknown\r\n";
            send(shell, 2, (const u8*)resp, 9);
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
