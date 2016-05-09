//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    movq rax, xmm0
    movq rax, xmm1
    movq rcx, xmm0
    movq rcx, xmm1
    ret
