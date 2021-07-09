//!driver = test-driver-gen.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // BSR instruction
    bsr rax, [rdi]
    bsr r9, [rdi]
    bsr eax, [rdi]
    bsr r9d, [rdi]
    bsr ax, [rdi]
    bsr r9w, [rdi]
    bsr rax, rbx
    bsr eax, ebx
    bsr ax, bx

    ret
