typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

static u64 _sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4) {
    u64 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

#define VI 0x1CE
#define VD 0x1CF
static void vw(u16 i, u16 v) { _sys(25, VI, i, 0, 0); _sys(25, VD, v, 0, 0); }

void _start(void) {
    vw(0, 0xB0C5);
    vw(4, 0);
    vw(1, 640);
    vw(2, 480);
    vw(3, 32);
    vw(4, 0x41);

    int r = _sys(23, 0xFD00000, 0xFD000000, 640*480*4, 6);
    if (r) for (;;) __asm__ volatile("pause");

    volatile u32 *fb = (u32*)0xFD00000;

    // White top stripe (30px)
    for (int y = 0; y < 30; y++)
        for (int x = 0; x < 640; x++)
            fb[y * 640 + x] = 0x00FFFFFF;

    // Red left stripe (30px, full height)
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 30; x++)
            fb[y * 640 + x] = 0x00FF0000;

    // Green horizontal thick bar (100px)
    for (int y = 190; y < 290; y++)
        for (int x = 0; x < 640; x++)
            fb[y * 640 + x] = 0x0000FF00;

    // Blue bottom stripe (30px)
    for (int y = 450; y < 480; y++)
        for (int x = 0; x < 640; x++)
            fb[y * 640 + x] = 0x000000FF;

    // Yellow X from center (3px thick)
    for (int i = -200; i < 200; i++) {
        int cx = 320, cy = 240;
        for (int t = -1; t <= 1; t++) {
            fb[(cy + i + t) * 640 + cx + i] = 0x00FFFF00;
            fb[(cy - i + t) * 640 + cx + i] = 0x00FFFF00;
        }
    }

    for (;;) __asm__ volatile("pause");
}
