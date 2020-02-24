//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // BSF instruction
    bsf rax, [rdi]
    bsf r9, [rdi]
    bsf eax, [rdi]
    bsf r9d, [rdi]
    bsf ax, [rdi]
    bsf r9w, [rdi]

    ret
