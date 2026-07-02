typedef struct {
    const char *name;
    const unsigned char *addr;
    const unsigned char *end;
} spawn_entry_t;

extern const unsigned char _binary_thick_line_elf_start[];
extern const unsigned char _binary_thick_line_elf_end[];

static const spawn_entry_t spawn_list[] = {
    {"thick_line", _binary_thick_line_elf_start, _binary_thick_line_elf_end},
};

static const int spawn_count = 1;
