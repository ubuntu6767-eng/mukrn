// Process spawn list for init
// Add new processes by declaring their .incbin and adding to spawn_list

typedef struct {
    const char *name;
    const unsigned char *addr;
    const unsigned char *end;
} spawn_entry_t;

static const spawn_entry_t spawn_list[] = {
    //
};

static const int spawn_count = sizeof(spawn_list) / sizeof(spawn_list[0]);
