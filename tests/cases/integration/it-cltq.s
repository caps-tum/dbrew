//!args=--var
    .text
    .globl  f1
    .type   f1, @function
f1:
    mov %edi,%eax
    cltq
    mov %rax,%rdx
    mov %esi,%eax
    cwtl
    add %rdx,%rax
    ret
