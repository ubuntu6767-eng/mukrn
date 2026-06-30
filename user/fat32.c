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
static int write_sector(int drive, u32 lba, u8 *buf);

static u16 bytes_per_sector;
static u8  sectors_per_cluster;
static u16 reserved_sectors;
static u8  num_fats;
static u32 sectors_per_fat;
static u32 root_cluster;
static u32 first_data_sector;

#define FAT32_DRIVE 1
#define EOC 0x0FFFFFF8

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

static int set_fat_entry(u32 c, u32 val)
{
    u32 off = c * 4;
    u32 fat_sec = reserved_sectors + off / bytes_per_sector;
    u32 in_off = off % bytes_per_sector;
    u8 sec[512];
    if (read_sector(FAT32_DRIVE, fat_sec, sec) < 0) return -1;
    *(u32*)&sec[in_off] = (*(u32*)&sec[in_off] & 0xF0000000) | (val & 0x0FFFFFFF);
    return write_sector(FAT32_DRIVE, fat_sec, sec);
}

static int is_eoc(u32 c)
{
    return c >= EOC;
}

static u32 alloc_cluster(void)
{
    u32 total_entries = sectors_per_fat * (bytes_per_sector / 4);
    for (u32 c = 2; c < total_entries; c++) {
        if (next_cluster(c) == 0) {
            if (set_fat_entry(c, EOC) < 0) return 0;
            return c;
        }
    }
    return 0;
}

static void free_cluster_chain(u32 start)
{
    u32 c = start;
    while (c >= 2 && !is_eoc(c)) {
        u32 next = next_cluster(c);
        set_fat_entry(c, 0);
        c = next;
    }
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

static int free_dir_entry(u32 dir_cluster)
{
    u32 c = dir_cluster;
    while (!is_eoc(c) && c >= 2) {
        u32 sec_base = cluster_to_sector(c);
        for (int s = 0; s < sectors_per_cluster; s++) {
            u8 sec[512];
            if (read_sector(FAT32_DRIVE, sec_base + s, sec) < 0) return -1;
            for (int off = 0; off < 512; off += 32) {
                u8 *e = sec + off;
                if (e[0] == 0 || e[0] == 0xE5) {
                    return write_sector(FAT32_DRIVE, sec_base + s, sec) < 0 ? -1 : off;
                }
            }
        }
        c = next_cluster(c);
    }
    return -1;
}

static int write_dir_entries(u32 dir_cluster, u32 entry_off, u32 sec_sector,
                             const u8 *entries, int n, int entry_size)
{
    u8 sec[512];
    if (read_sector(FAT32_DRIVE, sec_sector, sec) < 0) return -1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < entry_size; j++)
            sec[entry_off + i * 32 + j] = entries[i * 32 + j];
    return write_sector(FAT32_DRIVE, sec_sector, sec);
}

static int delete_entry(u32 dir_cluster, const u8 name[11])
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
                    u32 start_cluster = (*(u16*)&e[0x14] << 16) | *(u16*)&e[0x1A];
                    u32 sz = *(u32*)&e[0x1C];
                    e[0] = 0xE5;
                    if (write_sector(FAT32_DRIVE, sec_base + s, sec) < 0) return -1;
                    if (!(e[0xB] & 0x10))
                        free_cluster_chain(start_cluster);
                    return 0;
                }
            }
        }
        c = next_cluster(c);
    }
    return -1;
}

static int write_file_data(u32 start_cluster, const u8 *data, u32 size)
{
    u32 c = start_cluster;
    u32 off = 0;
    u32 prev_cluster = 0;
    while (off < size) {
        if (is_eoc(c) || c < 2) {
            u32 new_c = alloc_cluster();
            if (new_c == 0) return -1;
            if (prev_cluster && set_fat_entry(prev_cluster, new_c) < 0) return -1;
            if (prev_cluster == 0) start_cluster = new_c;
            c = new_c;
        }
        u32 sbase = cluster_to_sector(c);
        u32 to_write = size - off;
        if (to_write > bytes_per_sector * sectors_per_cluster)
            to_write = bytes_per_sector * sectors_per_cluster;
        u32 done = 0;
        for (int s = 0; s < sectors_per_cluster && done < to_write; s++) {
            u8 sec[512];
            u32 chunk = to_write - done;
            if (chunk > bytes_per_sector) chunk = bytes_per_sector;
            for (u32 i = 0; i < bytes_per_sector; i++)
                sec[i] = (i < chunk) ? data[off + done + i] : 0;
            if (write_sector(FAT32_DRIVE, sbase + s, sec) < 0) return -1;
            done += chunk;
        }
        off += done;
        prev_cluster = c;
        c = next_cluster(c);
    }
    return 0;
}

static int resolve_parent(const char *path, u32 *parent_cluster, const char **filename)
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
            if (find_in_dir(root_cluster, dir_83, &tmp_cluster, 0) != 1)
                return -1;
            *parent_cluster = tmp_cluster;
            *filename = path + i + 1;
            return 0;
        }
    }
    *parent_cluster = root_cluster;
    *filename = path;
    return 0;
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

        if (req.type == 0) {
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
        } else if (req.type == 1) {
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
        } else if (req.type == 2) {
            char *path = (char*)req.data;
            int path_len = 0;
            while (path[path_len]) path_len++;
            path_len++;
            int data_len = (int)req.length - path_len;
            if (data_len < 0) { send(req.sender_pid, 255, 0, 0); continue; }
            u32 parent_cluster;
            const char *filename;
            if (resolve_parent(path, &parent_cluster, &filename) < 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            u8 name_83[11];
            to_8_3(filename, name_83);
            u32 old_cluster, old_size;
            int exists = find_in_dir(parent_cluster, name_83, &old_cluster, &old_size);
            u32 data_start = data_len > 0 ? alloc_cluster() : 0;
            if (data_len > 0 && data_start == 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            if (data_len > 0 && write_file_data(data_start, req.data + path_len, data_len) < 0) {
                free_cluster_chain(data_start);
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            if (exists == 1) {
                u32 c = parent_cluster;
                int found = 0;
                while (!is_eoc(c) && c >= 2 && !found) {
                    u32 sec_base = cluster_to_sector(c);
                    for (int s = 0; s < sectors_per_cluster && !found; s++) {
                        u8 sec[512];
                        if (read_sector(FAT32_DRIVE, sec_base + s, sec) < 0) {
                            free_cluster_chain(data_start);
                            send(req.sender_pid, 255, 0, 0);
                            goto next;
                        }
                        for (int off = 0; off < 512; off += 32) {
                            u8 *e = sec + off;
                            if (e[0] == 0) { found = -1; break; }
                            if (e[0] == 0xE5) continue;
                            if (e[0xB] == 0x0F) continue;
                            if (e[0xB] & 0x08) continue;
                            int match = 1;
                            for (int i = 0; i < 11; i++)
                                if (e[i] != name_83[i]) { match = 0; break; }
                            if (match) {
                                free_cluster_chain(old_cluster);
                                *(u32*)&e[0x1C] = data_len;
                                e[0x14] = (data_start >> 16) & 0xFF;
                                e[0x15] = (data_start >> 24) & 0xFF;
                                e[0x1A] = data_start & 0xFF;
                                e[0x1B] = (data_start >> 8) & 0xFF;
                                if (write_sector(FAT32_DRIVE, sec_base + s, sec) < 0) {
                                    send(req.sender_pid, 255, 0, 0);
                                    goto next;
                                }
                                found = 1;
                            }
                        }
                    }
                    c = next_cluster(c);
                }
                if (found != 1) {
                    free_cluster_chain(data_start);
                    send(req.sender_pid, 255, 0, 0);
                    continue;
                }
            } else {
                int entry_off = free_dir_entry(parent_cluster);
                if (entry_off < 0) {
                    free_cluster_chain(data_start);
                    send(req.sender_pid, 255, 0, 0);
                    continue;
                }
                u8 entry[32];
                for (int i = 0; i < 32; i++) entry[i] = 0;
                for (int i = 0; i < 11; i++) entry[i] = name_83[i];
                entry[0xB] = 0x20;
                entry[0x1C] = data_len & 0xFF;
                entry[0x1D] = (data_len >> 8) & 0xFF;
                entry[0x1E] = (data_len >> 16) & 0xFF;
                entry[0x1F] = (data_len >> 24) & 0xFF;
                entry[0x1A] = data_start & 0xFF;
                entry[0x1B] = (data_start >> 8) & 0xFF;
                entry[0x14] = (data_start >> 16) & 0xFF;
                entry[0x15] = (data_start >> 24) & 0xFF;
                for (int i = 0; i < 4; i++) entry[0x10 + i] = 0; // ctime
                for (int i = 0; i < 4; i++) entry[0x12 + i] = 0; // cdate etc
                for (int i = 0; i < 4; i++) entry[0x16 + i] = 0; // atime/date
                u8 sec[512];
                u32 sbase = cluster_to_sector(parent_cluster);
                u32 entry_sec = sbase + (entry_off / 512);
                u32 entry_off_in_sec = entry_off % 512;
                if (read_sector(FAT32_DRIVE, entry_sec, sec) < 0) {
                    free_cluster_chain(data_start);
                    send(req.sender_pid, 255, 0, 0);
                    continue;
                }
                for (int i = 0; i < 32; i++)
                    sec[entry_off_in_sec + i] = entry[i];
                if (write_sector(FAT32_DRIVE, entry_sec, sec) < 0) {
                    free_cluster_chain(data_start);
                    send(req.sender_pid, 255, 0, 0);
                    continue;
                }
            }
            send(req.sender_pid, 200, 0, 0);
        } else if (req.type == 3 || req.type == 5) {
            char *path = (char*)req.data;
            u32 parent_cluster;
            const char *filename;
            if (resolve_parent(path, &parent_cluster, &filename) < 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            u8 name_83[11];
            to_8_3(filename, name_83);
            if (delete_entry(parent_cluster, name_83) < 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            send(req.sender_pid, 200, 0, 0);
        } else if (req.type == 4) {
            char *path = (char*)req.data;
            u32 parent_cluster;
            const char *dirname;
            if (resolve_parent(path, &parent_cluster, &dirname) < 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            u8 name_83[11];
            to_8_3(dirname, name_83);
            u32 new_cluster = alloc_cluster();
            if (new_cluster == 0) {
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            u8 sec[512];
            for (int i = 0; i < 512; i++) sec[i] = 0;
            sec[0] = '.'; sec[1] = ' ';
            sec[0x1B] = (new_cluster >> 8) & 0xFF;
            sec[0x1A] = new_cluster & 0xFF;
            sec[0x14] = (new_cluster >> 16) & 0xFF;
            sec[0x15] = (new_cluster >> 24) & 0xFF;
            sec[0xB] = 0x10;
            sec[32] = '.'; sec[33] = '.'; sec[34] = ' ';
            sec[32 + 0x1B] = (parent_cluster >> 8) & 0xFF;
            sec[32 + 0x1A] = parent_cluster & 0xFF;
            sec[32 + 0x14] = (parent_cluster >> 16) & 0xFF;
            sec[32 + 0x15] = (parent_cluster >> 24) & 0xFF;
            sec[32 + 0xB] = 0x10;
            u32 sbase = cluster_to_sector(new_cluster);
            if (write_sector(FAT32_DRIVE, sbase, sec) < 0) {
                free_cluster_chain(new_cluster);
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            int entry_off = free_dir_entry(parent_cluster);
            if (entry_off < 0) {
                free_cluster_chain(new_cluster);
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            u8 entry[32];
            for (int i = 0; i < 32; i++) entry[i] = 0;
            for (int i = 0; i < 11; i++) entry[i] = name_83[i];
            entry[0xB] = 0x10;
            entry[0x1A] = new_cluster & 0xFF;
            entry[0x1B] = (new_cluster >> 8) & 0xFF;
            entry[0x14] = (new_cluster >> 16) & 0xFF;
            entry[0x15] = (new_cluster >> 24) & 0xFF;
            u8 psec[512];
            u32 pbase = cluster_to_sector(parent_cluster);
            u32 psec_idx = pbase + (entry_off / 512);
            u32 poff_in_sec = entry_off % 512;
            if (read_sector(FAT32_DRIVE, psec_idx, psec) < 0) {
                free_cluster_chain(new_cluster);
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            for (int i = 0; i < 32; i++)
                psec[poff_in_sec + i] = entry[i];
            if (write_sector(FAT32_DRIVE, psec_idx, psec) < 0) {
                free_cluster_chain(new_cluster);
                send(req.sender_pid, 255, 0, 0);
                continue;
            }
            send(req.sender_pid, 200, 0, 0);
        } else {
            send(req.sender_pid, 255, 0, 0);
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

static int write_sector(int drive, u32 lba, u8 *buf) {
    int r;
    __asm__ volatile("int $0x80" : "=a"(r)
        : "a"(15), "D"((u64)drive), "S"((u64)lba), "d"((u64)buf));
    return r;
}
