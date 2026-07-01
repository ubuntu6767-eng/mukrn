typedef unsigned long long u64;
typedef unsigned short u16;

#include "init_procs.h"

static u64 _sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

static void putc(char c) {
    while (!(_sys(4, 0x3FD, 0, 0, 0) & 0x20));
    _sys(5, 0x3F8, (u64)c, 0, 0);
}
static void puts(const char *s) { while (*s) putc(*s++); }

void _start(void) {
    puts("[init] μkrn ready\n");

    for (int i = 0; i < spawn_count; i++) {
        u64 size = (u64)(spawn_list[i].end - spawn_list[i].addr);
        puts("[init] spawning ");
        puts(spawn_list[i].name);
        puts("\n");
        _sys(26, (u64)spawn_list[i].addr, size, 0, 0);
    }

    for (;;) __asm__ volatile("pause");
}
