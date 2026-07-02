typedef unsigned long long u64;
typedef unsigned short u16;
typedef unsigned int u32;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80"
        : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

#define VI 0x1CE
#define VD 0x1CF
static void vw(u16 i, u16 v) { sys(25, VI, i, 0, 0); sys(25, VD, v, 0, 0); }

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
    int y = 0, dy = 1;
    int H = 60;

    for (int yy = 0; yy < H; yy++)
        for (int x = 0; x < 640; x++)
            fb[(y + yy) * 640 + x] = 0x0000FF00;

    u64 last = 0;
    for (;;) {
        u64 t = sys(15, 0, 0, 0, 0);
        if (t - last >= 2) {
            last = t;
            for (int yy = 0; yy < H; yy++)
                for (int x = 0; x < 640; x++)
                    fb[(y + yy) * 640 + x] = 0;
            if (dy > 0 && y + dy + H > 480) dy = -dy;
            if (dy < 0 && y + dy < 0) dy = -dy;
            y += dy;
            for (int yy = 0; yy < H; yy++)
                for (int x = 0; x < 640; x++)
                    fb[(y + yy) * 640 + x] = 0x0000FF00;
        }
        sys(27, 0, 0, 0, 0);
    }
}
