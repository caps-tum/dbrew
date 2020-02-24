//!args=--var
    .text
    .globl  f1
    .type   f1, @function
f1:
    mov %rdi,%rax
    and $0,%rax
    mov %rdi,%rbx
    and $0xffffffffffffffff,%rbx
    add %rbx,%rax
    ret
