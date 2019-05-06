//!driver = test-driver-gen.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
        // TODO: Add tests for more encodings. This requires the test driver to
        // be able to handle generating code for more than a single BB
        call rax
    ret
