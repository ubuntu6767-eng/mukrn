// Process spawn list for init
// Add new processes by declaring their .incbin and adding to spawn_list

typedef struct {
    const char *name;
    const unsigned char *addr;
    const unsigned char *end;
} spawn_entry_t;

extern const unsigned char _binary_cursor_elf_start[];
extern const unsigned char _binary_cursor_elf_end[];

static const spawn_entry_t spawn_list[] = {
    {"cursor", _binary_cursor_elf_start, _binary_cursor_elf_end},
};

static const int spawn_count = 1;
