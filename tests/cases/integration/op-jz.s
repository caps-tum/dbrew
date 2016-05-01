.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    xor eax, eax
    test edi, edi
    jz 1f
    ret
1:
    inc eax
    ret
