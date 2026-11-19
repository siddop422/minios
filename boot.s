.section .multiboot2
.balign 8
mb2_header_start:
    .long 0xE85250D6
    .long 0
    .long mb2_header_end - mb2_header_start
    .long -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start))
    .short 0
    .short 0
    .long 8
mb2_header_end:

.section .text
.code32
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli
    mov $stack_top, %esp
    xor %ebp, %ebp
    push %ebx
    push %eax
    call kernel_main
1:
    cli
    hlt
    jmp 1b

.section .bss
.balign 16
stack_bottom:
    .skip 16384
stack_top:
