typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

typedef struct {
    u64 idx;
    u64 want_pid;
    int restart;
} init_entry_t;

static void putc(char c);
static void puts(const char *s);
static void puthex(u64 v);
static int spawn(u64 idx);
static int wait_any(void);
static int getstate(u64 pid);

static init_entry_t config[] = {
    {1, 2, 1},
    {2, 3, 1},
    {3, 4, 1},
};
static int n = 3;

void _start(void)
{
    u64 pids[16];

    if (n == 0) {
        for (;;) {
            putc('.');
            for (volatile u64 i = 0; i < 5000000; i++);
        }
    }

    puts("init: booting\r\n");

    for (int i = 0; i < n; i++) {
        int pid = spawn(config[i].idx);
        if (pid < 0) {
            puts("init: spawn failed\r\n");
            for (;;) { putc('.'); for (volatile u64 i = 0; i < 5000000; i++); }
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
    __asm__ volatile("int $0x80" : : "a"(0), "D"(c));
}

static void puts(const char *s) {
    __asm__ volatile("int $0x80" : : "a"(1), "D"(s));
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

static int spawn(u64 idx) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(9), "D"(idx));
    return r;
}

static int wait_any(void) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(11));
    return r;
}

static int getstate(u64 pid) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(12), "D"(pid));
    return r;
}
