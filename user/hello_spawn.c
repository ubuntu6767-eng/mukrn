typedef unsigned long long u64;
typedef unsigned char u8;

static int send(u64 pid, u64 type, const u8 *data, u64 len) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(2), "D"(pid), "S"(type), "d"((long)data), "c"(len));
    return r;
}

static void exit(int code) {
    __asm__ volatile("int $0x80" : : "a"(0), "D"((u64)code));
    for (;;) __asm__ volatile("pause");
}

void _start(void) {
    const char *msg = "Hello from spawned process!\r\n";
    while (*msg) {
        u8 buf[1] = { (u8)*msg };
        send(5, 0, buf, 1);
        msg++;
    }
    exit(0);
}
