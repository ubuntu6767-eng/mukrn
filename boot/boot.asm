org 0x7C00
bits 16

STAGE2_SECTORS equ 4
STAGE2_LBA    equ 1

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [boot_drive], dl

    mov si, dap
    mov ah, 0x42
    int 0x13
    jc disk_error

    jmp 0x0000:0x7E00

disk_error:
    mov si, msg_err
.l:
    lodsb
    or al, al
    jz .h
    mov ah, 0x0E
    int 0x10
    jmp .l
.h:
    hlt
    jmp .h

msg_err: db "Disk error", 0

dap:
    db 0x10
    db 0
    dw STAGE2_SECTORS
    dw 0x7E00
    dw 0x0000
    dq STAGE2_LBA

boot_drive: db 0

times 510 - ($ - $$) db 0
dw 0xAA55
