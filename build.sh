#!/usr/bin/env bash
set -e

mkdir -p build

echo "=== Building C kernel ==="
CFLAGS="-m64 -ffreestanding -nostdlib -nostartfiles \
    -fno-PIE -fno-asynchronous-unwind-tables -fno-stack-protector \
    -fno-toplevel-reorder -mno-red-zone -mgeneral-regs-only \
    -Ikernel/include"

for SRC in kernel.c idt.c serial.c pmm.c paging.c task.c gdt.c syscall.c; do
    gcc $CFLAGS -c "kernel/$SRC" -o "build/${SRC%.c}.o"
done

echo "=== Assembling ISR stubs ==="
nasm -f elf64 -o build/isr_stubs.o kernel/isr_stubs.asm


echo "=== Building init ==="
gcc $CFLAGS -c user/init.c -o build/init_user.o
ld -m elf_x86_64 -Ttext=0x400000 -o build/init.elf build/init_user.o

echo "=== Embedding init ==="
cd build
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    init.elf init_embed.o
cd - > /dev/null

ld -m elf_x86_64 -T linker.ld --oformat binary -o build/kernel.bin \
    build/kernel.o build/idt.o build/serial.o build/pmm.o build/paging.o \
    build/task.o build/gdt.o build/syscall.o \
    build/isr_stubs.o build/init_embed.o

KERNEL_SIZE=$(stat -c%s build/kernel.bin)
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))
echo "Kernel: $KERNEL_SIZE bytes -> $KERNEL_SECTORS sectors"

truncate -s $((KERNEL_SECTORS * 512)) build/kernel.bin

KERNEL_LBA=5

echo "=== Building stage2 ==="
nasm -f bin -o build/stage2.bin \
    -dKERNEL_LBA=$KERNEL_LBA \
    -dKERNEL_SECTORS=$KERNEL_SECTORS \
    boot/stage2.asm

STAGE2_SIZE=$(stat -c%s build/stage2.bin)
if [ $STAGE2_SIZE -gt 2048 ]; then
    echo "ERROR: stage2 too big ($STAGE2_SIZE bytes, max 2048)"
    exit 1
fi
truncate -s 2048 build/stage2.bin
echo "Stage2: $STAGE2_SIZE bytes -> 4 sectors"

echo "=== Building MBR ==="
nasm -f bin -o build/boot.bin boot/boot.asm

echo "=== Creating disk image ==="
cat build/boot.bin build/stage2.bin build/kernel.bin > build/os_image.bin
SIZE=$(stat -c%s build/os_image.bin)
PADDED=$(( (SIZE + 1048575) / 1048576 * 1048576 ))
truncate -s $PADDED build/os_image.bin
echo "Image: $SIZE bytes (padded to $PADDED)"

