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
static void puthex(u64 v);
static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static void sys_exit(void);

#define CMD_PID 3
#define SERIAL_PID 5

static char getchar(void);

void _start(void)
{
    char buf[256];

    puts("\x1b[2J\x1b[H");
    puts("sborchikOS v0.2 (microkernel)\r\n");
    puts("Type 'help'\r\n\r\n");

    while (1) {
        puts("root@os:~$ ");

        int len = 0;
        while (1) {
            char c = getchar();
            if (c == '\r' || c == '\n') {
                putc('\r');
                putc('\n');
                break;
            }
            putc(c);
            if (len < 255)
                buf[len++] = c;
        }
        buf[len] = 0;

        if (buf[0] == 0) continue;

        send(CMD_PID, 1, (const u8*)buf, len);

        ipc_msg_t resp;
        int r;
        do {
            __asm__ volatile("pause");
            r = recv(&resp);
        } while (r != 0 || resp.sender_pid != CMD_PID);

        if (resp.type == 3) {
            puts((const char*)resp.data);
            break;
        }

        puts((const char*)resp.data);
    }
    puts("shell: goodbye\r\n");
    for (;;) __asm__ volatile("pause");
}

static void putc(char c) {
    u8 buf[64] = {c};
    send(SERIAL_PID, 0, buf, 1);
}

static void puts(const char *s) {
    while (*s) {
        u8 buf[64];
        int i;
        for (i = 0; i < 63 && s[i]; i++)
            buf[i] = s[i];
        buf[i] = 0;
        send(SERIAL_PID, 1, buf, i + 1);
        s += i;
    }
}

static void puthex(u64 v) {
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int n = v & 0xF;
        buf[i] = n < 10 ? '0' + n : 'a' + n - 10;
        v >>= 4;
    }
    buf[16] = 0;
    puts(buf);
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

static void sys_exit(void) {
    __asm__ volatile("int $0x80" : : "a"(0));
}

static char getchar(void)
{
    for (;;) {
        ipc_msg_t msg;
        int r = recv(&msg);
        if (r == 0)
            return msg.data[0];
        __asm__ volatile("pause");
    }
}
