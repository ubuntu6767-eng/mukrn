#include "paging.h"
#include "pmm.h"
#include "serial.h"

u64 paging_root = 0x1000;
static u64 kernel_pml4_template = 0x1000;

void paging_init(void)
{
    puts("[kernel] Paging ready\r\n");
}

void paging_switch(u64 pml4_phys)
{
    paging_root = pml4_phys;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

u64 paging_clone_kernel(void)
{
    u64 *src_pml4 = (u64*)kernel_pml4_template;

    u64 pml4 = (u64)pmm_alloc_page();
    u64 pdpt = (u64)pmm_alloc_page();
    u64 pdt  = (u64)pmm_alloc_page();
    if (!pml4 || !pdpt || !pdt) {
        puts("[kernel] paging_clone: OOM\r\n");
        return 0;
    }

    u64 *dst_pml4 = (u64*)pml4;
    u64 *dst_pdpt = (u64*)pdpt;
    u64 *dst_pdt  = (u64*)pdt;

    dst_pml4[0] = pdpt | (src_pml4[0] & 0xFFF) | PAGE_USER;

    u64 *src_pdpt = (u64*)(src_pml4[0] & ~0xFFF);
    dst_pdpt[0] = pdt | (src_pdpt[0] & 0xFFF) | PAGE_USER;

    u64 *src_pdt = (u64*)(src_pdpt[0] & ~0xFFF);
    for (int i = 0; i < 512; i++)
        dst_pdt[i] = src_pdt[i] | PAGE_USER;

    return pml4;
}

void map_page(u64 virt, u64 phys, u64 flags)
{
    u64 pml4_i = (virt >> 39) & 0x1FF;
    u64 pdpt_i = (virt >> 30) & 0x1FF;
    u64 pdt_i  = (virt >> 21) & 0x1FF;
    u64 pt_i   = (virt >> 12) & 0x1FF;

    u64 *pml4 = (u64*)paging_root;

    u64 pml4e = pml4[pml4_i];
    if (!(pml4e & 1)) {
        void *page = pmm_alloc_page();
        if (!page) return;
        pml4[pml4_i] = (u64)page | 3;
        pml4e = pml4[pml4_i];
    }
    u64 *pdpt = (u64*)(pml4e & ~0xFFF);

    u64 pdpte = pdpt[pdpt_i];
    if (!(pdpte & 1)) {
        void *page = pmm_alloc_page();
        if (!page) return;
        pdpt[pdpt_i] = (u64)page | 3;
        pdpte = pdpt[pdpt_i];
    }
    u64 *pdt = (u64*)(pdpte & ~0xFFF);

    u64 pde = pdt[pdt_i];
    if ((virt & 0x1FFFFF) == 0 && (phys & 0x1FFFFF) == 0) {
        pdt[pdt_i] = phys | flags | PAGE_PS;
        return;
    }

    if (!(pde & 1)) {
        void *page = pmm_alloc_page();
        if (!page) return;
        pdt[pdt_i] = (u64)page | 7;
        pde = pdt[pdt_i];
    } else if (pde & PAGE_PS) {
        puts("[kernel] split "); puthex(virt); puts("->"); puthex(phys);
        puts(" cr3="); puthex(paging_root);
        void *page = pmm_alloc_page();
        if (!page) { puts(" OOM\n"); return; }
        u64 base = pde & ~0x1FFFFF;
        u64 *pt = (u64*)page;
        for (int i = 0; i < 512; i++)
            pt[i] = (base + i * 0x1000) | (pde & 0x1FF);
        pdt[pdt_i] = (u64)page | 7;
        pde = pdt[pdt_i];
        puts(" pt="); puthex((u64)page);
        puts(" pde="); puthex(pde); putc('\n');
    }
    u64 *pt = (u64*)(pde & ~0xFFF);

    pt[pt_i] = phys | flags;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

void unmap_page(u64 virt)
{
    u64 pml4_i = (virt >> 39) & 0x1FF;
    u64 pdpt_i = (virt >> 30) & 0x1FF;
    u64 pdt_i  = (virt >> 21) & 0x1FF;
    u64 pt_i   = (virt >> 12) & 0x1FF;

    u64 *pml4 = (u64*)paging_root;
    if (!(pml4[pml4_i] & 1)) return;

    u64 *pdpt = (u64*)(pml4[pml4_i] & ~0xFFF);
    if (!(pdpt[pdpt_i] & 1)) return;

    u64 pde = pdpt[pdpt_i];
    if (!(pde & 1)) return;

    if (pde & PAGE_PS) {
        pdpt[pdpt_i] = pde & ~1ULL;
        __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
        return;
    }

    u64 *pdt = (u64*)(pde & ~0xFFF);
    if (pdt[pdt_i] & PAGE_PS) {
        pdt[pdt_i] &= ~1ULL;
        __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
        return;
    }

    u64 *pt = (u64*)(pdt[pdt_i] & ~0xFFF);
    pt[pt_i] &= ~1ULL;
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
