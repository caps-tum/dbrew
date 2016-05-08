.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    xor eax, eax
    add rax, rdi
    js 1f
    xor eax, eax
    ret
1:
    mov eax, 1
    ret
