//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // LEA instructions, Intel Vol. 2A 3-480
    lea rax, [rdi]
    lea r9, [rdi]
    lea eax, [rdi]
    lea r9d, [rdi]
    lea ax, [rdi]
    lea r9w, [rdi]

    ret
