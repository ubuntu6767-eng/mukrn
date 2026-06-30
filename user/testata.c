typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;

static int read_sector(int drive, u32 lba, u8 *buf);
static void kputc(char c);
static void kputs(const char *s);
static void kputhex(u64 v);

void _start(void)
{
    kputs("testata: start\r\n");
    u8 sec[512];
    int r0 = read_sector(1, 0, sec);
    kputs("r0="); kputhex(r0); kputs("\r\n");
    int r1 = read_sector(1, 158, sec);
    kputs("r1="); kputhex(r1); kputs("\r\n");
    if (r1 == 0) {
        kputs("data: ");
        for (int i = 0; i < 11; i++) kputhex(sec[i]);
        kputs("\r\n");
    }
    kputs("testata: done\r\n");
    for (;;) __asm__ volatile("hlt");
}

static int read_sector(int drive, u32 lba, u8 *buf) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(13), "D"((u64)drive), "S"((u64)lba), "d"((u64)buf));
    return r;
}

// Use IPC to serial for output
#define SERIAL_PID 5

static void kputc(char c) {
    u8 buf[64] = { (u8)c };
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(2), "D"(SERIAL_PID), "S"(0ULL), "d"((u64)buf), "c"(1ULL));
    c = c; // used
}

static void kputs(const char *s) {
    while (*s) {
        int i;
        for (i = 0; i < 63 && s[i]; i++);
        int r;
        __asm__ volatile("int $0x80" : "=a"(r)
            : "a"(2), "D"(SERIAL_PID), "S"(1ULL), "d"((u64)s), "c"((u64)i));
        s += i;
    }
}

static void kputhex(u64 v) {
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int n = v & 0xF;
        buf[i] = n < 10 ? '0' + n : 'a' + n - 10;
        v >>= 4;
    }
    buf[16] = 0;
    kputs(buf);
}
