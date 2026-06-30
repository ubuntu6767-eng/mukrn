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

static int recv(ipc_msg_t *msg);
static int send(u64 pid, u64 type, const u8 *data, u64 len);
static int read_sector(int drive, u32 lba, u8 *buf);

static u16 bytes_per_sector;
static u8  sectors_per_cluster;
static u16 reserved_sectors;
static u8  num_fats;
static u32 sectors_per_fat;
static u32 root_cluster;
static u32 first_data_sector;

#define FAT32_DRIVE 1

static int init_fs(void)
{
    u8 sec[512];
    if (read_sector(FAT32_DRIVE, 0, sec) < 0) return -1;
    bytes_per_sector = *(u16*)&sec[0x0B];
    sectors_per_cluster = sec[0x0D];
    reserved_sectors = *(u16*)&sec[0x0E];
    num_fats = sec[0x10];
    sectors_per_fat = *(u32*)&sec[0x24];
    root_cluster = *(u32*)&sec[0x2C];
    first_data_sector = reserved_sectors + num_fats * sectors_per_fat;
    return 0;
}

static u32 cluster_to_sector(u32 c)
{
    return first_data_sector + (c - 2) * sectors_per_cluster;
}

static u32 next_cluster(u32 c)
{
    u32 off = c * 4;
    u32 fat_sec = reserved_sectors + off / bytes_per_sector;
    u32 in_off = off % bytes_per_sector;
    u8 sec[512];
    if (read_sector(FAT32_DRIVE, fat_sec, sec) < 0) return 0;
    return *(u32*)&sec[in_off] & 0x0FFFFFFF;
}

static int is_eoc(u32 c)
{
    return c >= 0x0FFFFFF8;
}

static void to_8_3(const char *name, u8 out[11])
{
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int dot = -1;
    for (int i = 0; name[i]; i++) {
        if (name[i] == '.') { dot = i; break; }
    }
    int lim = (dot < 0) ? 11 : dot;
    for (int i = 0; i < lim && i < 8 && name[i]; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[i] = c;
    }
    if (dot >= 0) {
        for (int i = 0; i < 3 && name[dot + 1 + i]; i++) {
            char c = name[dot + 1 + i];
            if (c >= 'a' && c <= 'z') c -= 32;
            out[8 + i] = c;
        }
    }
}

static int find_in_dir(u32 dir_cluster, const u8 name[11], u32 *out_cluster, u32 *out_size)
{
    u32 c = dir_cluster;
    while (!is_eoc(c) && c >= 2) {
        u32 sec_base = cluster_to_sector(c);
        for (int s = 0; s < sectors_per_cluster; s++) {
            u8 sec[512];
            if (read_sector(FAT32_DRIVE, sec_base + s, sec) < 0) return -1;
            for (int off = 0; off < 512; off += 32) {
                u8 *e = sec + off;
                if (e[0] == 0) return -1;
                if (e[0] == 0xE5) continue;
                if (e[0xB] == 0x0F) continue;
                if (e[0xB] & 0x08) continue;
                int match = 1;
                for (int i = 0; i < 11; i++)
                    if (e[i] != name[i]) { match = 0; break; }
                if (match) {
                    *out_cluster = (*(u16*)&e[0x14] << 16) | *(u16*)&e[0x1A];
                    *out_size = *(u32*)&e[0x1C];
                    return 1;
                }
            }
        }
        c = next_cluster(c);
    }
    return -1;
}

static int resolve_path(const char *path, u32 *cluster, u32 *size)
{
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/') {
            u8 dir_83[11];
            char dirname[64];
            int dlen = i;
            if (dlen > 63) return -1;
            for (int j = 0; j < dlen; j++) dirname[j] = path[j];
            dirname[dlen] = 0;
            to_8_3(dirname, dir_83);
            u32 tmp_cluster;
            if (find_in_dir(root_cluster, dir_83, &tmp_cluster, size) != 1)
                return -1;
            u8 file_83[11];
            to_8_3(path + i + 1, file_83);
            return find_in_dir(tmp_cluster, file_83, cluster, size);
        }
    }
    u8 name[11];
    to_8_3(path, name);
    return find_in_dir(root_cluster, name, cluster, size);
}

static u8 file_buf[16384];

void _start(void)
{
    if (init_fs() < 0) {
        for (;;) __asm__ volatile("pause");
    }

    for (;;) {
        ipc_msg_t req;
        while (recv(&req) != 0)
            __asm__ volatile("pause");

        if (req.type > 1) {
            send(req.sender_pid, 255, 0, 0);
            continue;
        }

        char *path = (char*)req.data;
        u32 cluster, size;
        if (resolve_path(path, &cluster, &size) != 1) {
            send(req.sender_pid, 255, 0, 0);
            continue;
        }

        if (size > sizeof(file_buf)) {
            send(req.sender_pid, 255, 0, 0);
            continue;
        }

        u32 c = cluster;
        u32 off = 0;
        while (!is_eoc(c) && c >= 2 && off < size) {
            u32 sbase = cluster_to_sector(c);
            for (int s = 0; s < sectors_per_cluster && off < size; s++) {
                if (read_sector(FAT32_DRIVE, sbase + s, file_buf + off) < 0) {
                    send(req.sender_pid, 255, 0, 0);
                    goto next;
                }
                off += bytes_per_sector;
            }
            c = next_cluster(c);
        }
        if (off < size) {
            send(req.sender_pid, 255, 0, 0);
            continue;
        }

        if (req.type == 1) {
            u64 pid;
            int r;
            __asm__ volatile("int $0x80" : "=a"(r)
                : "a"(14), "D"((u64)file_buf), "S"(size), "d"((u64)&pid));
            if (r == 0) {
                u8 resp[8];
                for (int i = 0; i < 8; i++) {
                    resp[i] = pid & 0xFF;
                    pid >>= 8;
                }
                send(req.sender_pid, 101, resp, 8);
            } else {
                send(req.sender_pid, 255, 0, 0);
            }
        } else {
            u32 offs = 0;
            while (offs < size) {
                u32 chunk = size - offs;
                if (chunk > 59) chunk = 59;
                u8 msg[64];
                msg[0] = offs & 0xFF;
                msg[1] = (offs >> 8) & 0xFF;
                msg[2] = (offs >> 16) & 0xFF;
                msg[3] = (offs >> 24) & 0xFF;
                msg[4] = chunk;
                for (u32 j = 0; j < chunk; j++)
                    msg[5 + j] = file_buf[offs + j];
                send(req.sender_pid, 100, msg, 5 + chunk);
                offs += chunk;
            }
        }
next: ;
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

static int read_sector(int drive, u32 lba, u8 *buf) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(13), "D"((u64)drive), "S"((u64)lba), "d"((u64)buf));
    return r;
}
