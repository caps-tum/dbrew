.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    movabs rax, 0x7fffffffffffffff
    add rax, rdi
    jo 1f
    xor eax, eax
    ret
1:
    mov eax, 1
    ret
