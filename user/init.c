typedef unsigned long long u64;
typedef unsigned char u8;

static u64 syscall(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

#define SERIAL_DATA 0x3F8
#define SERIAL_LSR 0x3FD

static void putc(char c)
{
    while (!(syscall(4, SERIAL_LSR, 0, 0, 0) & 0x20));
    syscall(5, SERIAL_DATA, c, 0, 0);
}

static void puts(const char *s)
{
    while (*s) putc(*s++);
}

void _start(void)
{
    u64 pid = syscall(1, 0, 0, 0, 0);

    puts("\r\n[init] PID = ");

    u64 ticks = syscall(15, 0, 0, 0, 0);
    puts("ticks = ");
    char buf[21];
    int i = 20;
    buf[i] = 0;
    if (ticks == 0) buf[--i] = '0';
    while (ticks) { buf[--i] = '0' + (ticks % 10); ticks /= 10; }
    puts(buf + i);

    syscall(14, 5000000, 0, 0, 0);

    ticks = syscall(15, 0, 0, 0, 0);
    puts(" after 5ms sleep ticks = ");
    i = 20;
    buf[i] = 0;
    if (ticks == 0) buf[--i] = '0';
    u64 t2 = ticks;
    while (t2) { buf[--i] = '0' + (t2 % 10); t2 /= 10; }
    puts(buf + i);

    u64 brk_val = syscall(19, 0, 0, 0, 0);
    puts(" brk=");
    i = 20;
    buf[i] = 0;
    if (brk_val == 0) buf[--i] = '0';
    while (brk_val) { buf[--i] = '0' + (brk_val % 10); brk_val /= 10; }
    puts(buf + i);

    puts("\r\n[init] All syscalls OK\r\n");

    for (;;)
        __asm__ volatile("pause");
}
