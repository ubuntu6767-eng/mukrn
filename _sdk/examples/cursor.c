typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned char u8;

#define VI 0x1CE
#define VD 0x1CF

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80"
        : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

static void vw(u16 i, u16 v) { sys(25, VI, i, 0, 0); sys(25, VD, v, 0, 0); }
static void putc(char c) {
    while (!(sys(4, 0x3FD, 0, 0, 0) & 0x20));
    sys(5, 0x3F8, (u64)c, 0, 0);
}
static void puts(const char *s) { while (*s) putc(*s++); }
static u8 getc(void) {
    while (!(sys(4, 0x3FD, 0, 0, 0) & 1));
    return (u8)sys(4, 0x3F8, 0, 0, 0);
}

static void mouse_wait(int a_type)
{
    int timeout = 100000;
    if (a_type == 0) {
        while (timeout--) {
            if (sys(4, 0x64, 0, 0, 0) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(sys(4, 0x64, 0, 0, 0) & 2)) return;
        }
    }
}

static void mouse_write(u8 byte)
{
    mouse_wait(1);
    sys(5, 0x64, 0xD4, 0, 0);
    mouse_wait(1);
    sys(5, 0x60, byte, 0, 0);
}

static u8 mouse_read(void)
{
    mouse_wait(0);
    return (u8)sys(4, 0x60, 0, 0, 0);
}

static void mouse_init(void)
{
    mouse_wait(1);
    sys(5, 0x64, 0xA8, 0, 0);

    u8 cfg;
    mouse_wait(1);
    sys(5, 0x64, 0x20, 0, 0);
    cfg = mouse_read();

    cfg |= 2;
    cfg &= ~0x20;
    mouse_wait(1);
    sys(5, 0x64, 0x60, 0, 0);
    mouse_wait(1);
    sys(5, 0x60, cfg, 0, 0);

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
}

static int mouse_poll(s32 *dx, s32 *dy, u8 *buttons)
{
    static int st = 0;
    static u8 pkt[3];

    u8 sts = (u8)sys(4, 0x64, 0, 0, 0);
    if (!(sts & 0x20) || !(sts & 1)) return 0;

    u8 byte = (u8)sys(4, 0x60, 0, 0, 0);

    if (st == 0) {
        if (!(byte & 0x08)) return 0;
        pkt[0] = byte; st = 1; return 0;
    }
    if (st == 1) {
        pkt[1] = byte; st = 2; return 0;
    }
    pkt[2] = byte; st = 0;

    *buttons = pkt[0] & 7;
    *dx = (pkt[0] & 0x10) ? ((s32)(signed char)pkt[1]) - 256 : (s32)pkt[1];
    *dy = (pkt[0] & 0x20) ? ((s32)(signed char)pkt[2]) - 256 : (s32)pkt[2];
    *dy = -*dy;
    return 1;
}

static void draw_cursor(volatile u32 *fb, int x, int y, u32 color)
{
    for (int i = -6; i <= 6; i++) {
        int px = x + i;
        if (px >= 0 && px < 640) fb[y * 640 + px] = color;
        int py = y + i;
        if (py >= 0 && py < 480) fb[py * 640 + x] = color;
    }
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++) {
            int px = x + dx, py = y + dy;
            if (px >= 0 && px < 640 && py >= 0 && py < 480)
                fb[py * 640 + px] = color;
        }
}

void _start(void)
{
    vw(0, 0xB0C5);
    vw(4, 0);
    vw(1, 640);
    vw(2, 480);
    vw(3, 32);
    vw(4, 0x41);
    sys(23, 0xFD00000, 0xFD000000, 640*480*4, 6);

    volatile u32 *fb = (u32*)0xFD00000;
    for (int i = 0; i < 640*480; i++) fb[i] = 0;

    mouse_init();
    puts("mouse+wasd cursor\n");

    int x = 320, y = 240;
    draw_cursor(fb, x, y, 0x00FFFFFF);

    while (1) {
        s32 dx = 0, dy = 0;
        u8 buttons = 0;

        while (mouse_poll(&dx, &dy, &buttons)) {
            draw_cursor(fb, x, y, 0);
            x += dx; y += dy;
            if (x < 0) x = 0;
            if (x >= 640) x = 639;
            if (y < 0) y = 0;
            if (y >= 480) y = 479;
            draw_cursor(fb, x, y, 0x00FFFFFF);
        }

        if (sys(4, 0x3FD, 0, 0, 0) & 1) {
            u8 c = (u8)sys(4, 0x3F8, 0, 0, 0);
            draw_cursor(fb, x, y, 0);
            switch (c) {
                case 'w': if (y > 0) y--; break;
                case 's': if (y < 479) y++; break;
                case 'a': if (x > 0) x--; break;
                case 'd': if (x < 639) x++; break;
                case 'q': goto done;
            }
            draw_cursor(fb, x, y, 0x00FFFFFF);
        }

        sys(27, 0, 0, 0, 0);
    }

done:
    puts("bye\n");
    sys(0, 0, 0, 0, 0);
}
