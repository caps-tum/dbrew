.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    shl edi, 5
    mov rax, rdi
    ret
