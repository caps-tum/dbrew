//!driver = test-driver-decode.c
//!cc = gcc
// ^fragile test case: only gcc behavior as expected, force gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    js short f1
