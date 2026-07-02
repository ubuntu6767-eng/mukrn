# μkrn

A minimal x86-64 microkernel in ~2300 lines of C and assembly.

**Status:** v0.2a — alpha stage. Everything works but expect rough edges.

## Quick Start

### Build

```bash
bash build.sh
```

Requires `gcc`, `nasm`, `ld`. The output is `build/os_image.bin`.

### Run

```bash
qemu-system-x86_64 -drive file=build/os_image.bin,format=raw,if=ide -serial stdio
```

Headless mode (no display window):

```bash
qemu-system-x86_64 -drive file=build/os_image.bin,format=raw,if=ide -serial mon:stdio -display none -no-reboot -monitor none
```

On real hardware, write `os_image.bin` to a USB drive:

```bash
dd if=build/os_image.bin of=/dev/sdX bs=1M
```

## System Calls

All syscalls use `int $0x80`. Arguments are passed in registers and the return value comes back in RAX.

| Register | Purpose |
|----------|---------|
| RAX | Syscall number on entry, return value on exit |
| RDI | arg1 |
| RSI | arg2 |
| RDX | arg3 |
| RCX | arg4 |

### Syscall Helper

Put this at the top of every user program:

```c
typedef unsigned long long u64;
typedef unsigned char u8;
typedef unsigned short u16;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
    u64 r;
    __asm__ volatile("int $0x80"
        : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "c"(a4));
    return r;
}
```

### 0 — exit

Terminate the current process. Memory is freed, the parent process (if waiting) is woken up.

```c
void _start(void)
{
    // ... do work ...
    sys(0, 0, 0, 0, 0);  // goodbye
}
```

### 1 — getpid

Return the current process ID. PIDs are positive integers starting from 1 (init).

```c
u64 my_pid = sys(1, 0, 0, 0, 0);
// my_pid == 42, for example
```

### 2 — send

Send an IPC message to another process. Up to 64 bytes of data. Returns 0 on success, -1 if the target doesn't exist or its queue is full (16 messages max).

```c
u64 target = 1;  // send to init
char *data = "hello";
int ok = sys(2, target, 42, (u64)data, 5);
// ok == 0 means delivered
```

The message has a numeric `type` field (42 in the example) that the receiver can use to distinguish messages.

### 3 — recv

Receive an IPC message. Returns 0 and fills the buffer, or -1 if no message is waiting (non-blocking).

```c
typedef struct {
    u64 sender_pid;
    u64 type;
    u8 data[64];
    u64 length;
} ipc_msg_t;

ipc_msg_t msg;
int r = sys(3, (u64)&msg, 0, 0, 0);
if (r == 0) {
    // msg.sender_pid — who sent it
    // msg.type      — message type
    // msg.data      — up to 64 bytes
    // msg.length    — actual data length
}
```

Since recv is non-blocking, you typically poll in a loop:

```c
ipc_msg_t msg;
while (sys(3, (u64)&msg, 0, 0, 0) == -1) {
    sys(27, 0, 0, 0, 0);  // yield CPU
}
// got a message
```

### 4 — inb

Read a byte from an I/O port. Used for hardware access.

```c
u8 status = sys(4, 0x3FD, 0, 0, 0);  // read serial line status
int data_ready = status & 1;
int tx_empty   = status & 0x20;
```

### 5 — outb

Write a byte to an I/O port.

```c
sys(5, 0x3F8, 'A', 0, 0);  // write 'A' to serial port
```

The standard serial port (COM1) is at 0x3F8. The line status register (LSR) at 0x3FD tells you when it's safe to read or write.

### 7 — wait

Wait for a specific child process to exit. If the child has already exited, returns immediately. Otherwise blocks until the child exits.

```c
u64 child_pid = sys(26, (u64)elf_addr, elf_size, 0, 0);
sys(7, child_pid, 0, 0, 0);  // wait for it
puts("child exited\n");
```

Returns 0 on success (or block), -1 if no process with that PID exists.

### 8 — wait_any

Wait for any child process. Returns the PID of an exited child, or 0 if none exist yet (blocks until one exits).

```c
u64 exited_pid = sys(8, 0, 0, 0, 0);
// some child exited
```

### 9 — getstate

Check the state of any process by PID.

```c
int state = sys(9, pid, 0, 0, 0);
```

State values:

| Value | Meaning |
|-------|---------|
| 0 | Exited |
| 1 | Ready |
| 2 | Running |
| 3 | Blocked |
| -1 | No such PID |

### 10 — irq_register

Register the current process to receive IPC messages for a hardware IRQ. Only IRQ 1 (keyboard) currently sends messages.

```c
sys(10, 1, 0, 0, 0);  // register for keyboard IRQ
```

When a key is pressed, an IPC message with type=0 and one byte of data (the scancode) is delivered to the registered process.

### 11 — mmap

Allocate memory pages at a specific virtual address. The pages have user-space read/write access.

```c
int ok = sys(11, 0x700000, 4096, 2, 0);
// ok == 0 means 1 page (4096 bytes) mapped at 0x700000
// flags=2 means writable
```

Flags: bit 1 (value 2) = writable.

### 12 — munmap

Unmap memory pages. Removes the page table entries.

```c
sys(12, 0x700000, 4096, 0, 0);
```

### 13 — kill

Kill a process by PID. Frees its memory and marks it as exited.

```c
int ok = sys(13, pid, 0, 0, 0);
// ok == 0 means killed, -1 means no such process
```

### 14 — nanosleep

Sleep for at least N nanoseconds. The PIT timer runs at 500 Hz (one tick every 2,000,000 ns). Minimum meaningful sleep is 2,000,000 ns.

```c
sys(14, 20000000, 0, 0, 0);  // sleep ~20 ms (10 ticks)
```

The actual sleep duration is rounded up to the next tick boundary. Sleeping for less than 2,000,000 ns may never wake up.

### 15 — getticks

Return the number of timer ticks since boot. At 500 Hz, each tick is 2 ms.

```c
u64 t0 = sys(15, 0, 0, 0, 0);
sys(14, 10000000, 0, 0, 0);  // sleep 10 ms
u64 t1 = sys(15, 0, 0, 0, 0);
u64 elapsed = t1 - t0;  // about 5 ticks
```

### 16 — debug_putc

Write a character to the QEMU/Bochs debug console at I/O port 0xE9.

```c
sys(16, '!', 0, 0, 0);  // appears in qemu -debugcon
```

### 17 — shutdown

Power off the system (QEMU). Uses the Bochs/QEMU ACPI shutdown port.

```c
sys(17, 0, 0, 0, 0);  // goodbye
```

### 18 — mprotect

Change the protection flags of existing mapped pages.

```c
sys(18, 0x400000, 4096, 1, 0);  // make first page read-only
```

Flags: bit 0 (value 1) = writable.

### 19 — brk

Set or query the program heap break. The heap starts at 0x600000. Memory is allocated automatically in page increments.

```c
u64 current_brk = sys(19, 0, 0, 0, 0);          // query
u64 old_brk = sys(19, current_brk + 4096, 0, 0, 0);  // extend
```

Returns the previous break value.

### 20 — irq_ack

Acknowledge an IRQ. Sends the End-Of-Interrupt signal to the PIC.

```c
sys(20, irq_number, 0, 0, 0);
```

Must be called after handling an IRQ for which you registered.

### 21 — clone

Create a new thread that shares the current process's address space. The new thread starts executing at `entry` with `arg` as its first argument and uses `user_stack` as its stack pointer.

```c
static u64 thread_func(u64 arg)
{
    // arg is whatever was passed
    return 0;
}

void _start(void)
{
    u64 stack = 0x7F0000;  // some free address
    sys(11, stack, 4096, 2, 0);  // allocate stack page
    u64 child_pid = sys(21, 0, stack + 4096, (u64)thread_func, 42);
    // child_pid is the new thread's PID
}
```

### 22 — futex

Fast userspace mutex. Two operations:

```c
u32 futex_word = 0;  // in shared memory

// FUTEX_WAIT (op=0): block while *uaddr == val
sys(22, (u64)&futex_word, 0, 0, 0);  // waits if futex_word == 0

// FUTEX_WAKE (op=1): wake up to `val` waiters
int woken = sys(22, (u64)&futex_word, 1, 1, 0);  // wake 1 waiter
```

### 23 — mmap_phys

Map physical memory into the process's address space. Used for memory-mapped hardware (framebuffer, MMIO).

```c
// Map VBE framebuffer (physical 0xFD000000) at virtual 0xFD00000
sys(23, 0xFD00000, 0xFD000000, 640*480*4, 6);
// flags=6 means writable (2) + cache disable (4)
```

Flags: bit 1 (value 2) = writable, bit 2 (value 4) = cache disable.

### 24 — inw

Read a 16-bit word from an I/O port.

```c
u16 v = sys(24, 0x1CE, 0, 0, 0);  // read from VBE index port
```

### 25 — outw

Write a 16-bit word to an I/O port.

```c
sys(25, 0x1CE, 0xB0C5, 0, 0);  // VBE: enable extended registers
```

### 26 — spawn_at

Load an ELF64 binary from memory and create a new process with its own page tables. Returns the new PID, or -1 on failure.

```c
extern char _binary_myapp_elf_start[];
extern char _binary_myapp_elf_end[];
u64 size = _binary_myapp_elf_end - _binary_myapp_elf_start;
u64 pid = sys(26, (u64)_binary_myapp_elf_start, size, 0, 0);
if (pid > 0) {
    // child is running
}
```

The binary must be a statically linked ELF64 file with entry point `_start` and linked at 0x400000.

### 27 — yield

Voluntarily yield the CPU to another process. Returns when the scheduler gives back control.

```c
while (some_condition) {
    // wait for something
    sys(27, 0, 0, 0, 0);
}
```

## Serial I/O

Since there is no filesystem driver in v0.1a, all user interaction goes through the serial port (COM1). These are the ports you need:

| Port | Purpose | Read | Write |
|------|---------|------|-------|
| 0x3F8 | Data register | Read byte | Write byte |
| 0x3FD | Line status register | Bit 0 = data ready, bit 5 = tx empty | — |

### putc — write one character

```c
static void putc(char c)
{
    while (!(sys(4, 0x3FD, 0, 0, 0) & 0x20));  // wait for tx empty
    sys(5, 0x3F8, (u64)c, 0, 0);
}
```

Wait for the transmitter holding register to be empty (bit 5 at 0x3FD), then write the byte.

### puts — write a string

```c
static void puts(const char *s)
{
    while (*s) putc(*s++);
}
```

### getc — read one character

```c
static u8 getc(void)
{
    while (!(sys(4, 0x3FD, 0, 0, 0) & 1));  // wait for data ready
    return (u8)sys(4, 0x3F8, 0, 0, 0);
}
```

Wait for data to arrive (bit 0 at 0x3FD), then read the byte.

### putdec — print a decimal number

```c
static void putdec(u64 v)
{
    char buf[20];
    int i = 0;
    if (v == 0) { putc('0'); return; }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while (i > 0) putc(buf[--i]);
}
```

### puthex — print a hex number

```c
static void puthex(u64 v)
{
    for (int i = 15; i >= 0; i--) {
        int d = (v >> (i * 4)) & 0xF;
        putc(d < 10 ? '0' + d : 'a' + d - 10);
    }
}
```

### read_num — read a decimal number

```c
static int read_num(void)
{
    int n = 0;
    char c;
    while (1) {
        c = getc();
        if (c >= '0' && c <= '9') { n = n * 10 + (c - '0'); putc(c); }
        else if (c == '\r' || c == '\n') { putc('\n'); return n; }
    }
}
```

## Framebuffer (VBE)

The bootloader enables VBE with a 640×480×32bpp mode. The linear framebuffer is at physical address 0xFD000000.

To draw pixels from a user program you must (1) set the VBE mode registers, (2) map the framebuffer, then (3) write to it.

```c
// Helper: write to a VBE register (index = 0x1CE, data = 0x1CF)
#define VI 0x1CE
#define VD 0x1CF
static void vw(u16 i, u16 v) { sys(25, VI, i, 0, 0); sys(25, VD, v, 0, 0); }

// Set up VBE mode
vw(0, 0xB0C5);         // enable VBE extended registers
vw(4, 0);              // display start = 0
vw(1, 640);            // X resolution
vw(2, 480);            // Y resolution
vw(3, 32);             // bits per pixel
vw(4, 0x41);           // bit 0 = LFB enabled

// Map the framebuffer into user space
// Virtual 0xFD00000, Physical 0xFD000000, 640*480*4 bytes, flags=6 (writable+no cache)
sys(23, 0xFD00000, 0xFD000000, 640*480*4, 6);

// Draw a pixel
volatile u32 *fb = (u32*)0xFD00000;
fb[y * 640 + x] = 0x00RRGGBB;  // 32-bit color
```

A simplified 3-line version (enable + display start) does *not* set the resolution and will leave the screen in a wrong mode — always use the full 6-line sequence above.

### Draw a square

```c
void _start(void)
{
    vw(0, 0xB0C5);
    vw(4, 0);
    vw(1, 640);
    vw(2, 480);
    vw(3, 32);
    vw(4, 0x41);
    sys(23, 0xFD00000, 0xFD000000, 640*480*4, 6);

    volatile u32 *fb = (u32*)0xFD00000;

    // Red 100x100 square in the center
    int sx = (640 - 100) / 2;
    int sy = (480 - 100) / 2;
    for (int y = sy; y < sy + 100; y++)
        for (int x = sx; x < sx + 100; x++)
            fb[y * 640 + x] = 0x00FF0000;  // red

    for (;;) __asm__ volatile("pause");
}
```

### Animation gotcha

When moving a shape across the framebuffer, check bounds **before** updating the position, not after. If `y` goes negative before you flip the direction, `fb[y * 640 + x]` will page‑fault (accessing memory before the framebuffer start at 0xFD00000).

```c
// Correct
if (dy > 0 && y + dy + H > 480) dy = -dy;
if (dy < 0 && y + dy < 0) dy = -dy;
y += dy;

// Wrong — y can underflow to -1 and crash
// y += dy;
// if (y < 0) dy = -dy;
```

## Writing a User Program

### Minimal Hello World

```c
typedef unsigned long long u64;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
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
    puts("Hello from my program!\n");
    sys(0, 0, 0, 0, 0);  // exit
}
```

### Build

```bash
gcc -m64 -ffreestanding -nostdlib -nostartfiles -fno-PIE \
    -fno-asynchronous-unwind-tables -fno-stack-protector \
    -mno-red-zone -mgeneral-regs-only -c myapp.c -o myapp.o

ld -m elf_x86_64 -Ttext=0x400000 -o myapp.elf myapp.o
```

### Embed into the kernel

1. Write your program and save it as `user/myapp.c`.

2. Edit `user/init_procs.h`:

```c
extern const unsigned char _binary_myapp_elf_start[];
extern const unsigned char _binary_myapp_elf_end[];

static const spawn_entry_t spawn_list[] = {
    {"myapp", _binary_myapp_elf_start, _binary_myapp_elf_end},
};
static const int spawn_count = 1;
```

3. Add the build and embed steps to `build.sh` **before** the init embedding block:

```bash
echo "=== Building myapp ==="
gcc $CFLAGS -c user/myapp.c -o build/myapp_user.o
ld -m elf_x86_64 -Ttext=0x400000 -o build/myapp.elf build/myapp_user.o

echo "=== Embedding programs ==="
cd build
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    myapp.elf myapp_embed.o
```

4. Link the embed object **into init.elf** (not the kernel!), right before the `objcopy` that creates `init_embed.o`:

```bash
ld -m elf_x86_64 -Ttext=0x400000 -o init.elf \
    ../build/init_user.o myapp_embed.o
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    init.elf init_embed.o
```

The `_binary_*` symbols must be visible to init.elf (user-space), so the embed.o must be linked there — adding it to the kernel link command will not make them available to init.

5. Rebuild:

```bash
bash build.sh
```

### Dynamic loading via shell

You cannot dynamically load a file from disk yet (no filesystem driver). Use init embedding for now.

## Guess the Number — Complete Example

```c
typedef unsigned long long u64;
typedef unsigned char u8;

static u64 sys(u64 n, u64 a1, u64 a2, u64 a3, u64 a4)
{
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

static u8 getc(void) {
    while (!(sys(4, 0x3FD, 0, 0, 0) & 1));
    return (u8)sys(4, 0x3F8, 0, 0, 0);
}

static void putdec(u64 v) {
    char buf[20];
    int i = 0;
    if (v == 0) { putc('0'); return; }
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i > 0) putc(buf[--i]);
}

static int read_num(void) {
    int n = 0;
    char c;
    while (1) {
        c = getc();
        if (c >= '0' && c <= '9') { n = n * 10 + (c - '0'); putc(c); }
        else if (c == '\r' || c == '\n') { putc('\n'); return n; }
    }
}

void _start(void)
{
    u64 ticks = sys(15, 0, 0, 0, 0);
    int secret = (int)(ticks % 100) + 1;
    int guess, attempts = 0;

    puts("\n=== Guess the Number ===\n");
    puts("I picked a number (1-100). Guess it!\n");

    while (1) {
        puts("> ");
        guess = read_num();
        attempts++;

        if (guess < secret)
            puts("Too low!\n");
        else if (guess > secret)
            puts("Too high!\n");
        else {
            puts("Correct! ");
            putdec(attempts);
            puts(" attempts.\n");
            break;
        }
    }

    sys(0, 0, 0, 0, 0);
}
```

## Memory Layout

| Region | Start | Size | Description |
|--------|-------|------|-------------|
| Kernel code | 0x100000 | ~33 KB | Kernel binary loaded by bootloader |
| Boot stack | 0x90000 | 4 KB | Initial stack for bootloader and kernel setup |
| PIC/IO | 0x20-0xA0 | — | Programmable interrupt controller |
| PIT | 0x40-0x43 | — | Programmable interval timer (500 Hz) |
| Serial | 0x3F8-0x3FD | — | COM1 serial port |
| User code | 0x400000 | up to ~1 MB | Program code and data |
| User stack | 0x4F0000 | 16 KB (4 pages) | Stack grows downward |
| Heap | 0x600000 | variable | Dynamic allocation via brk |
| User pages | 0x700000+ | variable | Additional pages via mmap |
| VBE LFB | 0xFD000000 | 640*480*4 = 1.2 MB | Framebuffer physical address |

## SDK Examples

Located in `_sdk/examples/`. Each is a complete user-space program.

| File | Description |
|------|-------------|
| shell.c | Interactive serial shell with 14-syscall test suite. Commands: help, pid, ticks, test, echo, exit, shutdown |
| fb_test.c | VBE framebuffer demo — draws colored shapes (stripes, cross, bars) |
| testchild.c | IPC recv polling loop — waits for a message from init and exits |

### Build an example

```bash
gcc -m64 -ffreestanding -nostdlib -nostartfiles -fno-PIE \
    -mno-red-zone -mgeneral-regs-only \
    -c _sdk/examples/shell.c -o shell.o

ld -m elf_x86_64 -Ttext=0x400000 -o shell.elf shell.o
```

Then embed via `init_procs.h` as described above.

## Project Structure

```
boot/           — Bootloader
  boot.asm      — MBR sector (loads stage2 via BIOS)
  stage2.asm    — Stage2: long mode setup, kernel loading

kernel/         — Kernel source
  kernel.c      — Entry point: init everything, start scheduler
  idt.c         — Interrupts, PIT, scheduler, IRQ handlers
  pmm.c         — Physical memory allocator (free-stack from E820)
  paging.c      — Virtual memory: map, unmap, clone page tables
  task.c        — Scheduler, IPC, syscalls, process lifecycle
  serial.c      — Kernel serial output (putc, puts, puthex)
  syscall.c     — Syscall dispatcher (routes to task.c handlers)
  gdt.c         — GDT + TSS for ring 3 transitions
  isr_stubs.asm — Assembly ISR wrappers, context save/restore

  include/      — Kernel headers
    io.h        — Types (u8/u16/u32/u64), inb/outb/inw/outw
    idt.h       — IDT entries, registers_t, PIC constants
    pmm.h       — Page allocator, E820 entry
    paging.h    — Page flags: PRESENT, WRITABLE, USER, PS
    task.h      — Task struct, max tasks, IPC queue, syscall prototypes
    syscall.h   — Syscall numbers (0-27)
    serial.h    — putc/puts/puthex declarations
    gdt.h       — TSS setup
    elf.h       — ELF64 loader structures

user/           — Init process
  init.c        — First process (PID 1), spawns programs from list
  init_procs.h  — Spawn list (edit to add programs)

_sdk/           — User-space SDK
  examples/     — Example programs (shell, fb_test, testchild)

build.sh        — Single script to build the entire image
linker.ld       — Kernel linker script (loads at 0x100000)
README.md       — This file
LICENSE         — MIT license
```

## Boot Process

1. **MBR** (`boot/boot.asm`, 512 bytes) is loaded by the BIOS at 0x7C00. It reads stage2 from LBA sector 1 via INT 0x13 with LBA extensions, then jumps to 0x7E00.

2. **Stage2** (`boot/stage2.asm`) sets up serial output, collects the E820 memory map via INT 0x15, loads the kernel from disk to 0x10000, prepares paging tables (PML4 → PDP → 512× 2 MB PDEs identity-mapping the first 1 GB), enters protected mode, then long mode, copies the kernel to 0x100000, and jumps to it.

3. **Kernel** (`kernel.c`) initializes the IDT (interrupt handlers), PIC (remaps IRQs to vectors 32-47), PIT (timer at 500 Hz), PMM (page allocator from E820), paging, GDT with TSS for ring 3 transitions, and the task system. It then loads init.elf (embedded in the kernel binary) as PID 1 and starts the scheduler.

4. **Init** (`init.c`, PID 1) prints a ready message and spawns any programs listed in `init_procs.h` using syscall 26 (spawn_at). If the list is empty, it loops with `pause`.

5. **User programs** run in ring 3 with their own page tables (cloned from the kernel identity map, with the USER flag set on user pages only). A page fault in user mode kills the process instead of crashing the system.

## Notes

- The timer runs at **500 Hz** (2 ms per tick). Nanosleep rounds up to the next tick.
- User programs must be **ELF64** binaries linked at **0x400000** with entry point **`_start`**.
- The stack is always 4 pages (16 KB), mapped at `load_address + 0x100000` (i.e. 0x4F0000 to 0x500000).
- The heap starts at 0x600000 and grows via `brk` in page increments.
- Syscall 6 (spawn) is not implemented. Use syscall 26 (spawn_at) for ELF64 binaries.
- There is no filesystem driver in v0.1a. Store data in the program binary or generate it with code.
- IPC is non-blocking on receive. Check the return value: 0 = message received, -1 = queue empty.
- **Embedding user programs:** The `_binary_*` symbols from `objcopy` must be visible to `init.elf` (user-space), not the kernel. Link `*_embed.o` into `init.elf`, not into `kernel.bin`. See the embedding section above for the exact `build.sh` changes.
- **Framebuffer bounds:** When animating on the framebuffer, always clamp the position before writing to it. A negative `y` that underflows will access memory before `0xFD00000` and cause a page fault.

## License

MIT
