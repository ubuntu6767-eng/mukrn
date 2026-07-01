typedef struct {
    const char *name;
    const unsigned char *addr;
    const unsigned char *end;
} spawn_entry_t;

__asm__(".section .rodata\n"
    ".globl shell_bin\n"
    "shell_bin:\n"
    ".incbin \"build/shell.elf\"\n"
    ".globl shell_bin_end\n"
    "shell_bin_end:\n"
    ".byte 0\n"
    ".previous\n");

extern const unsigned char shell_bin[];
extern const unsigned char shell_bin_end[];

static const spawn_entry_t spawn_list[] = {
    { "shell", shell_bin, shell_bin_end },
};

static const int spawn_count = sizeof(spawn_list) / sizeof(spawn_list[0]);
