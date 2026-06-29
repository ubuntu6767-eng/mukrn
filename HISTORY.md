# sborchikOS Development History

## Phase 1 - Initial x86-64 bootloader + kernel
- boot.asm (MBR, LBA), stage2.asm (real→PM→long mode)
- C kernel with IDT/PIC/PIT (100Hz timer)
- PMM (page frame allocator via E820)
- Paging (map/unmap with 2MB→4KB split)

## Phase 2 - Ring 3 user space + processes + shell
Goal: syscalls, ring 3 processes, init, shell

### Changes made:
- GDT: Added ring 3 code/data segments (0x20/0x28), TSS placeholder (0x30)
- stage2.asm: Added USER bit to PML4/PDPT entries (0x2007, 0x3007)
- gdt.c: TSS setup with ring 0 stack for ring 3→0 transitions
- task.h/c: Task struct, create_process, scheduler_start
- idt.h: Updated registers_t struct to match push order, added isr128
- idt.c: Syscall handler at int 0x80 (trap gate DPL=3), schedule() in timer
- isr_stubs.asm: isr_handler returns new RSP for task switching
- keyboard.h/c: PS/2 keyboard driver with ring buffer
- syscall.h/c: Syscall dispatch (putc, puts, read, exit)
- shell.c: User-space shell with syscall stubs (help/clear/exit)
- build.sh: Compile shell as flat binary, embed in kernel via objcopy
- paging.c: Fixed PDE split to preserve USER bit (7 instead of 3)
- paging.c: Added invlpg after map_page

### Current bugs:
- Page fault at 0x500000 (USER_STACK_TOP) when entering shell
- Err=5 (protection violation, user mode, read)
