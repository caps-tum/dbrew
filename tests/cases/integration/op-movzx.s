        .intel_syntax noprefix
        .text
        .global f1
        .type f1, @function
f1:
        // Test 8-bit zero extends
        mov     al, -1
        movzx   bx, al
        movzx   ecx, al
        movzx   rdx, al
        and     rdx, rbx
        and     rdx, rcx
        cmp     rdx,0xFF
        jne     fail
        // Test 16-bit zero extends
        mov     ax, 0xFF00
        movzx   ebx, ax
        movzx   rcx, ax
        and     rcx, rbx
        cmp     rcx, 0xFF00
        jne     fail
        // Or the two zero extends
        xor     rax, rax
        or      rax, rcx
        or      rax, rdx
        ret
fail:
        xor rax, rax
        ret
