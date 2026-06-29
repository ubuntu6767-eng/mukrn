typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

static void putc(char c);
static u8 inb(u16 port);
static int send(u64 pid, u64 type, const u8 *data, u64 len);

static char map[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 0, 0, 0, 0,
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void _start(void)
{
    for (;;) {
        if (inb(0x64) & 1) {
            u8 sc = inb(0x60);
            if (!(sc & 0x80) && sc < sizeof(map)) {
                char c = map[sc];
                if (c) {
                    u8 buf[1] = { (u8)c };
                    send(2, 1, buf, 1);
                }
            }
        }
        __asm__ volatile("pause");
    }
}

static void putc(char c) {
    __asm__ volatile("int $0x80" : : "a"(0), "D"(c));
}

static u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(7), "D"(port));
    return r;
}

static int send(u64 pid, u64 type, const u8 *data, u64 len) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(5), "D"(pid), "S"(type), "d"((long)data), "c"(len));
    return r;
}
