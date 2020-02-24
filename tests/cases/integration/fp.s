.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    xor rax, rax
    movq xmm0, rdi
    movq xmm1, rsi
    addsd xmm0, xmm1
    movq rax, xmm0
    ret
