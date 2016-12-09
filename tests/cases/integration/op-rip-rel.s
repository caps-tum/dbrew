        .intel_syntax noprefix
        .text
        .global f1
        .type   f1, @function
f1:
        mov     rax, [rip + pos1] # Load rip-relative addr
        inc     rax
        mov     [rip + pos1], rax # Store to rip-relative addr
        lea     rax, f2
        mov     [rip + pos2], rax
        xor     rax, rax
        jmp     [rip + pos2] # Jump to rip-relative addr
        xor     rax, rax
        ret

        .type   f2, @function
f2:
        mov     rax, [rip + pos1]
        inc     rax
        mov     [rip + pos1], rax
        ret     # On return rax should be 0xaaaaaaaaaaaaaaac

        .data
        .type pos1, @object
pos1:
        .quad 0xAAAAAAAAAAAAAAAA
        .type pos2, @object
pos2:
        .quad   0
