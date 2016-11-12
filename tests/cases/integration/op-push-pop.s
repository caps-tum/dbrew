        .intel_syntax noprefix
        .text
        .globl  f1
        .type   f1, @function
f1:
        push    0x5b
        push    -0xAAAAAA
        push    -0x10
        mov     rbx, 0xBBB
        push    bx
        push    0xCCC
        mov     rbx, 0xDDDDDDDDDDDD
        push    rbx
        pop     r9
        pop     rdx
        pop     cx
        pop     r10
        pop     rbx
        pop     rax
        add     rax, r9
        add     rax, rbx
        add     rax, r10
        add     rax, rcx
        add     rax, rdx
        ret
