//!driver = test-driver-decode.c
//!cc = cc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // LEAVE instructions, Intel Vol. 2A 3-483
    leave
    leavew

    ret
