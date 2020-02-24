//!driver = test-driver-decode.c
    .text
    .globl  f1
    .type   f1, @function
f1:
    mov %edi,%eax
    cltq
    mov %rax,%rdx
    mov %si,%ax
    cwtl
    add %rdx,%rax
    ret
