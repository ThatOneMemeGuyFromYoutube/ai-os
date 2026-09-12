.section .text.boot,"ax"
.align 4
/* Multiboot v1 header: keep this in the executable text section so it is
   guaranteed to be at the beginning of the kernel file. */
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)

.global _start
.type _start,@function
_start:
    cli
    mov $stack_top, %esp
    call kmain
1:
    hlt
    jmp 1b

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:
