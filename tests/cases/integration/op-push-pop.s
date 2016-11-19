        .intel_syntax noprefix
        .text
        .globl  f1
        .type   f1, @function
f1:
        push     0x5b
        push    -0xAAAAAA
        push    -0x10
        mov     rbx, 0xBBB
        push    bx
        .byte   0x66, 0x68, 0xCC, 0x0C # Manual assembly of pushw 0xCCC (couldn't get GNU as to generate it)
        movq    rbx, 0xDDDDDDDDDDDD # clang assembler needs q postfix
        push    rbx
        pop     r9
        pop     dx
        pop     cx
        pop     r10
        pop     rbx
        pop     rax
        add     rax, r9
        add     rax, rbx
        add     rax, r10
        add     rax, rcx
        add     rax, rdx # rax should be 0xdddddd334c05
        ret
