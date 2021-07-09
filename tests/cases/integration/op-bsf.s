        .intel_syntax noprefix
        .text
        .global f1
        .type   f1, @function
f1:
        xor     rax, rax
        mov     ecx, 0x1ffff
        bsf     ebx, ecx
        or      rax, rbx
        mov     rcx, 0xffffffff
        bsf     rbx, rcx
        or      rax, rbx
        xor     rcx, rcx        # rcx = 0
        bsf     rbx, rcx        # Shoud set ZF = 1
        jz      f2
f1_cont:
        bsf     rbx, rax        # Should set ZF = 0
        jnz     f3              # Jump if ZF was set correctly
        xor     rax, rax        # We should never get here
        ret
        .type   f2, @function
f2:
        inc     rax
        jmp     f1_cont

        .type f3, @function
f3:
        inc     rax
        bsf     rbx, [val]
        or      rax, rbx
        ret

        .type val, @object
val:
        .quad   0xfffffffffff
