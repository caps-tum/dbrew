//!driver = test-driver-decode.c
//!cc = cc
// ^fragile test case: only gcc behavior as expected, force gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    js f1
