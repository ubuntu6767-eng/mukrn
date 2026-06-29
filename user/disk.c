typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;

#define ATA_DATA      0x1F0
#define ATA_ERROR     0x1F1
#define ATA_SEC_CNT   0x1F2
#define ATA_LBA_LO    0x1F3
#define ATA_LBA_MID   0x1F4
#define ATA_LBA_HI    0x1F5
#define ATA_DRIVE     0x1F6
#define ATA_COMMAND   0x1F7
#define ATA_STATUS    0x1F7

#define ATA_CMD_READ  0x20
#define ATA_BSY       0x80
#define ATA_DRQ       0x08
#define ATA_ERR       0x01
#define ATA_DF        0x20

#define DISK_TYPE_READ  0
#define DISK_RESP_BASE  100
#define DISK_RESP_ERR   255

static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static u8 inb(u16 port);
static void outb(u16 port, u8 val);

static int ata_wait(void)
{
    for (int tries = 0; tries < 10000000; tries++) {
        u8 status = inb(ATA_STATUS);
        if (!(status & ATA_BSY)) {
            if (status & ATA_ERR) return -1;
            return 0;
        }
        __asm__ volatile("pause");
    }
    return -1;
}

static int ata_read(u32 lba, u8 drive, u8 *buf)
{
    outb(ATA_DRIVE, (drive << 4) | 0xE0 | ((lba >> 24) & 0x0F));
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);

    outb(ATA_SEC_CNT, 1);
    outb(ATA_LBA_LO, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HI, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ);
    inb(ATA_STATUS);

    if (ata_wait() < 0) return -1;

    for (int i = 0; i < 256; i++) {
        buf[i * 2] = inb(ATA_DATA);
        buf[i * 2 + 1] = inb(ATA_DATA);
    }

    return 0;
}

void _start(void)
{
    for (;;) {
        ipc_msg_t req;
        while (recv(&req) != 0)
            __asm__ volatile("pause");

        if (req.type == DISK_TYPE_READ) {
            u32 lba = (u32)req.data[0]
                    | ((u32)req.data[1] << 8)
                    | ((u32)req.data[2] << 16)
                    | ((u32)req.data[3] << 24);
            u8 drive = req.data[4];

            u8 sector[512];
            if (ata_read(lba, drive, sector) == 0) {
                for (int ch = 0; ch < 8; ch++) {
                    u8 chunk[64];
                    for (int j = 0; j < 64; j++)
                        chunk[j] = sector[ch * 64 + j];
                    send(req.sender_pid, DISK_RESP_BASE + ch, chunk, 64);
                }
            } else {
                send(req.sender_pid, DISK_RESP_ERR, 0, 0);
            }
        }
    }
}

static int recv(ipc_msg_t *msg) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(3), "D"(msg));
    return r;
}

static int send(u64 pid, u64 type, const u8 *data, u64 len) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(2), "D"(pid), "S"(type), "d"((long)data), "c"(len));
    return r;
}

static u8 inb(u16 port) {
    u8 r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(4), "D"(port));
    return r;
}

static void outb(u16 port, u8 val) {
    __asm__ volatile("int $0x80" : : "a"(5), "D"(port), "S"(val));
}
