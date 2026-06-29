OUTPUT_FORMAT(binary)
SECTIONS
{
    . = 0x100000;
    .entry : { *(.entry) }
    .text  : { *(.text) }
    .data  : { *(.data) *(.rodata*) }
    .bss  : { *(.bss) }
}
