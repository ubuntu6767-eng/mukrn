#ifndef PAGING_H
#define PAGING_H

#include "io.h"

#define PAGE_PRESENT  1
#define PAGE_WRITABLE 2
#define PAGE_USER     4
#define PAGE_PS       (1 << 7)

extern u64 paging_root;

void paging_init(void);
void map_page(u64 virt, u64 phys, u64 flags);
void unmap_page(u64 virt);
void paging_switch(u64 pml4_phys);
u64  paging_clone_kernel(void);

#endif
