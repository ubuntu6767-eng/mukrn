#!/usr/bin/env bash
set -e

echo "=== Building C kernel (x86-64) ==="
for SRC in kernel.c idt.c serial.c pmm.c paging.c; do
    gcc -m64 -ffreestanding -nostdlib -nostartfiles \
        -fno-PIE -fno-asynchronous-unwind-tables -fno-stack-protector \
        -fno-toplevel-reorder -mno-red-zone -mgeneral-regs-only \
        -c "$SRC" -o "${SRC%.c}.o"
done

echo "=== Assembling ISR stubs ==="
nasm -f elf64 -o isr_stubs.o isr_stubs.asm

ld -m elf_x86_64 -T linker.ld --oformat binary -o kernel.bin \
    kernel.o idt.o serial.o pmm.o paging.o isr_stubs.o

KERNEL_SIZE=$(stat -c%s kernel.bin)
KERNEL_SECTORS=$(( (KERNEL_SIZE + 511) / 512 ))
echo "Kernel: $KERNEL_SIZE bytes -> $KERNEL_SECTORS sectors"

truncate -s $((KERNEL_SECTORS * 512)) kernel.bin

KERNEL_LBA=5
echo "Kernel LBA: $KERNEL_LBA"

echo "=== Building stage2 ==="
nasm -f bin -o stage2.bin \
    -dKERNEL_LBA=$KERNEL_LBA \
    -dKERNEL_SECTORS=$KERNEL_SECTORS \
    stage2.asm

STAGE2_SIZE=$(stat -c%s stage2.bin)
if [ $STAGE2_SIZE -gt 2048 ]; then
    echo "ERROR: stage2 too big ($STAGE2_SIZE bytes, max 2048)"
    exit 1
fi
truncate -s 2048 stage2.bin
echo "Stage2: $STAGE2_SIZE bytes -> 4 sectors"

echo "=== Building MBR ==="
nasm -f bin -o boot.bin boot.asm

echo "=== Creating disk image ==="
cat boot.bin stage2.bin kernel.bin > os_image.bin
SIZE=$(stat -c%s os_image.bin)
PADDED=$(( (SIZE + 1048575) / 1048576 * 1048576 ))
truncate -s $PADDED os_image.bin
echo "Image: $SIZE bytes (padded to $PADDED)"

echo "=== Running QEMU ==="
qemu-system-x86_64 \
    -drive format=raw,file=os_image.bin,if=ide \
    -nographic \
    -no-reboot
