# μkrn

A minimal x86-64 microkernel (~2300 lines of C and assembly).

**Status:** v0.1a — alpha stage.

---

## Quick Start

### Build

```bash
bash build.sh
```

Requires: `gcc`, `nasm`, `ld`.

### Run

```bash
qemu-system-x86_64 -drive file=build/os_image.bin,format=raw,if=ide -serial stdio
```

Without display (headless):

```bash
qemu-system-x86_64 -drive file=build/os_image.bin,format=raw,if=ide -serial mon:stdio -display none -no-reboot -monitor none
```

---

## System Calls

All syscalls use `int $0x80`. Arguments in registers:

| Register | Purpose      |
|----------|--------------|
| RAX      | Syscall number |
| RDI      | arg1         |
| RSI      | arg2         |
| RDX      | arg3         |
| RCX      | arg4         |
| RAX      | Return value |

### Helper

```c
typedef unsigned long long u64;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4) {
    u64 r;
    __asm__ volatile("int $0x80"
        : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}
```

### Full List

| #  | Name         | Description                        | Args                          | Returns            |
|----|--------------|------------------------------------|-------------------------------|--------------------|
| 0  | exit         | Terminate current process          | —                             | —                  |
| 1  | getpid       | Get process ID                     | —                             | PID                |
| 2  | send         | Send IPC message                   | `target_pid, type, data, len` | 0 or -1            |
| 3  | recv         | Receive IPC message                | `msg_buf`                     | 0 or -1            |
| 4  | inb          | Read byte from I/O port            | `port`                        | value              |
| 5  | outb         | Write byte to I/O port             | `port, value`                 | —                  |
| 6  | spawn        | Spawn raw binary process           | `addr, size`                  | PID or -1          |
| 7  | wait         | Wait for specific child            | `pid`                         | 0 or -1            |
| 8  | wait_any     | Wait for any child                 | —                             | PID or 0           |
| 9  | getstate     | Get process state                  | `pid`                         | 0-3                |
| 10 | irq_register | Register for IRQ                   | `irq`                         | 0 or -1            |
| 11 | mmap         | Allocate memory pages              | `virt, size, flags`           | 0 or -1            |
| 12 | munmap       | Free memory pages                  | `virt, size`                  | 0 or -1            |
| 13 | kill         | Kill a process                     | `pid`                         | 0 or -1            |
| 14 | nanosleep    | Sleep for N nanoseconds            | `ns`                          | 0                  |
| 15 | getticks     | Timer ticks since boot (500 Hz)    | —                             | ticks              |
| 16 | debug_putc   | Write char to debug port (0xE9)    | `char`                        | —                  |
| 17 | shutdown     | Power off                          | —                             | —                  |
| 18 | mprotect     | Change page protections            | `virt, size, flags`           | 0 or -1            |
| 19 | brk          | Set/get program heap               | `addr` (0 = query)            | previous brk       |
| 20 | irq_ack      | Acknowledge IRQ                    | `irq`                         | —                  |
| 21 | clone        | Create thread in current process   | `flags, stack, entry, arg`    | PID or -1          |
| 22 | futex        | Fast userspace mutex               | `uaddr, op, val`              | 0, -1, or count    |
| 23 | mmap_phys    | Map physical memory to user space  | `virt, phys, size, flags`     | 0 or -1            |
| 24 | inw          | Read 16-bit word from I/O port     | `port`                        | value              |
| 25 | outw         | Write 16-bit word to I/O port      | `port, value`                 | —                  |
| 26 | spawn_at     | Spawn ELF64 process from memory    | `addr, size`                  | PID or -1          |
| 27 | yield        | Yield CPU voluntarily              | —                             | 0                  |

### States (getstate)

| Value | Meaning  |
|-------|----------|
| 0     | Exited   |
| 1     | Ready    |
| 2     | Running  |
| 3     | Blocked  |

---

## Writing a User Program

Programs are ELF64 binaries linked at `0x400000`. Entry point: `_start`.

### Hello World

```c
typedef unsigned long long u64;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4) {
    u64 r;
    __asm__ volatile("int $0x80"
        : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}

static void putc(char c) {
    while (!(sys(4, 0x3FD, 0, 0, 0) & 0x20));
    sys(5, 0x3F8, (u64)c, 0, 0);
}

static void puts(const char *s) {
    while (*s) putc(*s++);
}

void _start(void) {
    puts("Hello from ucrn!\n");
    sys(0, 0, 0, 0, 0);
}
```

### Build It

```bash
gcc -m64 -ffreestanding -nostdlib -nostartfiles -fno-PIE \
    -fno-asynchronous-unwind-tables -fno-stack-protector \
    -mno-red-zone -mgeneral-regs-only -c hello.c -o hello.o

ld -m elf_x86_64 -Ttext=0x400000 -o hello.elf hello.o
```

### Run It (via init)

1. Copy `hello.elf` to `user/`
2. Edit `user/init_procs.h`:

```c
static const spawn_entry_t spawn_list[] = {
    {"hello", _binary_hello_elf_start, _binary_hello_elf_end},
};
static const int spawn_count = 1;
```

3. Add to `build.sh`:

```bash
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    build/hello.elf build/hello_embed.o
```

4. Link `build/hello_embed.o` into the kernel binary.

### Run It (dynamic)

Use the interactive shell (see examples) and `spawn_at` syscall:

```c
extern char _binary_hello_elf_start[];
extern char _binary_hello_elf_end[];
u64 size = _binary_hello_elf_end - _binary_hello_elf_start;
u64 pid = sys(26, (u64)_binary_hello_elf_start, size, 0, 0);
```

---

## IPC (Inter-Process Communication)

Message passing with 16-slot queues per process. Non-blocking recv.

### Message Format

```c
typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;
```

### Example

```c
// Send
sys(2, target_pid, 42, (u64)"hello", 5);

// Receive (returns -1 if empty)
ipc_msg_t msg;
if (sys(3, (u64)&msg, 0, 0, 0) == 0) {
    // msg.sender_pid, msg.type, msg.data...
}
```

---

## Memory Layout

| Region      | Address    | Size              | Description          |
|-------------|------------|-------------------|----------------------|
| Kernel      | 0x100000   | varies (~33 KB)   | Kernel code + data   |
| Boot stack  | 0x90000    | 4 KB              | Initial stack        |
| User code   | 0x400000   | up to ~1 MB       | Program code         |
| User stack  | 0x4F0000   | 16 KB (4 pages)   | Grows down           |
| Heap        | 0x600000   | via brk           | Dynamic allocation   |

---

## SDK Examples

Located in `_sdk/examples/`:

| File          | Description                            |
|---------------|----------------------------------------|
| shell.c       | Interactive serial shell with test suite |
| fb_test.c     | VBE framebuffer demo (colored shapes)  |
| testchild.c   | IPC recv loop example                  |

To build and run the shell:

```bash
gcc -m64 -ffreestanding -nostdlib -nostartfiles -fno-PIE \
    -mno-red-zone -mgeneral-regs-only \
    -c _sdk/examples/shell.c -o build/shell.o

ld -m elf_x86_64 -Ttext=0x400000 -o build/shell.elf build/shell.o
```

Then add to `init_procs.h` or use the `spawn_at` syscall.

---

## Project Structure

```
.
├── boot/           # MBR and stage2 bootloader
│   ├── boot.asm
│   └── stage2.asm
├── kernel/         # Kernel source
│   ├── kernel.c    # Entry point
│   ├── idt.c       # Interrupts
│   ├── pmm.c       # Physical memory manager
│   ├── paging.c    # Virtual memory
│   ├── task.c      # Scheduler, IPC, syscalls
│   ├── serial.c    # Serial port driver
│   ├── syscall.c   # Syscall dispatcher
│   ├── gdt.c       # GDT/TSS
│   └── include/    # Headers
├── user/           # Init process
│   ├── init.c
│   └── init_procs.h
├── _sdk/           # User-space SDK
│   └── examples/   # Example programs
├── build.sh        # Build script
├── linker.ld       # Kernel linker script
└── README.md
```

---

## Boot Process

1. **MBR** reads stage2 from disk (LBA 1, 4 sectors)
2. **Stage2** sets up long mode, loads kernel, jumps to 0x100000
3. **Kernel** initializes IDT, PIC, PIT (500 Hz), PMM, paging, scheduler
4. **Init** (PID 1) spawns programs listed in `init_procs.h`
5. User programs run in ring 3 with isolated page tables

---

## License

MIT
