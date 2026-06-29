#ifndef PMM_H
#define PMM_H

#include "io.h"

#define PAGE_SIZE 4096

typedef struct {
    u64 base;
    u64 len;
    u32 type;
    u32 attrs;
} __attribute__((packed)) e820_entry_t;

void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);

#endif
