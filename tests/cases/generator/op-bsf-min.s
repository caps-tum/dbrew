        .intel_syntax noprefix
        .text
        .global f1
        .type   f1, @function
f1:
        mov     ecx, 0xfff
        bsf     eax, ecx
        inc     eax
        ret
