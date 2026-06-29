org 0x7E00
bits 16

CODE32_SEG equ 0x08
DATA_SEG   equ 0x10
CODE64_SEG equ 0x18
USER_DATA  equ 0x20
USER_CODE  equ 0x28
TSS_SEG    equ 0x30

KERNEL_ADDR equ 0x100000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7E00
    mov [boot_drive], dl

    call serial_init

    mov si, msg_start
    call serial_puts

    call check_cpuid
    mov si, msg_cpuid
    call serial_puts

    call check_long_mode
    mov si, msg_lm
    call serial_puts

    in al, 0x92
    or al, 2
    out 0x92, al
    mov si, msg_a20
    call serial_puts

    push es
    push ds
    pop es
    mov di, 0x4004
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150
.e820_loop:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .e820_done
    cmp eax, 0x534D4150
    jne .e820_done
    mov [di+20], dword 1
    add di, 24
    inc bp
    test ebx, ebx
    jnz .e820_loop
.e820_done:
    mov [0x4000], bp
    pop es
    mov si, msg_e820
    call serial_puts

    mov dl, [boot_drive]
    mov ah, 0x08
    int 0x13
    jc disk_error
    and cx, 0x3F
    mov [sectors], cx
    xor ax, ax
    mov al, dh
    inc ax
    mov [heads], ax

    mov ax, KERNEL_LBA
    xor dx, dx
    div word [sectors]
    inc dx
    mov [sec], dx
    xor dx, dx
    div word [heads]
    mov [cyl], ax
    mov [head], dx

    mov dl, [boot_drive]
    mov bx, 0x0000
    mov ax, 0x1000
    mov es, ax
    mov ah, 0x02
    mov al, KERNEL_SECTORS
    mov ch, byte [cyl]
    mov cl, byte [sec]
    mov dh, byte [head]
    int 0x13
    jc disk_error
    mov si, msg_kernel
    call serial_puts

    xor ax, ax
    mov es, ax

    call setup_paging
    mov si, msg_paging
    call serial_puts

    mov si, msg_pm
    call serial_puts

    lgdt [gdtr]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE32_SEG:pm_entry

disk_error:
    mov si, msg_disk_err
    call serial_puts
.h:
    hlt
    jmp .h

serial_init:
    push ax
    push dx
    mov dx, 0x3F9
    mov al, 0
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 1
    out dx, al
    mov dx, 0x3F9
    mov al, 0
    out dx, al
    mov dx, 0x3FB
    mov al, 3
    out dx, al
    mov dx, 0x3F8+2
    mov al, 0xC7
    out dx, al
    mov dx, 0x3F8+4
    mov al, 0x0B
    out dx, al
    pop dx
    pop ax
    ret

serial_putc:
    push ax
    push dx
    mov ah, al
    mov dx, 0x3FD
.w:
    in al, dx
    test al, 0x20
    jz .w
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    pop dx
    pop ax
    ret

serial_puts:
    push ax
    push si
.l:
    lodsb
    or al, al
    jz .d
    call serial_putc
    jmp .l
.d:
    pop si
    pop ax
    ret

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ecx
    shr eax, 21
    and ax, 1
    jz .no
    ret
.no:
    mov si, msg_no_cpuid
    call serial_puts
.h:
    hlt
    jmp .h

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no
    ret
.no:
    mov si, msg_no_lm
    call serial_puts
.h:
    hlt
    jmp .h

setup_paging:
    mov edi, 0x1000
    mov cr3, edi
    xor eax, eax
    mov ecx, 1024
    rep stosd
    mov ecx, 1024
    rep stosd
    mov ecx, 1024
    rep stosd
    mov dword [0x1000], 0x2007
    mov dword [0x1004], 0
    mov dword [0x2000], 0x3007
    mov dword [0x2004], 0
    mov edi, 0x3000
    mov eax, 0x000083
    mov ecx, 512
.fill:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .fill
    ret

msg_start:    db "[bootloader] Starting...", 13, 10, 0
msg_cpuid:    db "[bootloader] CPUID supported", 13, 10, 0
msg_lm:       db "[bootloader] Long mode supported", 13, 10, 0
msg_a20:      db "[bootloader] A20 gate enabled", 13, 10, 0
msg_e820:     db "[bootloader] E820 memory map", 13, 10, 0
msg_kernel:   db "[bootloader] Kernel loaded", 13, 10, 0
msg_paging:   db "[bootloader] Paging tables ready", 13, 10, 0
msg_pm:       db "[bootloader] Entering protected mode...", 13, 10, 0
msg_lm64:     db "[bootloader] Entered long mode", 13, 10, 0
msg_copy:     db "[bootloader] Copying kernel to 0x100000", 13, 10, 0
msg_jump:     db "[bootloader] Jumping to kernel", 13, 10, 0
msg_disk_err: db "[bootloader] DISK ERROR", 13, 10, 0

msg_no_cpuid: db "[bootloader] ERROR: CPUID not supported", 13, 10, 0
msg_no_lm:    db "[bootloader] ERROR: Long mode not supported", 13, 10, 0

align 16
boot_drive: db 0
sectors: dw 0
heads:   dw 0
cyl:     dw 0
head:    dw 0
sec:     dw 0

align 16
gdt:
    dq 0                          ; 0x00 null
    dq 0x00CF9A000000FFFF         ; 0x08 ring0 code32
    dq 0x00CF92000000FFFF         ; 0x10 ring0 data
    dq 0x00AF9A0000000000         ; 0x18 ring0 code64
    dq 0x00CFF2000000FFFF         ; 0x20 ring3 data
    dq 0x00AFFA0000000000         ; 0x28 ring3 code64
    dq 0, 0                       ; 0x30 TSS (set up by kernel)
gdt_end:

gdtr:
    dw gdt_end - gdt - 1
    dd gdt

bits 32
pm_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    mov al, 'P'
    call putc32

    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    jmp CODE64_SEG:lm_entry

putc32:
    push eax
    push edx
    mov ah, al
    mov dx, 0x3FD
.w:
    in al, dx
    test al, 0x20
    jz .w
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    pop edx
    pop eax
    ret

bits 64
lm_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, 0x90000

    mov rsi, msg_lm64
    call puts64

    mov rsi, msg_copy
    call puts64

    mov rsi, 0x10000
    mov rdi, 0x100000
    mov ecx, KERNEL_SECTORS * 512 / 4
    rep movsd

    mov rsi, msg_jump
    call puts64

    jmp KERNEL_ADDR

puts64:
    push rdx
    push rax
    push rsi
.l:
    lodsb
    or al, al
    jz .d
    push rax
    mov dx, 0x3FD
.w:
    in al, dx
    test al, 0x20
    jz .w
    mov dx, 0x3F8
    pop rax
    out dx, al
    jmp .l
.d:
    pop rsi
    pop rax
    pop rdx
    ret
