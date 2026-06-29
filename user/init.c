typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;

typedef struct {
    u64 idx;
    u64 want_pid;
    int restart;
} init_entry_t;

static void putc(char c);
static void puts(const char *s);
static void puthex(u64 v);
static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static int spawn(u64 idx);
static int wait_any(void);
static int getstate(u64 pid);

#define SERIAL_PID 5

static init_entry_t config[] = {
    {4, 5, 1},
    {1, 2, 1},
    {2, 3, 1},
    {3, 4, 1},
};
static int n = 4;

void _start(void)
{
    u64 pids[16];

    int serial_pid = spawn(config[0].idx);
    if (serial_pid < 0) {
        for (;;) __asm__ volatile("hlt");
    }
    pids[0] = (u64)serial_pid;

    puts("init: booting\r\n");

    for (int i = 1; i < n; i++) {
        int pid = spawn(config[i].idx);
        if (pid < 0) {
            puts("init: spawn failed\r\n");
            for (;;) {
                putc('.');
                for (volatile u64 i = 0; i < 5000000; i++);
            }
        }
        pids[i] = (u64)pid;
    }

    for (;;) {
        wait_any();
        for (int i = 0; i < n; i++) {
            if (getstate(pids[i]) == 0) {
                if (config[i].restart) {
                    puts("init: restart ");
                    puthex(pids[i]);
                    putc('\n');
                    pids[i] = (u64)spawn(config[i].idx);
                }
            }
        }
    }
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

static int spawn(u64 idx) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(6), "D"(idx));
    return r;
}

static int wait_any(void) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(8));
    return r;
}

static int getstate(u64 pid) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(9), "D"(pid));
    return r;
}
