# sborchikOS Development History

## Phase 1 - Initial x86-64 bootloader + kernel
- boot.asm (MBR, LBA), stage2.asm (real→PM→long mode)
- C kernel with IDT/PIC/PIT (100Hz timer)
- PMM (page frame allocator via E820)
- Paging (map/unmap with 2MB→4KB split)

## Phase 2 - Ring 3 user space + processes + shell

### Debug timeline:
1. GDT: Added ring 3 code/data (0x20/0x28), TSS placeholder (0x30)
2. stage2: Added USER bit to PML4/PDPT entries (0x2007/0x3007)
3. paging.c: Fixed PDE split to preserve USER bit (7 instead of 3)
4. paging.c: Added invlpg after map_page
5. gdt.c: TSS with ring 0 stack for ring 3→0 transitions
6. task.c: create_process, scheduler_start (iretq to ring 3)
7. Bug: shell binary linked at 0x0 but runs at 0x400000 → wrong absolute addrs
   Fix: link at 0x400000
8. Bug: shell functions in wrong order (putc first, _start at offset 0x7C)
   Fix: put _start first in source with forward declarations
9. Bug: stack only 1 page, overflowed into unmapped memory
   Fix: map 4 pages for user stack
10. Bug: shell waits for PS/2 keyboard, no serial input in -nographic
    Fix: SYSCALL_READ reads from both keyboard and serial (0x3F8)
11. Bug: kb_head/kb_tail static in keyboard.c, not accessible from syscall.c
    Fix: added kb_available() accessor

### Working state:
- Full boot chain: boot.asm → stage2 → kernel → ring 3 shell
- Syscalls: putc, puts, read (keyboard+serial), exit
- Shell commands: help, clear, exit
- TSS for ring 3→0 transitions (syscall via int 0x80)
- Keyboard IRQ + serial polling for input

### TODO:
- Multiple process support with timer preemption
- Init process spawning multiple programs
- Filesystem / disk access from user space
- More shell commands (run, ps, etc.)
