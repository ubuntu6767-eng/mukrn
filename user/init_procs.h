// Process spawn list for init
// Add new processes by declaring their .incbin and adding to spawn_list

typedef struct {
    const char *name;
    const unsigned char *addr;
    const unsigned char *end;
} spawn_entry_t;

static const spawn_entry_t spawn_list[] = {
    //
    // Example:
    // extern const u8 mydriver_bin[];
    // extern const u8 mydriver_bin_end[];
    // { "mydriver", mydriver_bin, mydriver_bin_end },
};

static const int spawn_count = sizeof(spawn_list) / sizeof(spawn_list[0]);
