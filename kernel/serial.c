#include "serial.h"

void putc(char c)
{
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

void puts(const char *s)
{
    while (*s) putc(*s++);
}

void puthex(u64 v)
{
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int n = v & 0xF;
        buf[i] = n < 10 ? '0' + n : 'a' + n - 10;
        v >>= 4;
    }
    buf[16] = 0;
    puts(buf);
}
