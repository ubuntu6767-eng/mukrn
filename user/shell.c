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
static u8 inb(u16 port);

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

        if (buf[0] == 'h') {
            puts("Commands:\r\n  help\r\n  clear\r\n  exit\r\n");
        } else if (buf[0] == 'c') {
            puts("\x1b[2J\x1b[H");
        } else if (buf[0] == 'e') {
            puts("Bye!\r\n");
            for (;;) __asm__ volatile("cli; hlt");
        } else {
            puts("Unknown\r\n");
        }
    }
}

static void putc(char c) {
    __asm__ volatile("int $0x80" : : "a"(0), "D"(c));
}

static void puts(const char *s) {
    __asm__ volatile("int $0x80" : : "a"(1), "D"(s));
}

static u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(7), "D"(port));
    return r;
}

static int recv(ipc_msg_t *msg) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(6), "D"(msg));
    return r;
}

static char getchar(void)
{
    for (;;) {
        ipc_msg_t msg;
        int r = recv(&msg);
        if (r == 0 && msg.type == 1) {
            return msg.data[0];
        }
        if (inb(0x3FD) & 1) {
            return inb(0x3F8);
        }
        __asm__ volatile("pause");
    }
}
