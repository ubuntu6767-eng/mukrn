static void putc(char c);
static void puts(const char *s);
static int read(char *buf, int max);
static void exit(void);

void _start(void)
{
    char buf[256];

    puts("\x1b[2J\x1b[H");
    puts("sborchikOS v0.1\r\n");
    puts("Type 'help'\r\n\r\n");

    while (1) {
        puts("root@os:~$ ");

        int len = 0;
        while (1) {
            read(buf + len, 1);
            if (buf[len] == '\r' || buf[len] == '\n')
                break;
            len++;
        }
        buf[len] = 0;

        if (buf[0] == 0) continue;

        if (buf[0] == 'c') {
            puts("\x1b[2J\x1b[H");
        } else if (buf[0] == 'h') {
            puts("Commands:\r\n  help\r\n  clear\r\n  exit\r\n");
        } else if (buf[0] == 'e') {
            puts("Bye!\r\n");
            exit();
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

static int read(char *buf, int max) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(2), "D"(buf), "S"((long)max));
    return r;
}

static void exit(void) {
    __asm__ volatile("int $0x80" : : "a"(3));
    for (;;);
}
