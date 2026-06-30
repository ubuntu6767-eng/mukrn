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
static void sys_exit(void);

#define FAT32_PID 7
#define FAT32_TYPE_READ 0
#define FAT32_TYPE_SPAWN 1
#define FAT32_CHUNK 100
#define FAT32_PID_RESP 101
#define FAT32_ERR 255

static void send_str(u64 pid, const char *s)
{
    while (*s) {
        int i;
        for (i = 0; i < 63 && s[i]; i++);
        send(pid, 2, (const u8*)s, i);
        s += i;
    }
}

void _start(void)
{
    for (;;) {
        ipc_msg_t msg;
        while (recv(&msg) != 0)
            __asm__ volatile("pause");

        if (msg.type != 1) continue;

        u64 shell = msg.sender_pid;
        char *cmd = (char*)msg.data;

        if (cmd[0] == 'h') {
            send_str(shell, "Commands:\r\n  help\r\n  readfile <path>\r\n  spawn <path>\r\n  clear\r\n  exit\r\n");
        } else if (cmd[0] == 'c') {
            send_str(shell, "\x1b[2J\x1b[H");
        } else if (cmd[0] == 'e') {
            send_str(shell, "Bye!\r\n");
            send(shell, 3, (const u8*)"", 1);
            sys_exit();
        } else if (cmd[0] == 'r' && cmd[1] == 'e') {
            char *path = cmd + 9;
            while (*path == ' ') path++;
            send(FAT32_PID, FAT32_TYPE_READ, (const u8*)path, 63);
            char out[64];
            int olen = 0;
            for (;;) {
                ipc_msg_t resp;
                while (recv(&resp) != 0 || resp.sender_pid != FAT32_PID)
                    __asm__ volatile("pause");
                if (resp.type == FAT32_ERR) {
                    send_str(shell, "Error\r\n");
                    break;
                }
                if (resp.type != FAT32_CHUNK) break;
                u32 chunk_sz = resp.data[4];
                for (u32 i = 0; i < chunk_sz && olen < 60; i++)
                    out[olen++] = resp.data[5 + i];
                if (chunk_sz < 59) {
                    out[olen] = 0;
                    send_str(shell, out);
                    break;
                }
            }
        } else if (cmd[0] == 's') {
            char *path = cmd + 6;
            while (*path == ' ') path++;
            send(FAT32_PID, FAT32_TYPE_SPAWN, (const u8*)path, 63);
            ipc_msg_t resp;
            while (recv(&resp) != 0 || resp.sender_pid != FAT32_PID)
                __asm__ volatile("pause");
            if (resp.type == FAT32_ERR) {
                send_str(shell, "Spawn error\r\n");
            } else if (resp.type == FAT32_PID_RESP) {
                u64 pid = 0;
                for (int i = 7; i >= 0; i--)
                    pid = (pid << 8) | resp.data[i];
                send_str(shell, "Spawned PID ");
                char hex[17];
                for (int i = 15; i >= 0; i--) {
                    int n = pid & 0xF;
                    hex[i] = n < 10 ? '0' + n : 'a' + n - 10;
                    pid >>= 4;
                }
                hex[16] = 0;
                int z = 0;
                while (hex[z] == '0' && z < 15) z++;
                send_str(shell, hex + z);
                send_str(shell, "\r\n");
            }
        } else {
            send_str(shell, "Unknown\r\n");
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

static void sys_exit(void) {
    __asm__ volatile("int $0x80" : : "a"(0));
}
