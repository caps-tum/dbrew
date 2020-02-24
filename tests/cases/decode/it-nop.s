//!driver = test-driver-decode.c
//!cc = gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // NOP instructions, Intel Vol. 2B 4-9
    nop
    nop di
    nop edi

    ret
